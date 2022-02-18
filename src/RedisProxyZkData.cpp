/*************************************************************************
    > File Name: RedisProxyZkData.cpp
    > Author: annulsluo
    > Mail: annulsluo@gmail.com
    > Created Time: 一  2/17 15:27:58 2020
 ************************************************************************/
#include "RedisProxyZkData.h"
#include "util/tc_json.h"
#include "util/myboost_log.h"
#include "util/tc_thread_rwlock.h"
#include "RedisProxyAppFlowControl.h"
#include "RedisProxyKafka.h"
#include "RedisProxyFailOver.h"
#include "RedisProxyRegister.h"
#include "RedisProxySlotAction.h"
#include "RedisProxySlot.h"
#include <algorithm>
using namespace aiad;

void RedisProxyZkData::Init()
{
	// 封装 zk 配置类，用于执行以下操作
	// 1. 获取本地文件配置信息(host/timeout)
	// 2. 初始化 zk
	// 3. 获取存储在 zk 中的信息(Appid信息/Consul信息/redisconnpool信息/slot的状态信息)，并且加载到对应的内存中
	// Key 为节点路径，Value 为 Json 结构序列化的信息数据
	// 4. 设置 watch 函数，并且设置监听事件；当监听事件触发回调的时候进行加锁并且把数据更新到内存中
	// 5. 开发一个工具或者用使用 nacos 配置页面用于设置 ZK 中的信息, 使用py工具写入
    // 6. TODO 对于缩容，删除节点是否存在问题？
	string sZooHost		= ServerConfigMng::getInstance()->GetSObj( "zookeeper_host" );
	int	nZooTimeout		= ServerConfigMng::getInstance()->GetIObj( "zookeeper_timeout" );
	string sEnv			= ServerConfigMng::getInstance()->GetSObj( "env" );
	if( !sEnv.empty() )
	{
		_sRootPath	= ServerConfigMng::getInstance()->GetSObj( "rootpath" ) + "/" + sEnv;
		// Index 处理环境变量问题
		_nDiffIndex = 1;
	}
	else
	{
		_sRootPath	= ServerConfigMng::getInstance()->GetSObj( "rootpath" );
		_nDiffIndex = 0;
	}

	if( !_oZkProcess.zkConnect( sZooHost, (uint32_t)nZooTimeout, _sRootPath, NULL, NULL, NULL ) )
	{
		MYBOOST_LOG_ERROR( "RedisProxyZkData Init Error." );
	}
	else 
	{
		InitAppInfo();
		InitConsulInfo();
		InitKafkaInfo();
		InitRedisAppTableList();
        MYBOOST_LOG_INFO( "RedisProxyZkData Init Succ." );
	}
}

void RedisProxyZkData::InitGuard()
{
    // 如何对ProxyGuard的ZK配置进行感知？
    //      当时考虑的是对所有配置进行一次性加载，所以需要串行执行
    //      需要实现首次串行，后续进行watch
	string sZooHost = ServerConfigMng::getInstance()->GetSObj( "zookeeper_host" );
	int	nZooTimeout	= ServerConfigMng::getInstance()->GetIObj( "zookeeper_timeout" );
	string sEnv		= ServerConfigMng::getInstance()->GetSObj( "env" );
	if( !sEnv.empty() )
	{
		_sRootPath	= ServerConfigMng::getInstance()->GetSObj( "rootpath" ) + "/" + sEnv;
		_nDiffIndex = 1;
	}
	else
	{
		_sRootPath	= ServerConfigMng::getInstance()->GetSObj( "rootpath" );
		_nDiffIndex = 0;
	}
	if( !_oZkProcess.zkConnect( sZooHost, (uint32_t)nZooTimeout, _sRootPath, NULL, NULL, NULL ) )
	{
		MYBOOST_LOG_ERROR( "RedisProxyZkData Init Error." );
	}
	else 
	{
		InitKafkaInfoByGuard();
		InitRedisAppTableListByGuard();
        MYBOOST_LOG_INFO( "RedisProxyZkData InitGuard Succ." );
	}
}

const ConsulInfo & RedisProxyZkData::GetConsulInfo()
{
	return _oConsulInfo;
}

const GroupInfo & RedisProxyZkData::GetGroupInfoByAppTableGid( 
        const std::string & sAppTable, 
        const int nGroupId )
{
	MapStr2GIdGroupInfo::iterator msggi = _mapAppTable2GidGroupInfo.find( sAppTable );
	if( msggi != _mapAppTable2GidGroupInfo.end() )
	{
		map< int, RedisProxyCommon::GroupInfo >::iterator migi = msggi->second.find( nGroupId ); 
		if( migi != msggi->second.end() )
		{
			return migi->second;
		}
	}
    return _oEmptyGroupInfo;
}

const MapStr2GIdGroupInfo & RedisProxyZkData::GetGIdGroupInfo()
{
    return _mapAppTable2GidGroupInfo;
}

const MapStr2FailOverInfo & RedisProxyZkData::GetHost2FailOverInfos()
{
    return _mapHost2FailOverInfo;
}

KafkaInfo RedisProxyZkData::GetKafkaInfo()
{
	return _oKafkaInfo;
}

void RedisProxyZkData::InitAppInfo()
{
	string sAppInfoPath	= _sRootPath + "/AppInfo";
	MYBOOST_LOG_DEBUG( OS_KV("AppInfoPath", sAppInfoPath ) );
	_oZkProcess.watchData( sAppInfoPath, RedisProxyZkData::AppInfoCallback, this );
}

void RedisProxyZkData::ConsulInfoCallback( 
		const std::string & sPath, const std::string & sValue,
		void * pCtx, const struct Stat * pStat )
{
	ConsulInfo oConsulInfo;
    MYBOOST_LOG_INFO( OS_KV( "consulinfo", sValue ) );
    RedisProxyZkData * pZk = static_cast<RedisProxyZkData *>(pCtx);
	if( DecodeConsuleInfo( sValue, oConsulInfo ) == AICommon::RET_SUCC )
	{
		taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( pZk->_olock );
		pZk->_oConsulInfo = oConsulInfo;
		MYBOOST_LOG_INFO( "Decode ConsulInfo." << OS_KV( "iplist", pZk->_oConsulInfo.sConsulIpList ) 
				<< OS_KV( "port", pZk->_oConsulInfo.nConsulPort ) 
				<< OS_KV( "api", pZk->_oConsulInfo.sConsulApi ) );
		RedisProxyRegister::getInstance()->InitRegister( pZk->_oConsulInfo, "", "" );
	}
}

int RedisProxyZkData::DecodeConsuleInfo( const std::string & sConsulInfo, ConsulInfo & oConsulInfo )
{
	// 通过JSon解析ConsulInfo
	try
	{
		JsonValueObjPtr basePtr = taf::JsonValueObjPtr::dynamicCast(TC_Json::getValue(sConsulInfo));
		if( !basePtr )
		{
			MYBOOST_LOG_ERROR( "Decode ConsulInfo Error." );
			return AICommon::RET_FAIL;
		}

		JsonValueNumPtr consulregisterratioPtr	
					= taf::JsonValueNumPtr::dynamicCast(basePtr->value["consul_registerratio"]);
		if(consulregisterratioPtr){	oConsulInfo.nConsulRegisterRatio = consulregisterratioPtr->value; }

		JsonValueStringPtr consulprotoPtr   
			= taf::JsonValueStringPtr::dynamicCast(basePtr->value["consul_proto"]);
		if(consulprotoPtr){ oConsulInfo.sConsulProto    = consulprotoPtr->value;  }

		JsonValueStringPtr consuliplistPtr   
			= taf::JsonValueStringPtr::dynamicCast(basePtr->value["consul_iplist"]);
		if(consuliplistPtr){ oConsulInfo.sConsulIpList = consuliplistPtr->value;  }

		JsonValueNumPtr consulportPtr   
					= taf::JsonValueNumPtr::dynamicCast(basePtr->value["consul_port"]);
		if(consulportPtr){ oConsulInfo.nConsulPort = consulportPtr->value; }

		JsonValueNumPtr consulreportintevalPtr   
					= taf::JsonValueNumPtr::dynamicCast(basePtr->value["consul_report_interval"]);
		if(consulreportintevalPtr){ oConsulInfo.nConsulReportInterVal = consulreportintevalPtr->value; }

		JsonValueNumPtr consulreporttimeoutPtr   
					= taf::JsonValueNumPtr::dynamicCast(basePtr->value["consul_report_timeout"]);
		if(consulreporttimeoutPtr){ oConsulInfo.nConsulReportTimeout = consulreporttimeoutPtr->value; }

		JsonValueStringPtr consultokenPtr   
					= taf::JsonValueStringPtr::dynamicCast(basePtr->value["consul_token"]);
		if(consultokenPtr){ oConsulInfo.sConsulToken = consultokenPtr->value; }
		
		JsonValueStringPtr consulapiPtr   
					= taf::JsonValueStringPtr::dynamicCast(basePtr->value["consul_api"]);
		if(consulapiPtr){ oConsulInfo.sConsulApi = consulapiPtr->value; }

		JsonValueStringPtr consulservicenamePtr   
					= taf::JsonValueStringPtr::dynamicCast(basePtr->value["consul_servicename"]);
		if(consulservicenamePtr){ oConsulInfo.sConsulServiceName = consulservicenamePtr->value; }

	}
	catch (std::exception &e)
	{
		MYBOOST_LOG_ERROR( "Decode ConsulInfo Fail." << OS_KV( "msg", e.what() ) );
		return AICommon::RET_FAIL;
	}
	return AICommon::RET_SUCC;
}

const RedisProxyCommon::AppInfo & RedisProxyZkData::GetAppInfoByAppId( 
		const std::string & sAppId )
{
	MapStr2AppInfo::iterator mit;
	if( ( mit = _mapAppid2AppInfo.find( sAppId ) ) != _mapAppid2AppInfo.end() )
	{
		return mit->second;
	}
	return _oEmptyAppInfo;
}

const RedisConf & RedisProxyZkData::GetRedisConfByAppTable( const std::string & sAppTable )
{
	MapStr2RedisConf::iterator mIt;
	if( ( mIt = _mapAppTable2RedisConf.find( sAppTable ) ) != _mapAppTable2RedisConf.end() )
	{
		return mIt->second;
	}
	return _oEmptyRedisConf;
}

const RedisConnPoolConf & RedisProxyZkData::GetRedisConnPoolConfByAppTable( const std::string & sAppTable )
{
	MapStr2RedisConnPoolConf::iterator mIt;
	if( ( mIt = _mapAppTable2RedisConnPoolConf.find( sAppTable ) ) != _mapAppTable2RedisConnPoolConf.end() )
	{
		return mIt->second;
	}
	return _oEmptyRedisConnPoolConf;
}

void RedisProxyZkData::AppInfoCallback( 
		const std::string & sPath, const std::string & sValue,
		void * pCtx, const struct Stat * pStat )
{
	MYBOOST_LOG_INFO( OS_KV( "AppInfoCallback", sValue ) );
	map< std::string, RedisProxyCommon::AppInfo > mapAppid2AppInfo;
    RedisProxyZkData * pZk = static_cast<RedisProxyZkData *>(pCtx);
	if( DecodeAppInfo( sValue, mapAppid2AppInfo ) == AICommon::RET_SUCC )
	{
		taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( pZk->_olock );
		pZk->_mapAppid2AppInfo.swap( mapAppid2AppInfo );

		// 更新数据到流控中
		RedisProxyAppFlowControl::getInstance()->Init(pZk->_mapAppid2AppInfo);
	}
}

int RedisProxyZkData::DecodeAppInfo( const std::string & sAppInfo,
		map< std::string, RedisProxyCommon::AppInfo > & mapAppid2AppInfo )
{
	// 通过JSon解析AppInfo
	try
	{
		JsonValueObjPtr basePtr = taf::JsonValueObjPtr::dynamicCast(TC_Json::getValue(sAppInfo));
		if( !basePtr )
		{
			MYBOOST_LOG_ERROR( "Decode AppInfo Error." );
			return AICommon::RET_FAIL;
		}
		JsonValueArrayPtr AppInfoListPtr = taf::JsonValueArrayPtr::dynamicCast( basePtr->value["AppInfoList"] );
		if(!AppInfoListPtr)
		{
			MYBOOST_LOG_ERROR( "Decode AppInfoList Null." );
			return AICommon::RET_FAIL;
		}
		for( size_t i = 0; i < size_t(AppInfoListPtr->value.size()); ++ i )
		{
			RedisProxyCommon::AppInfo oAppInfo;
            JsonValueObjPtr AppInfoPtr     = taf::JsonValueObjPtr::dynamicCast(AppInfoListPtr->value[i]);

            JsonValueStringPtr appidPtr   = taf::JsonValueStringPtr::dynamicCast(AppInfoPtr->value["appid"]);
            if(appidPtr){ oAppInfo.set_sappid( appidPtr->value ); }
            JsonValueStringPtr apppwPtr   = taf::JsonValueStringPtr::dynamicCast(AppInfoPtr->value["apppw"]);
            if(apppwPtr){ oAppInfo.set_sapppw( apppwPtr->value ); }
            JsonValueStringPtr redisidPtr   = taf::JsonValueStringPtr::dynamicCast(AppInfoPtr->value["redisid"]);
            if(redisidPtr){ oAppInfo.set_sredisid( redisidPtr->value ); }
            JsonValueStringPtr redispwPtr   = taf::JsonValueStringPtr::dynamicCast(AppInfoPtr->value["redispw"]);
            if(redispwPtr){ oAppInfo.set_sredispw( redispwPtr->value ); }
            JsonValueStringPtr appnamePtr   = taf::JsonValueStringPtr::dynamicCast(AppInfoPtr->value["appname"]);
            if(appnamePtr){ oAppInfo.set_sappname( appnamePtr->value ); }
            JsonValueStringPtr tablenamePtr   = taf::JsonValueStringPtr::dynamicCast(AppInfoPtr->value["tablename"]);
            if(tablenamePtr){ oAppInfo.set_stablename( tablenamePtr->value ); }
            JsonValueNumPtr flowcontrolmaxreqPtr   = taf::JsonValueNumPtr::dynamicCast(AppInfoPtr->value["flowcontrolmaxreq"]);
            if(flowcontrolmaxreqPtr){ oAppInfo.set_nflowcontrolmaxreq( flowcontrolmaxreqPtr->value ); }
            JsonValueNumPtr flowcontrolintervalmsPtr   = taf::JsonValueNumPtr::dynamicCast(AppInfoPtr->value["flowcontrolintervalms"]);
            if(flowcontrolintervalmsPtr){ oAppInfo.set_nflowcontrolintervalms( flowcontrolintervalmsPtr->value ); }
            JsonValueStringPtr lmdbidPtr   = taf::JsonValueStringPtr::dynamicCast(AppInfoPtr->value["lmdbid"]);
            if(lmdbidPtr){ oAppInfo.set_slmdbid( lmdbidPtr->value ); }
            JsonValueStringPtr lmdbpwPtr   = taf::JsonValueStringPtr::dynamicCast(AppInfoPtr->value["lmdbpw"]);
            if(lmdbpwPtr){ oAppInfo.set_slmdbpw( lmdbpwPtr->value ); }
			mapAppid2AppInfo.insert( std::pair< std::string, RedisProxyCommon::AppInfo >( appidPtr->value, oAppInfo ) );
			MYBOOST_LOG_INFO( "Decode AppInfo." << oAppInfo.DebugString() );
		}
	}
	catch (std::exception &e)
	{
		MYBOOST_LOG_ERROR( "Decode AppInfoList Fail." << OS_KV( "msg", e.what() ) );
		return AICommon::RET_FAIL;
	}
	return AICommon::RET_SUCC;
}

void RedisProxyZkData::InitConsulInfo()
{
	string sConsulPath = _sRootPath + "/ConsulInfo";
	_oZkProcess.watchData( sConsulPath, RedisProxyZkData::ConsulInfoCallback, this );
}

void RedisProxyZkData::KafkaInfoCallback( 
		const std::string & sPath, const std::string & sValue,
		void * pCtx, const struct Stat * pStat )
{
	KafkaInfo oKafkaInfo;
    RedisProxyZkData * pZk = static_cast<RedisProxyZkData *>(pCtx);
	if( DecodeKafkaInfo( sValue, oKafkaInfo ) == AICommon::RET_SUCC )
	{
		taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( pZk->_olock );
		pZk->_oKafkaInfo = oKafkaInfo;
		RedisProxyKafka::getInstance()->InitProduce( pZk->_oKafkaInfo );
		MYBOOST_LOG_INFO( "Decode KafkaInfo." << OS_KV( "bokers", pZk->_oKafkaInfo.sBrokers ) );
	}
}

int RedisProxyZkData::DecodeKafkaInfo( const std::string & sKafkaInfo, KafkaInfo & oKafkaInfo )
{
	// 通过JSon解析KafkaInfo
	try
	{
		JsonValueObjPtr basePtr = taf::JsonValueObjPtr::dynamicCast(TC_Json::getValue(sKafkaInfo));
		if( !basePtr )
		{
			MYBOOST_LOG_ERROR( "Decode KafkaInfo Error." );
			return AICommon::RET_FAIL;
		}
		JsonValueStringPtr brokersPtr   = taf::JsonValueStringPtr::dynamicCast(basePtr->value["brokers"]);
		if(brokersPtr){ oKafkaInfo.sBrokers			= brokersPtr->value;}

		JsonValueStringPtr topicsPtr   
					= taf::JsonValueStringPtr::dynamicCast(basePtr->value["topics"]);
		if(topicsPtr){ oKafkaInfo.sTopics			= topicsPtr->value; }
		
		JsonValueStringPtr partitionsPtr   
					= taf::JsonValueStringPtr::dynamicCast(basePtr->value["partitions"]);
		if(partitionsPtr){ oKafkaInfo.sPartitions	= partitionsPtr->value; }

	   JsonValueNumPtr lmdbpartitionnumPtr
            = taf::JsonValueNumPtr::dynamicCast(basePtr->value["lmdbpartitionnum"]);
        if(lmdbpartitionnumPtr){ oKafkaInfo.nLmdbPartition = lmdbpartitionnumPtr->value;}

		JsonValueStringPtr IsKeepRunPtr   
					= taf::JsonValueStringPtr::dynamicCast(basePtr->value["IsKeepRun"]);
		if( IsKeepRunPtr )
		{
			if( IsKeepRunPtr->value == "true" ) oKafkaInfo.bIsKeepRun = true;
			else oKafkaInfo.bIsKeepRun = false;
		}
	}
	catch (std::exception &e)
	{
		MYBOOST_LOG_ERROR( "Decode KafkaInfo Fail." << OS_KV( "msg", e.what() ) );
		return AICommon::RET_FAIL;
	}
	return AICommon::RET_SUCC;
}

void RedisProxyZkData::InitKafkaInfoByGuard()
{
	string sKafKaPath   = _sRootPath + "/KafkaInfo";
    string sValue       = "";
	_oZkProcess.getData( sKafKaPath, sValue );
	if( DecodeKafkaInfo( sValue, _oKafkaInfo ) == AICommon::RET_SUCC )
	{
		RedisProxyKafka::getInstance()->InitConsumer( _oKafkaInfo );
		MYBOOST_LOG_INFO( "Decode KafkaInfo." << OS_KV( "bokers", _oKafkaInfo.sBrokers ) );
	}
}
void RedisProxyZkData::InitKafkaInfo()
{
	string sKafKaPath   = _sRootPath + "/KafkaInfo";
	_oZkProcess.watchData( sKafKaPath, RedisProxyZkData::KafkaInfoCallback, this );
}

int RedisProxyZkData::DecodeRedisConf( 
		const std::string & sRedisConf, 
		RedisConf & oRedisConf )
{
	// 通过JSon解析 RedisConf
	try
	{
		JsonValueObjPtr basePtr = taf::JsonValueObjPtr::dynamicCast(TC_Json::getValue(sRedisConf));
		if( !basePtr )
		{
			MYBOOST_LOG_ERROR( "Decode RedisConf Error." );
			return AICommon::RET_FAIL;
		}
		JsonValueNumPtr maxtrytimePtr   = taf::JsonValueNumPtr::dynamicCast(basePtr->value["connectmaxtrytimes"]);
		if(maxtrytimePtr){ oRedisConf._nConnectMaxTryTimes = maxtrytimePtr->value;}

		JsonValueNumPtr timeoutsecPtr   = taf::JsonValueNumPtr::dynamicCast(basePtr->value["timeoutsec"]);
		if(timeoutsecPtr){ oRedisConf._nTimeout.tv_sec = timeoutsecPtr->value;}

		JsonValueNumPtr timeoutusecPtr   = taf::JsonValueNumPtr::dynamicCast(basePtr->value["timeoutusec"]);
		if(timeoutusecPtr){ oRedisConf._nTimeout.tv_usec = timeoutusecPtr->value;}

		JsonValueStringPtr passwdPtr   
					= taf::JsonValueStringPtr::dynamicCast(basePtr->value["passwd"]);
		if(passwdPtr){ oRedisConf._sPasswd = passwdPtr->value; }
	}
	catch (std::exception &e)
	{
		MYBOOST_LOG_ERROR( "Decode RedisConf Fail." << OS_KV( "msg", e.what() ) );
		return AICommon::RET_FAIL;
	}
	return AICommon::RET_SUCC;
}

void RedisProxyZkData::RedisConfCallback( 
		const std::string & sPath, const std::string & sValue,
		void * pCtx, const struct Stat * pStat )
{
	RedisConf oRedisConf;
	vector< string > vecRedisConfInfo	= TC_Common::sepstr< string >( sPath, "/" );
	string sAppTable					= "";
	RedisProxyZkData * pZk = static_cast<RedisProxyZkData *>(pCtx);
	if( vecRedisConfInfo.size() > size_t( REDISCONFINFO_APPTABLE_INDEX + pZk->_nDiffIndex ) ) 
	{
		string sTmpAppTable		= vecRedisConfInfo[REDISCONFINFO_APPTABLE_INDEX + pZk->_nDiffIndex];
		string sApp				= TC_Common::sepstr<string>( sTmpAppTable, "_" )[0];
		string sTable			= TC_Common::sepstr<string>( sTmpAppTable, "_" )[1];
		sAppTable				= sApp + "." + sTable;
	}

	if( DecodeRedisConf( sValue, oRedisConf ) == AICommon::RET_SUCC )
	{
		taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( pZk->_olock );
		MapStr2RedisConf::iterator mSt = pZk->_mapAppTable2RedisConf.find( sAppTable );
		if( mSt == pZk->_mapAppTable2RedisConf.end() )
		{
			pZk->_mapAppTable2RedisConf.insert( std::pair< string, RedisConf >( sAppTable, oRedisConf ) );
			MYBOOST_LOG_INFO( "Insert RedisConf Succ." << OS_KV( "apptable", sAppTable ) 
					<< OS_KV( "size", pZk->_mapAppTable2RedisConf.size() ) ); 
		}
		else
		{
			mSt->second = oRedisConf;
			MYBOOST_LOG_INFO( "Update RedisConf Succ." << OS_KV( "apptable", sAppTable ) 
					<< OS_KV( "size", pZk->_mapAppTable2RedisConf.size() ) ); 
		}
	}
}

const MigrateConf & RedisProxyZkData::GetMigrateConf( const std::string & sAppTable )
{
    MapStr2MigrateConf::iterator ms2mc = _mapAppTable2MigrateConf.find( sAppTable );
    if( ms2mc != _mapAppTable2MigrateConf.end() )
    {
        return ms2mc->second;
    }
    return _oEmptyMigrateConf;
}

int RedisProxyZkData::DecodeMigrateConf( const std::string & sMigrateConf, MigrateConf & oMigrateConf )
{
	// 通过JSon解析MigrateConf
	try
	{
		JsonValueObjPtr basePtr = taf::JsonValueObjPtr::dynamicCast(TC_Json::getValue(sMigrateConf));
		if( !basePtr )
		{
			MYBOOST_LOG_ERROR( "Decode MigrateConf Error." );
			return AICommon::RET_FAIL;
		}
		JsonValueNumPtr scancountPtr = taf::JsonValueNumPtr::dynamicCast(basePtr->value["scan_count"]);
		if( scancountPtr ){ oMigrateConf.nScanCount = scancountPtr->value; }
		JsonValueNumPtr batchmigratenumPtr 
                = taf::JsonValueNumPtr::dynamicCast(basePtr->value["batch_migrate_num"]);
		if( batchmigratenumPtr ){ oMigrateConf.nBatchMigrateNum = batchmigratenumPtr->value; }
		JsonValueNumPtr migratetimeoutPtr 
                = taf::JsonValueNumPtr::dynamicCast(basePtr->value["migrate_timeout"]);
		if( migratetimeoutPtr ){ oMigrateConf.nMigrateTimeout = migratetimeoutPtr->value; }
		JsonValueNumPtr migratesleeptimePtr 
                = taf::JsonValueNumPtr::dynamicCast(basePtr->value["migrate_sleep_time"]);
		if( migratesleeptimePtr ){ oMigrateConf.nMigrateSleepTime = migratesleeptimePtr->value; }

		JsonValueNumPtr maxscantimesPtr 
                = taf::JsonValueNumPtr::dynamicCast(basePtr->value["maxscantimes"]);
		if( maxscantimesPtr ){ oMigrateConf.nMaxScanTimes = maxscantimesPtr->value; }

        JsonValueStringPtr scanpatternPtr 
                = taf::JsonValueStringPtr::dynamicCast(basePtr->value["scan_pattern"]);
        if( scanpatternPtr ){ oMigrateConf.sScanPattern = scanpatternPtr->value; }

	}
	catch( std::exception & e )
	{
		MYBOOST_LOG_ERROR( "Decode MigrateConf Fail." << OS_KV( "msg", e.what() ) );
		return AICommon::RET_FAIL;
	}
	return AICommon::RET_SUCC;
}

void RedisProxyZkData::InitRedisMigrateConfByGuard( const std::string & sAppTable )
{
	string sMigrateConfPath	= _sRootPath + "/RedisAppTable/" + sAppTable + "/migrateconf";
    string sValue           = "";
	_oZkProcess.getData( sMigrateConfPath, sValue );
    MigrateConf oMigrateConf;
    
	if( DecodeMigrateConf( sValue, oMigrateConf ) == AICommon::RET_SUCC )
	{
		string sApp				= TC_Common::sepstr<string>( sAppTable, "_" )[0];
		string sTable			= TC_Common::sepstr<string>( sAppTable, "_" )[1];
		string sCommaAppTable   = sApp + "." + sTable;
		MapStr2MigrateConf::iterator mSt = _mapAppTable2MigrateConf.find( sCommaAppTable );
		if( mSt == _mapAppTable2MigrateConf.end() )
		{
			_mapAppTable2MigrateConf.insert( std::pair< string, MigrateConf >( sCommaAppTable, oMigrateConf ) );
			MYBOOST_LOG_INFO( "Insert MigrateConf Succ." << OS_KV( "apptable", sCommaAppTable ) 
					<< OS_KV( "size", _mapAppTable2MigrateConf.size() ) ); 
		}
		else
		{
			mSt->second = oMigrateConf;
			MYBOOST_LOG_INFO( "Update MigrateConf Succ." << OS_KV( "apptable", sAppTable ) 
					<< OS_KV( "size", _mapAppTable2MigrateConf.size() ) ); 
		}
	}
}

void RedisProxyZkData::InitRedisConfByGuard( const std::string & sAppTable )
{
	string sRedisConfPath	= _sRootPath + "/RedisAppTable/" + sAppTable + "/redisconf";
    string sValue           = "";
	_oZkProcess.getData( sRedisConfPath, sValue );
	MYBOOST_LOG_INFO( OS_KV( "redisconfpath", sRedisConfPath ) << OS_KV( "value", sValue ) );
    RedisConf oRedisConf;
    
	if( DecodeRedisConf( sValue, oRedisConf ) == AICommon::RET_SUCC )
	{
		string sApp				= TC_Common::sepstr<string>( sAppTable, "_" )[0];
		string sTable			= TC_Common::sepstr<string>( sAppTable, "_" )[1];
		string sCommaAppTable   = sApp + "." + sTable;
		MapStr2RedisConf::iterator mSt = _mapAppTable2RedisConf.find( sCommaAppTable );
		if( mSt == _mapAppTable2RedisConf.end() )
		{
			_mapAppTable2RedisConf.insert( std::pair< string, RedisConf >( sCommaAppTable, oRedisConf ) );
			MYBOOST_LOG_INFO( "Insert RedisConf Succ." << OS_KV( "apptable", sCommaAppTable ) 
					<< OS_KV( "size", _mapAppTable2RedisConf.size() ) ); 
		}
		else
		{
			mSt->second = oRedisConf;
			MYBOOST_LOG_INFO( "Update RedisConf Succ." << OS_KV( "apptable", sAppTable ) 
					<< OS_KV( "size", _mapAppTable2RedisConf.size() ) ); 
		}
	}
}

void RedisProxyZkData::InitRedisConf( const std::string & sAppTable, void * pCtx )
{
	RedisProxyZkData * pZk	= static_cast<RedisProxyZkData *>(pCtx);
	string sRedisConfPath	= pZk->_sRootPath + "/RedisAppTable/" + sAppTable + "/redisconf";
	pZk->_oZkProcess.watchData( sRedisConfPath, RedisProxyZkData::RedisConfCallback, pZk);
	MYBOOST_LOG_INFO( "InitRedisConf Succ." << OS_KV( "redisconfpath", sRedisConfPath ) );
}

void RedisProxyZkData::InitRedisConnPoolInfoByGuard( const std::string & sAppTable )
{
	string sRedisConnPoolPath   = _sRootPath + "/RedisAppTable/" + sAppTable + "/redisconnpoolinfo";
    string sValue               = "";
	_oZkProcess.getData( sRedisConnPoolPath, sValue );
	MYBOOST_LOG_INFO( OS_KV( "redisconnpoolpath", sRedisConnPoolPath ) << OS_KV( "value", sValue ) );
    RedisProxyZkData::GenRedisConnPoolInfo( sRedisConnPoolPath, sValue, this );
}

void RedisProxyZkData::GenRedisConnPoolInfo( 
        const std::string & sPath, const std::string & sValue, void * pCtx )
{
	RedisProxyZkData * pZk = static_cast<RedisProxyZkData *>(pCtx);
    RedisConnPoolConf oRedisConnPoolConf;
	vector< string > vecRedisConnPoolConfInfo	= TC_Common::sepstr< string >( sPath, "/" );
	string sAppTable							= "";
	if( vecRedisConnPoolConfInfo.size() > size_t(REDISCONNPOOLCONFINFO_APPTABLE_INDEX + pZk->_nDiffIndex) )
	{
		string sTmpAppTable		= vecRedisConnPoolConfInfo[REDISCONNPOOLCONFINFO_APPTABLE_INDEX + pZk->_nDiffIndex];
		string sApp				= TC_Common::sepstr<string>( sTmpAppTable, "_" )[0];
		string sTable			= TC_Common::sepstr<string>( sTmpAppTable, "_" )[1];
		sAppTable				= sApp + "." + sTable;
	}

	if( DecodeRedisConnPoolInfo( sValue, oRedisConnPoolConf ) == AICommon::RET_SUCC )
	{
		taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( pZk->_olock );
		MapStr2RedisConnPoolConf::iterator mSt = pZk->_mapAppTable2RedisConnPoolConf.find( sAppTable );
		if( mSt == pZk->_mapAppTable2RedisConnPoolConf.end() )
		{
			pZk->_mapAppTable2RedisConnPoolConf.insert( std::pair< string, RedisConnPoolConf>( 
						sAppTable, oRedisConnPoolConf ) );
			MYBOOST_LOG_INFO( "Insert RedisConnPoolConf Succ." << OS_KV( "apptable", sAppTable ) 
					<< OS_KV( "size", pZk->_mapAppTable2RedisConnPoolConf.size() ) ); 
		}
		else
		{
			mSt->second = oRedisConnPoolConf;
			MYBOOST_LOG_INFO( "Update RedisConnPoolConf Succ." << OS_KV( "apptable", sAppTable ) 
					<< OS_KV( "size", pZk->_mapAppTable2RedisConnPoolConf.size() ) ); 
		}
	}
	else
	{
		MYBOOST_LOG_ERROR( "getData RedisConnPool Error." << OS_KV( "ConnPoolPath",sPath ) );
	}
}

void RedisProxyZkData::InitRedisConnPoolInfo( const std::string & sAppTable, void * pCtx )
{
	RedisProxyZkData * pZk	= static_cast<RedisProxyZkData *>(pCtx);
	string sRedisConnPoolPath   = pZk->_sRootPath + "/RedisAppTable/" + sAppTable + "/redisconnpoolinfo";
	pZk->_oZkProcess.watchData( sRedisConnPoolPath, RedisProxyZkData::RedisConnPoolInfoCallback, pZk );
	MYBOOST_LOG_INFO( "InitRedisConnPoolInfo Succ." << OS_KV( "RedisConnPoolPath", sRedisConnPoolPath ) );
}

void RedisProxyZkData::RedisConnPoolInfoCallback( 
		const std::string & sPath, const std::string & sValue, 
		void * pCtx, const struct Stat * pStat )
{
	RedisProxyZkData * pZk = static_cast<RedisProxyZkData *>(pCtx);
	RedisConnPoolConf oRedisConnPoolConf;
	vector< string > vecRedisConnPoolConfInfo	= TC_Common::sepstr< string >( sPath, "/" );
	string sAppTable							= "";
	if( vecRedisConnPoolConfInfo.size() > size_t(REDISCONNPOOLCONFINFO_APPTABLE_INDEX + pZk->_nDiffIndex) )
	{
		string sTmpAppTable		= vecRedisConnPoolConfInfo[REDISCONNPOOLCONFINFO_APPTABLE_INDEX + pZk->_nDiffIndex ];
		string sApp				= TC_Common::sepstr<string>( sTmpAppTable, "_" )[0];
		string sTable			= TC_Common::sepstr<string>( sTmpAppTable, "_" )[1];
		sAppTable				= sApp + "." + sTable;
	}

	if( DecodeRedisConnPoolInfo( sValue, oRedisConnPoolConf ) == AICommon::RET_SUCC )
	{
		taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( pZk->_olock );
		MapStr2RedisConnPoolConf::iterator mSt = pZk->_mapAppTable2RedisConnPoolConf.find( sAppTable );
		if( mSt == pZk->_mapAppTable2RedisConnPoolConf.end() )
		{
			pZk->_mapAppTable2RedisConnPoolConf.insert( std::pair< string, RedisConnPoolConf>( 
						sAppTable, oRedisConnPoolConf ) );
			MYBOOST_LOG_INFO( "Insert RedisConnPoolConf Succ." << OS_KV( "apptable", sAppTable ) 
					<< OS_KV( "size", pZk->_mapAppTable2RedisConnPoolConf.size() ) ); 
		}
		else
		{
			mSt->second = oRedisConnPoolConf;
			MYBOOST_LOG_INFO( "Update RedisConnPoolConf Succ." << OS_KV( "apptable", sAppTable ) 
					<< OS_KV( "size", pZk->_mapAppTable2RedisConnPoolConf.size() ) ); 
		}
	}
	else
	{
		MYBOOST_LOG_ERROR( "getData RedisConnPool Error." << OS_KV( "ConnPoolPath",sPath ) );
	}
}

int RedisProxyZkData::DecodeRedisConnPoolInfo( 
		const std::string & sRedisConnPoolInfo,
		RedisConnPoolConf & oRedisConnPoolConf )
{
	// 通过JSon解析 redisconnpollinfo
	try
	{
		JsonValueObjPtr basePtr = taf::JsonValueObjPtr::dynamicCast(TC_Json::getValue(sRedisConnPoolInfo));
		if( !basePtr )
		{
			MYBOOST_LOG_ERROR( "Decode RedisConnPoolInfo Error." );
			return AICommon::RET_FAIL;
		}

		JsonValueNumPtr maxconnsizePtr   = taf::JsonValueNumPtr::dynamicCast(basePtr->value["maxconnsize"]);
		if(maxconnsizePtr){ oRedisConnPoolConf._nMaxSize		= maxconnsizePtr->value;}
		JsonValueNumPtr minconnsizePtr   = taf::JsonValueNumPtr::dynamicCast(basePtr->value["minconnsize"]);
		if(minconnsizePtr){ oRedisConnPoolConf._nMinSize		= minconnsizePtr->value;}
		JsonValueNumPtr minusedcntPtr   = taf::JsonValueNumPtr::dynamicCast(basePtr->value["minusedcnt"]);
		if(minusedcntPtr){ oRedisConnPoolConf._nMinUsedCnt		= minusedcntPtr->value;}
		JsonValueNumPtr maxidletimePtr   = taf::JsonValueNumPtr::dynamicCast(basePtr->value["maxidletime"]);
		if(maxidletimePtr){ oRedisConnPoolConf._nMaxIdleTime	= maxidletimePtr->value;}
		JsonValueNumPtr createsizePtr   = taf::JsonValueNumPtr::dynamicCast(basePtr->value["createsize"]);
		if(createsizePtr){ oRedisConnPoolConf._nCreateSize		= createsizePtr->value;}
		JsonValueBooleanPtr isneedkeepalivePtr  
                = taf::JsonValueBooleanPtr::dynamicCast(basePtr->value["isneedkeepalive"]);
		if(isneedkeepalivePtr){ oRedisConnPoolConf._bIsNeedKeepAlive = isneedkeepalivePtr->value;}

		JsonValueNumPtr keepalivetimePtr   = taf::JsonValueNumPtr::dynamicCast(basePtr->value["keepalivetime"]);
		if(keepalivetimePtr){ oRedisConnPoolConf._nKeepAliveTime =  keepalivetimePtr->value;}

		JsonValueBooleanPtr isneedreportdetectPtr  
                = taf::JsonValueBooleanPtr::dynamicCast(basePtr->value["isneedreportdetect"]);
		if(isneedreportdetectPtr){ oRedisConnPoolConf._bIsNeedReportDetect = isneedreportdetectPtr->value;}
	}
	catch (std::exception &e)
	{
		MYBOOST_LOG_ERROR( "Decode RedisConnPoolInfo Fail." << OS_KV( "msg", e.what() ) );
		return AICommon::RET_FAIL;
	}
	return AICommon::RET_SUCC;
}

const SlotConf & RedisProxyZkData::GetSlotConfByAppTable( const std::string & sAppTable )
{
	MapStr2SlotConf::iterator mIt;
	if( ( mIt = _mapAppTable2SlotConf.find( sAppTable ) ) != _mapAppTable2SlotConf.end() )
	{
		return mIt->second;
	}
	return _oEmptySlotConf;
}

int RedisProxyZkData::DecodeSlotConf( const std::string & sSlotConf, SlotConf & oSlotConf )
{
	// 通过JSon解析SlotConf
	try
	{
		JsonValueObjPtr basePtr = taf::JsonValueObjPtr::dynamicCast(TC_Json::getValue(sSlotConf));
		if( !basePtr )
		{
			MYBOOST_LOG_ERROR( "Decode SlotConf Error." );
			return AICommon::RET_FAIL;
		}
		JsonValueNumPtr slotcntPtr = taf::JsonValueNumPtr::dynamicCast(basePtr->value["slotcnt"]);
		if( slotcntPtr ){ oSlotConf.set_nslotcnt( slotcntPtr->value ); }
		JsonValueStringPtr groupslotPtr = taf::JsonValueStringPtr::dynamicCast(basePtr->value["group_slot"]);
		if( groupslotPtr ){ oSlotConf.set_sgroupslot( groupslotPtr->value ); }
	}
	catch( std::exception & e )
	{
		MYBOOST_LOG_ERROR( "Decode SlotConf Fail." << OS_KV( "msg", e.what() ) );
		return AICommon::RET_FAIL;
	}
	return AICommon::RET_SUCC;
}

std::string RedisProxyZkData::EncodeSlotConf( RedisProxyCommon::SlotConf & oSlotConf )
{
	// 通过JSon解析SlotConf
	try
	{
		taf::JsonValueObjPtr basePtr	= new taf::JsonValueObj();

        taf::JsonValueNumPtr slotcntPtr = new taf::JsonValueNum( oSlotConf.nslotcnt() );
        basePtr->value["slotcnt"]       = slotcntPtr;

        taf::JsonValueStringPtr groupslotPtr = new taf::JsonValueString( oSlotConf.sgroupslot() );
        basePtr->value["group_slot"]    = groupslotPtr;

       return taf::TC_Json::writeValue( basePtr );
	}
	catch( std::exception & e )
	{
		MYBOOST_LOG_ERROR( "Encode SlotConf Fail." << OS_KV( "msg", e.what() ) );
	}
    return "";
}

void RedisProxyZkData::RedisSlotConfCallback( 
		const std::string & sPath, const std::string & sValue, 
		void * pCtx, const struct Stat * pStat )
{
	RedisProxyZkData * pZk = static_cast< RedisProxyZkData * >( pCtx );
	vector< string > vecRedisSlotConf	= TC_Common::sepstr< string >( sPath, "/" );
	string sAppTable							= "";
	if( vecRedisSlotConf.size() > size_t(SLOTCONF_APPTABLE_INDEX + pZk->_nDiffIndex) )
	{
		string sTmpAppTable		= vecRedisSlotConf[SLOTCONF_APPTABLE_INDEX + pZk->_nDiffIndex ];
		string sApp				= TC_Common::sepstr<string>( sTmpAppTable, "_" )[0];
		string sTable			= TC_Common::sepstr<string>( sTmpAppTable, "_" )[1];
		sAppTable				= sApp + "." + sTable;
	}

	SlotConf oSlotConf;
	if( DecodeSlotConf( sValue, oSlotConf ) == AICommon::RET_SUCC )
	{
		taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( pZk->_olock );
		MapStr2SlotConf::iterator mSt	= pZk->_mapAppTable2SlotConf.find( sAppTable );
		if( mSt == pZk->_mapAppTable2SlotConf.end() )
		{
			pZk->_mapAppTable2SlotConf.insert( std::pair< string, SlotConf >( sAppTable, oSlotConf ) );
			MYBOOST_LOG_INFO( "Insert SlotConf Succ." << OS_KV( "apptable", sAppTable ) 
					<< OS_KV( "size", pZk->_mapAppTable2SlotConf.size() ) ); 
		}
		else
		{
			mSt->second = oSlotConf;
			MYBOOST_LOG_INFO( "Update SlotConf Succ." << OS_KV( "apptable", sAppTable ) 
					<< OS_KV( "size", pZk->_mapAppTable2SlotConf.size() ) ); 
		}
		RedisProxySlot::getInstance()->InitSlot2GroupInfoByZkData( sAppTable, oSlotConf.sgroupslot() );
	}
}

string RedisProxyZkData::TransferGroupState2Str( int nState )
{
	string sState;
	switch( nState )
	{
		case RedisProxyCommon::GROUP_STATE_OK:
			sState = "ok";
			break;
		case RedisProxyCommon::GROUP_STATE_PFAIL:
			sState = "pfail";
			break;
		case RedisProxyCommon::GROUP_STATE_FAIL:
			sState = "fail";
			break;
	}
	return sState;
}

int RedisProxyZkData::TransferGroupState2Int( string sState )
{
	if( sState == "ok" )
	{
		return RedisProxyCommon::GROUP_STATE_OK;
	}
	else if( sState == "pfail" )
	{
		return RedisProxyCommon::GROUP_STATE_PFAIL;
	}
	else if( sState == "fail" )
	{
		return RedisProxyCommon::GROUP_STATE_FAIL;
	}
	return RedisProxyCommon::GROUP_STATE_OK;
}

// 考虑是否需要先把状态更新到本地内存中，然后再写入到ZK
bool RedisProxyZkData::SetGroupData( RedisProxyCommon::GroupInfo & oGroupInfo )
{
	//"/AIAD-DMP/RedisProxyServer/RedisAppTable/user_labels/groups/group_1":
	//		{"iid":1,"gid":1,"host":"172.16.155.210:20010","state":"ok","slotnum":170,"ctime":"2020-03-26 17:05:32","lftime":""},
	std::string sAppTable			=  oGroupInfo.sapptable();
	vector<std::string> vecAppInfo	= TC_Common::sepstr<string>( sAppTable, "." );
	std::string sApp				= vecAppInfo[0];
	std::string sTable				= vecAppInfo[1];
	string sRedisGroupPath = _sRootPath + "/RedisAppTable/" + sApp + "_" + sTable + 
		"/groups/group_" + TC_Common::tostr( oGroupInfo.ngroupid() );
	string sValue = EnCodeGroups(oGroupInfo);
	if( !sValue.empty() && !_oZkProcess.setData( sRedisGroupPath, sValue ) )
	{
		MYBOOST_LOG_ERROR( "SetGroupData." << OS_KV( "Path",sRedisGroupPath ) << OS_KV( "Value", sValue ) );
		return false;
	}
	return true;
}

void RedisProxyZkData::SetRebalanceSC2SlotConf( const std::string & sAppTable )
{
    // 修改zk中的配置
    // 缩容前的配置"group_slot": "1:1~170,2:171~341,3:342~512,4:513~682,5:683~853,6:854~1024"
    // 缩容后的配置"group_slot": "2:171~341|1~34,3:342~512|35~68,4:513~682|69~102,5:683~853|103~136,6:854~1024|137~170"
    string sValue   = "";
    auto ms2sc      = _mapAppTable2RebalanceSC.find( sAppTable );
    if( ms2sc != _mapAppTable2RebalanceSC.end() )
            sValue  = EncodeSlotConf( ms2sc->second );
	string sApp	    = TC_Common::sepstr<string>( sAppTable, "." )[0];
	string sTable   = TC_Common::sepstr<string>( sAppTable, "." )[1];
	string sLineAppTable    = sApp + "_" + sTable;
    string sSlotConfPath    = _sRootPath + "/RedisAppTable/" + sLineAppTable + "/groups/slotconf";
    if( !sValue.empty() && !_oZkProcess.setData( sSlotConfPath, sValue ) )
    {
        MYBOOST_LOG_ERROR( "Set slotconf Error." 
                << OS_KV( "Path", sSlotConfPath ) << OS_KV( "Value", sValue ) );
    }
    else
    {
        MYBOOST_LOG_DEBUG( "Set slotconf Succ." 
                << OS_KV( "Path", sSlotConfPath ) << OS_KV( "Value", sValue ) );
    }
}

bool RedisProxyZkData::DelGroupData( RedisProxyCommon::GroupInfo & oGroupInfo )
{
    //"/AIAD-DMP/RedisProxyServer/RedisAppTable/user_labels/groups/group_1":
    //		{"iid":1,"gid":1,"host":"172.16.155.210:20010","state":"ok","slotnum":170,"ctime":"2020-03-26 17:05:32","lftime":""},
    std::string sAppTable			=  oGroupInfo.sapptable();
    vector<std::string> vecAppInfo	= TC_Common::sepstr<string>( sAppTable, "." );
	std::string sApp				= vecAppInfo[0];
	std::string sTable				= vecAppInfo[1];
	string sRedisGroupPath = _sRootPath + "/RedisAppTable/" + sApp + "_" + sTable + 
		    "/groups/group_" + TC_Common::tostr( oGroupInfo.ngroupid() );
	if( !_oZkProcess.deleteNode( sRedisGroupPath, DEFAULT_GROUP_VERSION ) )
	{
		MYBOOST_LOG_ERROR( "DelGroupData." << OS_KV( "Path",sRedisGroupPath ) );
		return false;
	}
	return true;
}

void RedisProxyZkData::DelAppTableGroupData( const std::string & sAppTable )
{
	auto mat2dgi = _mapAppTable2DelGroupInfo.find( sAppTable );
	if( mat2dgi != _mapAppTable2DelGroupInfo.end() )
	{
		for( auto vgi = mat2dgi->second.begin(); vgi != mat2dgi->second.end(); ++ vgi )
		{
			DelGroupData( *vgi );
		}
	}
}

int RedisProxyZkData::DecodeGroupsRebalance(
		const std::string & sGroupsRebalance, 
                const std::string & sAppTable,
                vector< RedisProxyCommon::GroupInfo > & vecSetGroup,
                vector< RedisProxyCommon::GroupInfo > & vecDelGroup )
{
    /*    group_rebalance":
     *    { "addlist":[
     *          {"iid":1,"gid":7,"host":"172.16.155.239:10010"},
     *          {"iid":1,"gid":8,"host":"172.16.155.239:10011"}], 
     *      "dellist":[
     *          {"iid":1,"gid":1}] 
     *    } */
	// 通过JSon解析GroupsRebalance
	try
	{
		JsonValueObjPtr basePtr = taf::JsonValueObjPtr::dynamicCast(TC_Json::getValue(sGroupsRebalance));
		if( !basePtr )
		{
			MYBOOST_LOG_ERROR( "Decode GroupsRebalance Error." );
			return AICommon::RET_FAIL;
		}
		JsonValueArrayPtr addlistPtr 
                = taf::JsonValueArrayPtr::dynamicCast( basePtr->value["addlist"] );
        MYBOOST_LOG_DEBUG( OS_KV( "addlist.size", addlistPtr->value.size() ) );
		if(!addlistPtr)
		{
			MYBOOST_LOG_ERROR( "Decode addlist Null." );
			return AICommon::RET_FAIL;
		}
		for( size_t i = 0; i < size_t(addlistPtr->value.size()); ++ i )
		{
            RedisProxyCommon::GroupInfo oGroupInfo;
            RedisProxyCommon::Instance & oMaster = *( oGroupInfo.mutable_omaster() );
            oGroupInfo.set_sapptable( sAppTable );
            JsonValueObjPtr groupinfoPtr    = taf::JsonValueObjPtr::dynamicCast(addlistPtr->value[i]);

            JsonValueNumPtr iidPtr			= taf::JsonValueNumPtr::dynamicCast(groupinfoPtr->value["iid"]);
            if(iidPtr){ oMaster.set_ninstanceid( iidPtr->value );}

            JsonValueNumPtr gidPtr			= taf::JsonValueNumPtr::dynamicCast(groupinfoPtr->value["gid"]);
            if(gidPtr){ oGroupInfo.set_ngroupid( gidPtr->value );}
            
            JsonValueStringPtr hostPtr      = taf::JsonValueStringPtr::dynamicCast(groupinfoPtr->value["host"]);
            std::string sHost               = hostPtr->value;
            vector<std::string>vecHost      =  TC_Common::sepstr< string >( sHost, ":" );
            if( vecHost.size() == 2 )
            {
                oMaster.set_sip( vecHost[0] );
                oMaster.set_nport( TC_Common::strto<int>( vecHost[1] ) );
            }
            oGroupInfo.set_ngroupstate( RedisProxyCommon::GROUP_STATE_OK );
            oMaster.set_sctime( TC_Common::now2str( "%Y-%m-%d %H:%M:%S" ) );
            vecSetGroup.push_back( oGroupInfo );
			MYBOOST_LOG_INFO( "Decode groupinfo Succ." 
                    << OS_KV( "setgroupsize", vecSetGroup.size() )
                    << OS_KV( "groupinfo", oGroupInfo.DebugString() ) );
		}

        JsonValueArrayPtr dellistPtr 
                = taf::JsonValueArrayPtr::dynamicCast( basePtr->value["dellist"] );
        MYBOOST_LOG_DEBUG( OS_KV( "dellist.size", dellistPtr->value.size() ) );
		if(!dellistPtr)
		{
			MYBOOST_LOG_ERROR( "Decode dellist Null." );
			return AICommon::RET_FAIL;
		}
		for( size_t i = 0; i < size_t(dellistPtr->value.size()); ++ i )
		{
            RedisProxyCommon::GroupInfo oGroupInfo;
            RedisProxyCommon::Instance & oMaster = *( oGroupInfo.mutable_omaster() );
            oGroupInfo.set_sapptable( sAppTable );
            JsonValueObjPtr groupinfoPtr    = taf::JsonValueObjPtr::dynamicCast(dellistPtr->value[i]);

            JsonValueNumPtr iidPtr			= taf::JsonValueNumPtr::dynamicCast(groupinfoPtr->value["iid"]);
            if(iidPtr){ oMaster.set_ninstanceid( iidPtr->value );}

            JsonValueNumPtr gidPtr			= taf::JsonValueNumPtr::dynamicCast(groupinfoPtr->value["gid"]);
            if(gidPtr){ oGroupInfo.set_ngroupid( gidPtr->value );}

            vecDelGroup.push_back( oGroupInfo );
			MYBOOST_LOG_INFO( "Decode groupinfo Succ." 
                    << OS_KV( "delgroupsize", vecDelGroup.size() )
                    << OS_KV( "groupinfo", oGroupInfo.DebugString() ) );
        }
	}
	catch (std::exception &e)
	{
		MYBOOST_LOG_ERROR( "Decode groups_rebalance Fail." << OS_KV( "msg", e.what() ) );
		return AICommon::RET_FAIL;
	}
	return AICommon::RET_SUCC;
}

string RedisProxyZkData::EnCodeGroups( RedisProxyCommon::GroupInfo & oGroupInfo )
{
	MY_TRY
		taf::JsonValueObjPtr basePtr	= new taf::JsonValueObj();
	
		int	nIid						= oGroupInfo.omaster().ninstanceid();
		taf::JsonValueNumPtr iidPtr		= new taf::JsonValueNum( nIid );
		basePtr->value["iid"]			= iidPtr;

		int nGid						= oGroupInfo.ngroupid();
		taf::JsonValueNumPtr gidPtr		= new taf::JsonValueNum( nGid );
		basePtr->value["gid"]			= gidPtr;
		
		string sIp						= oGroupInfo.omaster().sip();
		string sPort					= TC_Common::tostr( oGroupInfo.omaster().nport() );
		taf::JsonValueStringPtr hostPtr = new taf::JsonValueString( sIp +":"+ sPort );
		basePtr->value["host"] = hostPtr;

		string	sState					= TransferGroupState2Str( oGroupInfo.ngroupstate() );
		taf::JsonValueStringPtr statePtr = new taf::JsonValueString(sState);
		basePtr->value["state"] = statePtr;

		int	nSlotNum					= oGroupInfo.omaster().nslotnum();	
		taf::JsonValueNumPtr slotnumPtr	= new taf::JsonValueNum( nSlotNum );
		basePtr->value["slotnum"]		= slotnumPtr;

		string sCtime					= oGroupInfo.omaster().sctime();
		taf::JsonValueStringPtr ctimePtr = new taf::JsonValueString(sCtime);
		basePtr->value["ctime"]			= ctimePtr;

		string sMtime					= TC_Common::now2str( "%Y-%m-%d %H:%M:%S" );
		taf::JsonValueStringPtr mtimePtr = new taf::JsonValueString(sMtime);
		basePtr->value["mtime"]			= mtimePtr;

		if( sState == "fail" )
		{
			string sLftime					= TC_Common::now2str( "%Y-%m-%d %H:%M:%S" );
			taf::JsonValueStringPtr lftimePtr = new taf::JsonValueString(sLftime);
			basePtr->value["lftime"]		= lftimePtr;
		}
		return taf::TC_Json::writeValue( basePtr );
	MYBOOST_CATCH( "EncodeGroups Error", LOG_ERROR );
	return "";
}

void RedisProxyZkData::InitRedisSlotConf( const std::string & sAppTable, void * pCtx )
{
	RedisProxyZkData * pZk	= static_cast<RedisProxyZkData *>(pCtx);
	string sRedisSlotConfPath = pZk->_sRootPath + "/RedisAppTable/" + sAppTable + "/slotconf";
	pZk->_oZkProcess.watchData( sRedisSlotConfPath, RedisProxyZkData::RedisSlotConfCallback, pZk );
	MYBOOST_LOG_INFO( "InitRedisSlotConf Succ." << OS_KV( "RedisSlotConfPath", sRedisSlotConfPath ) );
}

int RedisProxyZkData::TransferSlotState2Int( const std::string & sState )
{
	if( sState == "pending" )
	{
		return RedisProxyCommon::SLOT_STATE_PENDING;
	}
	else if( sState == "migrating" )
	{
		return RedisProxyCommon::SLOT_STATE_MIGRATING;
	}
	else if( sState == "importing" )
	{
		return RedisProxyCommon::SLOT_STATE_IMPORTING;
	}
	else if( sState == "finish" )
	{
		return RedisProxyCommon::SLOT_STATE_FINISH;
	}
	return RedisProxyCommon::SLOT_STATE_NONE;
}

string RedisProxyZkData::TransferSlotState2Str( int nState )
{
    string sState;
    switch( nState )
    {
        case RedisProxyCommon::SLOT_STATE_PENDING:
            sState = "pending";
            break;
        case RedisProxyCommon::SLOT_STATE_MIGRATING:
            sState = "migrating";
            break;
        case RedisProxyCommon::SLOT_STATE_PREPARING:
            sState = "preparing";
            break;
        case RedisProxyCommon::SLOT_STATE_IMPORTING:
            sState = "importing";
            break;
        case RedisProxyCommon::SLOT_STATE_FINISH:
            sState = "finish";
            break;
        default:
            sState = "normal";
    }
    return sState;
}

int RedisProxyZkData::DecodeSlotAction( const std::string & sSlotAction, MapInt2SlotAction & mapSlotId2SA )
{
	// 通过JSon解析SlotActionList
	try
	{
		JsonValueObjPtr basePtr = taf::JsonValueObjPtr::dynamicCast(TC_Json::getValue(sSlotAction));
		if( !basePtr )
		{
			MYBOOST_LOG_ERROR( "Decode SlotAction Error." );
			return AICommon::RET_FAIL;
		}
        JsonValueArrayPtr SlotActionListPtr     
                = taf::JsonValueArrayPtr::dynamicCast( basePtr->value["slotactionlist"] );
        if( !SlotActionListPtr )
        {
			MYBOOST_LOG_ERROR( "Decode SlotActionList Null." );
			return AICommon::RET_FAIL;
        }
        for( size_t i = 0; i < size_t( SlotActionListPtr->value.size()); ++ i )
        {
            SlotAction oSlotAction;
            JsonValueObjPtr slotactionPtr     = taf::JsonValueObjPtr::dynamicCast(SlotActionListPtr->value[i]);

            JsonValueNumPtr slotidPtr = taf::JsonValueNumPtr::dynamicCast(slotactionPtr->value["slotid"]);
            if( slotidPtr ){ oSlotAction.set_nslotid( slotidPtr->value ); }
            JsonValueStringPtr statePtr = taf::JsonValueStringPtr::dynamicCast(slotactionPtr->value["state"]);
            if( statePtr ){ oSlotAction.set_nstate( TransferSlotState2Int( statePtr->value ) ); }
            JsonValueNumPtr fromgidPtr = taf::JsonValueNumPtr::dynamicCast(slotactionPtr->value["fromgid"]);
            if( fromgidPtr ){ oSlotAction.set_nfromgid( fromgidPtr->value ); }
            JsonValueNumPtr togidPtr = taf::JsonValueNumPtr::dynamicCast(slotactionPtr->value["togid"]);
            if( togidPtr ){ oSlotAction.set_ntogid( togidPtr->value ); }
            JsonValueNumPtr cursorPtr = taf::JsonValueNumPtr::dynamicCast(slotactionPtr->value["cursor"]);
            if( cursorPtr ){ oSlotAction.set_ncursor( cursorPtr->value ); }
            JsonValueStringPtr stimePtr = taf::JsonValueStringPtr::dynamicCast(slotactionPtr->value["stime"]);
            if( stimePtr ){ oSlotAction.set_sstime( stimePtr->value ); }
            JsonValueStringPtr ctimePtr = taf::JsonValueStringPtr::dynamicCast(slotactionPtr->value["ctime"]);
            if( ctimePtr ){ oSlotAction.set_sctime( ctimePtr->value ); }
			mapSlotId2SA.insert( std::pair< int, RedisProxyCommon::SlotAction >( 
                        oSlotAction.nslotid(), oSlotAction ) );
			MYBOOST_LOG_INFO( "Decode SlotActionList Succ." 
					<< OS_KV( "mapslot2sa.size", mapSlotId2SA.size() )
					<< OS_KV( "slotaction", oSlotAction.DebugString() ) );
        }
	}
	catch( std::exception & e )
	{
		MYBOOST_LOG_ERROR( "Decode SlotActionList Fail." << OS_KV( "msg", e.what() ) );
		return AICommon::RET_FAIL;
	}
	return AICommon::RET_SUCC;
}

string RedisProxyZkData::EncodeSlotAction( MapInt2SlotAction & mapSlotId2SlotAction )
{
	MY_TRY
		taf::JsonValueObjPtr basePtr			= new taf::JsonValueObj();
		taf::JsonValueArrayPtr slotactionsPtr	= new taf::JsonValueArray();
		basePtr->value["slotactionlist"]		= slotactionsPtr;
		for( MapInt2SlotAction::iterator mit = mapSlotId2SlotAction.begin();
				mit != mapSlotId2SlotAction.end(); ++ mit )
		{
			taf::JsonValueObjPtr slotactionPtr = new taf::JsonValueObj();

			int	nSlotId						= mit->second.nslotid();
			taf::JsonValueNumPtr slotidPtr	= new taf::JsonValueNum( nSlotId );
			slotactionPtr->value["slotid"]	= slotidPtr;

			string	sState					= TransferSlotState2Str( mit->second.nstate() );
			taf::JsonValueStringPtr statePtr = new taf::JsonValueString(sState);
			slotactionPtr->value["state"]	= statePtr;

			int nfromgid					= mit->second.nfromgid();
			taf::JsonValueNumPtr nfromgidPtr= new taf::JsonValueNum( nfromgid );
			slotactionPtr->value["fromgid"]	= nfromgidPtr;

			int ntogid						= mit->second.ntogid();
			taf::JsonValueNumPtr ntogidPtr	= new taf::JsonValueNum( ntogid );
			slotactionPtr->value["togid"]	= ntogidPtr;

			int ncursor						= mit->second.ncursor();
			taf::JsonValueNumPtr cursorPtr	= new taf::JsonValueNum( ncursor );
			slotactionPtr->value["cursor"]	= cursorPtr;

			string	ssTime					= mit->second.sstime();
			taf::JsonValueStringPtr stimePtr = new taf::JsonValueString(ssTime);
			slotactionPtr->value["stime"]	= stimePtr;

			string	scTime					= mit->second.sctime();
			taf::JsonValueStringPtr ctimePtr = new taf::JsonValueString(scTime);
			slotactionPtr->value["ctime"]	= ctimePtr;

			slotactionsPtr->value.push_back( slotactionPtr );
		}
		return taf::TC_Json::writeValue( basePtr );
	MYBOOST_CATCH( "EncodeSlotAction Error", LOG_ERROR );
	return "";
}

bool RedisProxyZkData::UpdateSlotAction( 
		const std::string & sAppTable, 
		const RedisProxyCommon::SlotAction & oSlotAction )
{
	vector<std::string> vecAppInfo	= TC_Common::sepstr<string>( sAppTable, "." );
	std::string sApp				= vecAppInfo[0];
	std::string sTable				= vecAppInfo[1];

    taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( _olock );
	MapStr2ISlotAction::iterator msisa = _mapAppTable2ISlotAction.find( sAppTable );
	if( msisa != _mapAppTable2ISlotAction.end() )	
	{
		MapInt2SlotAction::iterator misa = msisa->second.find( oSlotAction.nslotid() );
		if( misa != msisa->second.end() )
		{
			misa->second		= oSlotAction;
			std::string sValue	= EncodeSlotAction( msisa->second ); 
			string sSlotActionPath
					= _sRootPath + "/RedisAppTable/" + sApp + "_" + sTable + "/groups/slotaction";
			if( !sValue.empty() && !_oZkProcess.setData( sSlotActionPath, sValue ) )
			{
				MYBOOST_LOG_ERROR( "Update SlotActionData Error." 
						<< OS_KV( "Path", sSlotActionPath ) 
						<< OS_KV( "Value", sValue ) );
				return false;
			}
			else
			{
				MYBOOST_LOG_DEBUG( "Update SlotActionData Succ." 
						<< OS_KV( "Path", sSlotActionPath ) 
						<< OS_KV( "Value", sValue ) );
				return true;
			}
		}
		else
		{
			MYBOOST_LOG_ERROR( "Update SlotAction NotFind ." << OS_KV( "SlotId", oSlotAction.nslotid() ) 
					<< OS_KV( "size", msisa->second.size() ) 
					<< OS_KV( "size", _mapAppTable2ISlotAction.size() ) ); 
		}
	}
	else
	{
		MYBOOST_LOG_ERROR( "Update SlotAction NotFind ." << OS_KV( "AppTable", sAppTable )  
				<< OS_KV( "size", _mapAppTable2ISlotAction.size() ) ); 
	}
	return false;
}
/*
void RedisProxyZkData::InitRedisSlotConfByGuard( const std::string & sAppTable )
{
    string sValue               = "";
	string sRedisSlotConfPath = _sRootPath + "/RedisAppTable/" + sAppTable + "/groups/slotconf";
	_oZkProcess.getData( sRedisSlotConfPath, sValue );
	SlotConf oSlotConf;

	if( DecodeSlotConf( sValue, oSlotConf ) == AICommon::RET_SUCC )
	{
		string sApp				= TC_Common::sepstr<string>( sAppTable, "_" )[0];
		string sTable			= TC_Common::sepstr<string>( sAppTable, "_" )[1];
		string sCommaAppTable   = sApp + "." + sTable;
		MapStr2SlotConf::iterator mSt = _mapAppTable2SlotConf.find( sCommaAppTable );
		if( mSt == _mapAppTable2SlotConf.end() )
		{
			_mapAppTable2SlotConf.insert( std::pair< string, SlotConf >( sCommaAppTable, oSlotConf ) );
			MYBOOST_LOG_INFO( "Insert SlotConf Succ." << OS_KV( "apptable", sCommaAppTable ) 
					<< OS_KV( "size", _mapAppTable2SlotConf.size() ) ); 
		}
		else
		{
			mSt->second = oSlotConf;
			MYBOOST_LOG_INFO( "Update SlotConf Succ." << OS_KV( "apptable", sAppTable ) 
					<< OS_KV( "size", _mapAppTable2SlotConf.size() ) ); 
		}
		RedisProxySlot::getInstance()->InitSlot2GroupInfoByZkData( sAppTable, oSlotConf.sgroupslot() );
	}
}
*/

int RedisProxyZkData::DecodeRedisGroupsInfo(
		const std::string & sGroupsInfo, RedisProxyCommon::GroupInfo & oGroupInfo )
{
	// "/AIAD-DMP/RedisProxyServer/RedisAppTable/user_labels/groups/group_1":
	//			{"iid":1,"gid":1,"host":"172.16.155.210:20010","state":"ok","slotnum":170,"ctime":"2020-03-26 17:05:32","lftime":""},
	RedisProxyCommon::Instance & oMaster = *( oGroupInfo.mutable_omaster() );
	try
	{
		JsonValueObjPtr basePtr = taf::JsonValueObjPtr::dynamicCast(TC_Json::getValue(sGroupsInfo));
		if( !basePtr )
		{
			MYBOOST_LOG_ERROR( "Decode GroupInfo Error." << OS_KV( "groupinfo", sGroupsInfo ) );
			return AICommon::RET_FAIL;
		}

		JsonValueNumPtr iidPtr	= taf::JsonValueNumPtr::dynamicCast( basePtr->value["iid"] );
		if(iidPtr){ oMaster.set_ninstanceid( iidPtr->value ); }

		JsonValueNumPtr gidPtr	= taf::JsonValueNumPtr::dynamicCast( basePtr->value["gid"] );
		if(gidPtr){ oGroupInfo.set_ngroupid( gidPtr->value ); }

		JsonValueStringPtr hostPtr   = taf::JsonValueStringPtr::dynamicCast(basePtr->value["host"]);
		if(hostPtr){ 
			vector< string > vecHost = TC_Common::sepstr<string>( hostPtr->value, ":" );
				oMaster.set_sip( vecHost[0] );
				oMaster.set_nport( TC_Common::strto<int>(vecHost[1]) );
		}
		
		JsonValueStringPtr statePtr   = taf::JsonValueStringPtr::dynamicCast(basePtr->value["state"]);
		if(statePtr){ 
			int nState = TransferGroupState2Int( statePtr->value );
			oGroupInfo.set_ngroupstate( nState ); 
			oMaster.set_ninstancestate( nState );
		}

		JsonValueNumPtr slotnumPtr	= taf::JsonValueNumPtr::dynamicCast( basePtr->value["slotnum"] );
		if(slotnumPtr){ oMaster.set_nslotnum( slotnumPtr->value ); }
		
		JsonValueStringPtr ctimePtr   
					= taf::JsonValueStringPtr::dynamicCast(basePtr->value["ctime"]);
		if(ctimePtr){ oMaster.set_sctime( ctimePtr->value ); }

		JsonValueStringPtr lftimePtr   
					= taf::JsonValueStringPtr::dynamicCast(basePtr->value["lftime"]);
		if(lftimePtr){ oMaster.set_slftime( lftimePtr->value ); }
	}
	catch ( std::exception & e )
	{
		MYBOOST_LOG_ERROR( "DecodeRedisGroupsInfo Fail."<< OS_KV( "msg", e.what() ) );
		return AICommon::RET_FAIL;
	}
	return AICommon::RET_SUCC;
}

void RedisProxyZkData::UpdateFailOvers( 
		RedisProxyCommon::FailOverInfo & oFailOverInfo,
		MapStr2FailOverInfo & mapHost2FailOverInfo )
{
	std::string sHost					= oFailOverInfo.shost();
	MapStr2FailOverInfo::iterator mIt	= mapHost2FailOverInfo.find( sHost );
	if( mIt == mapHost2FailOverInfo.end() )
	{
		mapHost2FailOverInfo.insert( 
				std::pair< string, RedisProxyCommon::FailOverInfo >( sHost, oFailOverInfo ) ) ;
	}
	else
	{
		mIt->second = oFailOverInfo;
	}
}

bool RedisProxyZkData::DelFailOver( const string & sAppTable, const string & sHost )
{
	vector<std::string> vecAppInfo	= TC_Common::sepstr<string>( sAppTable, "." );
	std::string sApp				= vecAppInfo[0];
	std::string sTable				= vecAppInfo[1];
    MapStr2FailOverInfo::iterator mIt	= _mapHost2FailOverInfo.find( sHost );
	if( mIt != _mapHost2FailOverInfo.end() )
	{
		_mapHost2FailOverInfo.erase( mIt );
	}
	string sFailOverPath	
			= _sRootPath + "/RedisAppTable/" + sApp + "_" + sTable + "/groups/failover";
	string sValue			= EncodeFailovers( _mapHost2FailOverInfo );
	MYBOOST_LOG_DEBUG( "Del FailOverData." << OS_KV( "Path", sFailOverPath ) << OS_KV( "Value", sValue ) );
	if( !sValue.empty() && !_oZkProcess.setData( sFailOverPath, sValue ) )
	{
		MYBOOST_LOG_ERROR( "Del FailOverData." << OS_KV( "Path", sFailOverPath ) << OS_KV( "Value", sValue ) );
		return false;
	}
	return true;
}

bool RedisProxyZkData::SetFailOvers( RedisProxyCommon::FailOverInfo & oFailOverInfo )
{
	std::string sAppTable			=  oFailOverInfo.sapptable();
	vector<std::string> vecAppInfo	= TC_Common::sepstr<string>( sAppTable, "." );
	std::string sApp				= vecAppInfo[0];
	std::string sTable				= vecAppInfo[1];

	string sFailOverPath	
			= _sRootPath + "/RedisAppTable/" + sApp + "_" + sTable + "/groups/failover";
	// 构成Host2FailOverInfo，全部更新到Zk中
	UpdateFailOvers( oFailOverInfo, _mapHost2FailOverInfo );
	string sValue			= EncodeFailovers( _mapHost2FailOverInfo );
	MYBOOST_LOG_INFO( "Set FailOverData." << OS_KV( "Path", sFailOverPath ) << OS_KV( "Value", sValue ) );
	if( !sValue.empty() && !_oZkProcess.setData( sFailOverPath, sValue ) )
	{
		MYBOOST_LOG_ERROR( "Set FailOverData." << OS_KV( "Path", sFailOverPath ) << OS_KV( "Value", sValue ) );
		return false;
	}
	RedisProxyKafka::getInstance()->UpdateFailOverTopic( _mapHost2FailOverInfo );
	return true;
}

string RedisProxyZkData::EncodeFailovers( MapStr2FailOverInfo & mapHost2FailOverInfo )
{
	MY_TRY
		taf::JsonValueObjPtr basePtr		= new taf::JsonValueObj();
		taf::JsonValueArrayPtr failoverinfosPtr	= new taf::JsonValueArray();
		basePtr->value["failoverlist"]		= failoverinfosPtr;
		for( MapStr2FailOverInfo::iterator mit = mapHost2FailOverInfo.begin();
				mit != mapHost2FailOverInfo.end(); ++ mit )
		{
			taf::JsonValueObjPtr failoverinfoPtr = new taf::JsonValueObj();

			int	nIid						= mit->second.ninstanceid();
			taf::JsonValueNumPtr iidPtr		= new taf::JsonValueNum( nIid );
			failoverinfoPtr->value["iid"]	= iidPtr;

			int nGid						= mit->second.ngroupid();
			taf::JsonValueNumPtr gidPtr		= new taf::JsonValueNum( nGid );
			failoverinfoPtr->value["gid"]			= gidPtr;

			string	sState					= TransferGroupState2Str( mit->second.ngroupstate() );
			taf::JsonValueStringPtr statePtr = new taf::JsonValueString(sState);
			failoverinfoPtr->value["state"] = statePtr;

			string	sTopic					= mit->second.stopic();
			taf::JsonValueStringPtr topicPtr = new taf::JsonValueString(sTopic);
			failoverinfoPtr->value["topic"] = topicPtr;

			int nPartition					= mit->second.npartition();
			taf::JsonValueNumPtr partitionPtr= new taf::JsonValueNum( nPartition );
			failoverinfoPtr->value["partition"]		= partitionPtr;

			string	sHost					= mit->second.shost();
			taf::JsonValueStringPtr hostPtr = new taf::JsonValueString(sHost);
			failoverinfoPtr->value["host"]	= hostPtr;

			failoverinfosPtr->value.push_back( failoverinfoPtr );
		}
		return taf::TC_Json::writeValue( basePtr );
	MYBOOST_CATCH( "EncodeFailOver Error", LOG_ERROR );
	return "";
}

int RedisProxyZkData::DecodeRedisFailOverInfos(
		const std::string & sFailOverInfo, 
        const std::string & sAppTable,
		MapStr2FailOverInfo & mapHost2FailOverInfo )
{
	/*
	"/AIAD-DMP/RedisProxyServer/RedisAppTable/user_labels/groups/failover":{"failoverlist":[
			{"iid":1,"gid":1,"state":"fail","topic":"redis_failover","partition":0,"host":"172.16.155.210:20010"},
			{"iid":1,"gid":2,"state":"fail","topic":"redis_failover","partition":1,"host":"172.16.155.239:20010"}
	]}
	*/
	try
	{
		JsonValueObjPtr basePtr = taf::JsonValueObjPtr::dynamicCast(TC_Json::getValue(sFailOverInfo));
		if( !basePtr )
		{
			MYBOOST_LOG_ERROR( "Decode FailOverInfo Error." << OS_KV( "failoverinfo", sFailOverInfo ) );
			return AICommon::RET_FAIL;
		}
		JsonValueArrayPtr FailOverListPtr = taf::JsonValueArrayPtr::dynamicCast( basePtr->value["failoverlist"] );
		if(!FailOverListPtr)
		{
			MYBOOST_LOG_ERROR( "Decode FailOverList Null." << OS_KV( "failoverinfo", sFailOverInfo ) );
			return AICommon::RET_FAIL;
		}
		for( size_t i = 0; i < size_t( FailOverListPtr->value.size()); ++ i )
		{
			JsonValueObjPtr failoverPtr = taf::JsonValueObjPtr::dynamicCast(FailOverListPtr->value[i]);

			RedisProxyCommon::FailOverInfo oFailOverInfo;

            string sApp				= TC_Common::sepstr<string>( sAppTable, "_" )[0];
            string sTable			= TC_Common::sepstr<string>( sAppTable , "_" )[1];
            string sCommaAppTable   = sApp + "." + sTable;
            oFailOverInfo.set_sapptable( sCommaAppTable );

			JsonValueNumPtr iidPtr	= taf::JsonValueNumPtr::dynamicCast( failoverPtr->value["iid"] );
			if(iidPtr){ oFailOverInfo.set_ninstanceid( iidPtr->value ); }

			JsonValueNumPtr gidPtr	= taf::JsonValueNumPtr::dynamicCast( failoverPtr->value["gid"] );
			if(gidPtr){ oFailOverInfo.set_ngroupid( gidPtr->value ); }

			JsonValueStringPtr hostPtr   = taf::JsonValueStringPtr::dynamicCast( failoverPtr->value["host"] );
			if(hostPtr) oFailOverInfo.set_shost( hostPtr->value );

			JsonValueStringPtr statePtr   = taf::JsonValueStringPtr::dynamicCast( failoverPtr->value["state"] );
			if(statePtr){ 
				int nState = TransferGroupState2Int( statePtr->value );
				oFailOverInfo.set_ngroupstate( nState ); 
			}

			JsonValueNumPtr partitionPtr	= taf::JsonValueNumPtr::dynamicCast( failoverPtr->value["partition"] );
			if(partitionPtr){ oFailOverInfo.set_npartition( partitionPtr->value ); }
			
			JsonValueStringPtr topicPtr   
						= taf::JsonValueStringPtr::dynamicCast( failoverPtr->value["topic"]);
			if(topicPtr){ oFailOverInfo.set_stopic( topicPtr->value ); }

			JsonValueStringPtr lftimePtr   
						= taf::JsonValueStringPtr::dynamicCast( failoverPtr->value["lftime"]);
			if(lftimePtr){ oFailOverInfo.set_slftime( lftimePtr->value ); }

			string sHost = oFailOverInfo.shost();
			mapHost2FailOverInfo.insert( pair< string, RedisProxyCommon::FailOverInfo >( sHost, oFailOverInfo ) );
			MYBOOST_LOG_DEBUG( OS_KV( "index", i ) 
					<< OS_KV( "size", mapHost2FailOverInfo.size() )
					<< "FailOverInfo:" << oFailOverInfo.DebugString() );
		}
	}
	catch ( std::exception & e )
	{
		MYBOOST_LOG_ERROR( "DecodeRedisGroupFailOverInfo Fail."<< OS_KV( "msg", e.what() ) 
				<< OS_KV( "failoverinfo", sFailOverInfo ) );
		return AICommon::RET_FAIL;
	}
	return AICommon::RET_SUCC;
}

void RedisProxyZkData::RedisGroupDataCallback( 
		const std::string & sPath, const std::string & sValue, 
		void * pCtx, const struct Stat * pStat )
{
	MYBOOST_LOG_INFO( OS_KV( "Path", sPath ) << OS_KV( "GroupDataCallback", sValue ) );
	RedisProxyZkData * pZk = static_cast<RedisProxyZkData *>(pCtx);
	vector< string > vecGroupsInfo	= TC_Common::sepstr< string >( sPath, "/" );
	string sGroupsNode				= "";
	if( vecGroupsInfo.size() >= size_t(GROUPS_NODE_INDEX + pZk->_nDiffIndex) )	
		sGroupsNode = vecGroupsInfo[GROUPS_NODE_INDEX + pZk->_nDiffIndex ];
    string sAppTable			    = "";
    if( vecGroupsInfo.size() > size_t(GROUPS_APPTABLE_INDEX + pZk->_nDiffIndex) )
    {
        string sTmpAppTable		= vecGroupsInfo[GROUPS_APPTABLE_INDEX + pZk->_nDiffIndex];
        string sApp				= TC_Common::sepstr<string>( sTmpAppTable, "_" )[0];
        string sTable			= TC_Common::sepstr<string>( sTmpAppTable, "_" )[1];
        sAppTable				= sApp + "." + sTable;
    }
    // 注意：除开group_实例节点使用模糊查找，其他节点都需要精确比较 "=="

	if( sGroupsNode == "failover" )
	{
		map< string, RedisProxyCommon::FailOverInfo > mapHost2FailOverInfo;
		if( DecodeRedisFailOverInfos( sValue, sAppTable, mapHost2FailOverInfo ) == AICommon::RET_SUCC )
		{
			RedisProxyZkData * pZk = static_cast<RedisProxyZkData *>(pCtx);
			taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( pZk->_olock );
            pZk->_mapHost2FailOverInfo.swap( mapHost2FailOverInfo );
			RedisProxyKafka::getInstance()->UpdateFailOverTopic( pZk->_mapHost2FailOverInfo );
		}
	}
    else if( sGroupsNode == "slotconf" )
    {
        SlotConf oSlotConf;
        if( DecodeSlotConf( sValue, oSlotConf ) == AICommon::RET_SUCC )
        {
            taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( pZk->_olock );
            MapStr2SlotConf::iterator mSt	= pZk->_mapAppTable2SlotConf.find( sAppTable );
            if( mSt == pZk->_mapAppTable2SlotConf.end() )
            {
                pZk->_mapAppTable2SlotConf.insert( std::pair< string, SlotConf >( sAppTable, oSlotConf ) );
                MYBOOST_LOG_INFO( "Insert SlotConf Succ." << OS_KV( "apptable", sAppTable ) 
                        << OS_KV( "size", pZk->_mapAppTable2SlotConf.size() ) ); 
            }
            else
            {
                mSt->second = oSlotConf;
                MYBOOST_LOG_INFO( "Update SlotConf Succ." << OS_KV( "apptable", sAppTable ) 
                        << OS_KV( "size", pZk->_mapAppTable2SlotConf.size() ) ); 
            }
            RedisProxySlot::getInstance()->InitSlot2GroupInfoByZkData( sAppTable, oSlotConf.sgroupslot() );
            CRedisConnectPoolFactory_Queue<string>::getInstance()->InitRedisConnectPoolFactoryByZk();
        }
    }
    else if( sGroupsNode == "slotaction" )
    {
        // 需要解析出多个slotaction
        MapInt2SlotAction mapSlotId2SA;
        if( DecodeSlotAction( sValue, mapSlotId2SA ) == AICommon::RET_SUCC )
        {
            taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( pZk->_olock );
            MapStr2ISlotAction::iterator mSt	= pZk->_mapAppTable2ISlotAction.find( sAppTable );
            if( mSt == pZk->_mapAppTable2ISlotAction.end() )
            {
                pZk->_mapAppTable2ISlotAction.insert( std::pair< string, MapInt2SlotAction >( sAppTable, mapSlotId2SA ) );
                MYBOOST_LOG_INFO( "Insert SlotAction Succ." << OS_KV( "apptable", sAppTable ) 
                        << OS_KV( "size", pZk->_mapAppTable2ISlotAction.size() ) ); 
                RedisProxySlotAction::getInstance()->InitSlotActions( sAppTable, mapSlotId2SA );
            }
            else
            {
                mSt->second = mapSlotId2SA;
                MYBOOST_LOG_INFO( "Update SlotAction Succ." << OS_KV( "apptable", sAppTable ) 
                        << OS_KV( "size", pZk->_mapAppTable2ISlotAction.size() ) ); 
                RedisProxySlotAction::getInstance()->UpdateSlotActions( sAppTable, mapSlotId2SA );
            }
        }
    }
    else if( sGroupsNode == "slotconf_rebalance" )
    {
        // 生成一个rebalance映射表
        SlotConf oSlotConf;
        if( DecodeSlotConf( sValue, oSlotConf ) == AICommon::RET_SUCC )
        {
            taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( pZk->_olock );
            MapStr2SlotConf::iterator mSt	= pZk->_mapAppTable2RebalanceSC.find( sAppTable );
            if( mSt == pZk->_mapAppTable2RebalanceSC.end() )
            {
                pZk->_mapAppTable2RebalanceSC.insert( std::pair< string, SlotConf >( sAppTable, oSlotConf ) );
                MYBOOST_LOG_INFO( "Insert Rebalance SlotConf Succ." << OS_KV( "apptable", sAppTable ) 
                        << OS_KV( "size", pZk->_mapAppTable2RebalanceSC.size() ) ); 
            }
            else
            {
                mSt->second = oSlotConf;
                MYBOOST_LOG_INFO( "Update Rebalance SlotConf Succ." << OS_KV( "apptable", sAppTable ) 
                        << OS_KV( "size", pZk->_mapAppTable2RebalanceSC.size() ) ); 
            }
            RedisProxySlot::getInstance()->InitRebalanceSlot2GroupInfoByZkData( sAppTable, oSlotConf.sgroupslot() );
        }
    }
	else if( sGroupsNode.find( "group_" ) != string::npos )
	{
		RedisProxyCommon::GroupInfo oGroupInfo;
		oGroupInfo.set_sapptable( sAppTable );
		if( DecodeRedisGroupsInfo( sValue, oGroupInfo ) == AICommon::RET_SUCC )
		{
			int nGroupId			= oGroupInfo.ngroupid();
			RedisProxyZkData * pZk	= static_cast<RedisProxyZkData *>(pCtx);
			taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( pZk->_olock );
			MapStr2GIdGroupInfo::iterator msIt = pZk->_mapAppTable2GidGroupInfo.find( sAppTable );
			if( msIt != pZk->_mapAppTable2GidGroupInfo.end() )
			{
				MYBOOST_LOG_INFO( "Update GroupsInfo Start." << OS_KV( "apptable", sAppTable ) 
						<< OS_KV( "groupid", nGroupId ) 
						<< OS_KV( "size", pZk->_mapAppTable2GidGroupInfo.size() ));
				MapInt2GroupInfo::iterator miIt = msIt->second.find( nGroupId );
				if( miIt != msIt->second.end() )
				{
					miIt->second = oGroupInfo;
				}
				else
				{
					msIt->second.insert( std::pair< int, RedisProxyCommon::GroupInfo >( nGroupId, oGroupInfo ) );
				}
				RedisProxyFailOver::getInstance()->UpdateGroupsInfo( sAppTable, msIt->second );
			}
			else
			{
				MYBOOST_LOG_INFO( "Insert GroupsInfo Succ." << OS_KV( "apptable", sAppTable ) 
						<< OS_KV( "groupid", nGroupId ) 
						<< OS_KV( "size", pZk->_mapAppTable2GidGroupInfo.size() ));
				MapInt2GroupInfo mapGid2GroupInfo;
				mapGid2GroupInfo.insert( std::pair< int, RedisProxyCommon::GroupInfo >( nGroupId, oGroupInfo ) );
				pZk->_mapAppTable2GidGroupInfo.insert( 
						std::pair< string, map< int, RedisProxyCommon::GroupInfo > >( sAppTable, mapGid2GroupInfo ) );
				RedisProxyFailOver::getInstance()->UpdateGroupsInfo( sAppTable, mapGid2GroupInfo );
			}
		}
		else
		{
			MYBOOST_LOG_INFO( "DecodeRedisGroupsInfo Fail." << OS_KV( "apptable", sAppTable ) ); 
		}
	}
}

void RedisProxyZkData::RedisGroupsChildCallback( 
		const std::string & sPath, const std::vector< std::string > & vecChild,
		void * pCtx, const struct Stat * pStat )
{
	MYBOOST_LOG_INFO( OS_KV( "GroupChildCallback size", vecChild.size() ) );
    std::vector<std::string>vecSortChild(vecChild.begin(),vecChild.end());
    std::sort( vecSortChild.begin(), vecSortChild.end() );
	for( vector< std::string >::const_iterator cit = vecSortChild.begin(); cit != vecSortChild.end(); ++ cit )
	{
		RedisProxyZkData * pZk = static_cast<RedisProxyZkData *>(pCtx);
		std::string sRedisGroupPath = sPath + "/" + (*cit);
		MYBOOST_LOG_INFO( OS_KV("RedisGroupPath", sRedisGroupPath ) );
		pZk->_oZkProcess.watchData( sRedisGroupPath, RedisProxyZkData::RedisGroupDataCallback, pZk );
	}
}

int RedisProxyZkData::TransferAppTableStateStr2Int( string sState )
{
	if( sState == "active" ) return RedisProxyCommon::APPTABLE_STATE_ACTIVE;
	else if( sState == "inactive" ) return RedisProxyCommon::APPTABLE_STATE_INACTIVE;
	return RedisProxyCommon::APPTABLE_STATE_ACTIVE;
}

string RedisProxyZkData::TransferAppTableStateInt2Str( int nState )
{
    string sState = "active";
    switch( nState )
    {
        case RedisProxyCommon::APPTABLE_STATE_ACTIVE:
            sState = "active";
            break;
        case RedisProxyCommon::APPTABLE_STATE_INACTIVE:
            sState = "inactive";
            break;
    }
    return sState;
}

std::string RedisProxyZkData::GetAppTableStateByAppTable( const std::string & sAppTable )
{
    int nState = RedisProxyCommon::APPTABLE_STATE_ACTIVE;
    MapStr2Int::iterator ms2i = _mapAppTable2State.find( sAppTable );
    if( ms2i != _mapAppTable2State.end() )
    {
        nState = ms2i->second;
    }
    return TransferAppTableStateInt2Str( nState );
}

int RedisProxyZkData::DecodeAppTableList(
		const std::string & sAppTableList,
		MapStr2Int & mapAppTable2State )
{
	// 通过JSon解析AppTableList
	try
	{
		JsonValueObjPtr basePtr = taf::JsonValueObjPtr::dynamicCast(TC_Json::getValue(sAppTableList));
		if( !basePtr )
		{
			MYBOOST_LOG_ERROR( "Decode AppTableList Error." );
			return AICommon::RET_FAIL;
		}
		JsonValueArrayPtr AppTableListPtr = taf::JsonValueArrayPtr::dynamicCast( basePtr->value["AppTableList"] );
		if(!AppTableListPtr)
		{
			MYBOOST_LOG_ERROR( "Decode AppTableList Null." );
			return AICommon::RET_FAIL;
		}
		for( size_t i = 0; i < size_t(AppTableListPtr->value.size()); ++ i )
		{
			string sAppTable;
			int nState;
            JsonValueObjPtr AppTableInfoPtr     = taf::JsonValueObjPtr::dynamicCast(AppTableListPtr->value[i]);

            JsonValueStringPtr apptablePtr		= taf::JsonValueStringPtr::dynamicCast(AppTableInfoPtr->value["apptable"]);
            if(apptablePtr){ sAppTable			=  apptablePtr->value; }
            JsonValueStringPtr statePtr			= taf::JsonValueStringPtr::dynamicCast(AppTableInfoPtr->value["state"]);
            if(statePtr){ nState				=  TransferAppTableStateStr2Int( statePtr->value ); }
			mapAppTable2State.insert( std::pair< std::string, int >( sAppTable, nState ) );
			MYBOOST_LOG_INFO( "Decode AppTableList Succ." );
		}
	}
	catch (std::exception &e)
	{
		MYBOOST_LOG_ERROR( "Decode AppTableList Fail." << OS_KV( "msg", e.what() ) );
		return AICommon::RET_FAIL;
	}
	return AICommon::RET_SUCC;
}

void RedisProxyZkData::RedisAppTableListCallback( 
		const std::string & sPath, const std::string & sValue,
		void * pCtx, const struct Stat * pStat )
{
	MYBOOST_LOG_INFO( OS_KV( "RedisAppTableListCallback", sValue ) );
	MapStr2Int mapAppTable2State;
	if( DecodeAppTableList( sValue, mapAppTable2State ) == AICommon::RET_SUCC )
	{
		RedisProxyZkData * pZk = static_cast<RedisProxyZkData *>(pCtx);
		taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( pZk->_olock );
		pZk->_mapAppTable2State.swap( mapAppTable2State );
		MYBOOST_LOG_INFO( OS_KV( "apptable2state size", pZk->_mapAppTable2State.size()));

		for( MapStr2Int::iterator mit = pZk->_mapAppTable2State.begin(); 
                mit != pZk->_mapAppTable2State.end(); ++ mit )
		{
			if( mit->second == RedisProxyCommon::APPTABLE_STATE_ACTIVE )
			{
				InitRedisConf( mit->first, pCtx );
				InitRedisConnPoolInfo( mit->first, pCtx );
				InitRedisGroupsInfo( mit->first, pCtx );
			}
			else if( mit->second == RedisProxyCommon::APPTABLE_STATE_INACTIVE )
			{
				// 如果设置为下线，需要从内存中把配置信息删除
				string sApp							= TC_Common::sepstr<string>( mit->first, "_" )[0];
				string sTable						= TC_Common::sepstr<string>( mit->first, "_" )[1];
				std::string sAppTable				= sApp + "." + sTable;
				MapStr2GIdGroupInfo::iterator msIt	= pZk->_mapAppTable2GidGroupInfo.find( sAppTable );
				if( msIt != pZk->_mapAppTable2GidGroupInfo.end() )
				{
					pZk->_mapAppTable2GidGroupInfo.erase( msIt );
				}
				MapStr2RedisConf::iterator mSt = pZk->_mapAppTable2RedisConf.find( sAppTable );
				if( mSt != pZk->_mapAppTable2RedisConf.end() )
				{
					pZk->_mapAppTable2RedisConf.erase( mSt );
				}
				MapStr2RedisConnPoolConf::iterator mSt1 = pZk->_mapAppTable2RedisConnPoolConf.find( sAppTable );
				if( mSt1 == pZk->_mapAppTable2RedisConnPoolConf.end() )
				{
					pZk->_mapAppTable2RedisConnPoolConf.erase( mSt1 );
				}
				MapStr2SlotConf::iterator mSsc	= pZk->_mapAppTable2SlotConf.find( sAppTable );
				if( mSsc == pZk->_mapAppTable2SlotConf.end() )
				{
					pZk->_mapAppTable2SlotConf.erase( mSsc );
				}
				MapStr2ISlotAction::iterator mS2Isa = pZk->_mapAppTable2ISlotAction.find( sAppTable );
				if( mS2Isa == pZk->_mapAppTable2ISlotAction.end() )
				{
					pZk->_mapAppTable2ISlotAction.erase( mS2Isa );
				}
			}
		}
	}
}

void RedisProxyZkData::InitRedisAppTableList()
{
	string sRedisAppTableListPath = _sRootPath + "/RedisAppTableList";
	_oZkProcess.watchData( sRedisAppTableListPath, RedisProxyZkData::RedisAppTableListCallback, this );
	MYBOOST_LOG_INFO( "InitRedisAppTableList Succ." );
}

void RedisProxyZkData::InitRedisAppTableListByGuard()
{
	string sRedisAppTableListPath = _sRootPath + "/RedisAppTableList";
    string sValue                   = "";
	_oZkProcess.getData( sRedisAppTableListPath, sValue );
	if( DecodeAppTableList( sValue, _mapAppTable2State ) == AICommon::RET_SUCC )
    {
        for( MapStr2Int::iterator mit = _mapAppTable2State.begin(); mit != _mapAppTable2State.end(); ++ mit )
		{
			if( mit->second == RedisProxyCommon::APPTABLE_STATE_ACTIVE )
			{
				InitRedisConfByGuard( mit->first );
				InitRedisConnPoolInfoByGuard( mit->first );
                InitRedisMigrateConfByGuard( mit->first );
                InitRedisGroupsRebalanceByGuard( mit->first );
				InitRedisGroupsInfoByGuard( mit->first );
                // TODO 需要watch的配置
			}
		}
    }
	MYBOOST_LOG_INFO( "InitRedisAppTableListByGuard Succ." );
}

void RedisProxyZkData::InitRedisGroupsRebalanceByGuard( const std::string & sAppTable )
{
	// 扩容情况需要先增加group配置，
	// 缩容情况需要保留group配置，缩容完成再变更
	string sGroupsRebalancePath	= _sRootPath + "/RedisAppTable/" + sAppTable + "/groups_rebalance";
    string sValue               = "";
	_oZkProcess.getData( sGroupsRebalancePath, sValue );
    vector< RedisProxyCommon::GroupInfo > vecSetGroup, vecDelGroup;
    string sApp				= TC_Common::sepstr<string>( sAppTable, "_" )[0];
    string sTable			= TC_Common::sepstr<string>( sAppTable, "_" )[1];
    string sCommaAppTable   = sApp + "." + sTable;
    
    if( DecodeGroupsRebalance( sValue, sCommaAppTable, vecSetGroup, vecDelGroup ) == AICommon::RET_SUCC )
	{
        for( auto vgi = vecSetGroup.begin(); vgi != vecSetGroup.end(); ++ vgi )
        {
            SetGroupData( *vgi );
        }
		auto mat2dgi = _mapAppTable2DelGroupInfo.find( sCommaAppTable );
		if( mat2dgi == _mapAppTable2DelGroupInfo.end() )
		{
			_mapAppTable2DelGroupInfo.insert( 
					std::pair< std::string, vector<RedisProxyCommon::GroupInfo > >(sCommaAppTable, vecDelGroup) );
		}
		else
		{
			mat2dgi->second = vecDelGroup;
		}
    }
}

void RedisProxyZkData::InitRedisGroupsInfoByGuard( const std::string & sSourceAppTable )
{
    // 1. 要获取所有的子路径
    // 2. 把子路径上的KV信息加载到内存中
    // 3. 监听所有子路径，若发生变更时把变更后的内容更新到内存中
    // 4. 需要子节点进行排序处理，因为slot 依赖了 group_*的配置
    string sRedisGroupsPath = _sRootPath + "/RedisAppTable/" + sSourceAppTable + "/groups";
	string sAppTable		= "";
	string sApp				= TC_Common::sepstr<string>( sSourceAppTable, "_" )[0];
	string sTable			= TC_Common::sepstr<string>( sSourceAppTable, "_" )[1];
	sAppTable				= sApp + "." + sTable;

    vector< std::string > vecChild;
    _oZkProcess.getChildren( sRedisGroupsPath, vecChild );
    std::sort( vecChild.begin(), vecChild.end() );
    for( vector< std::string >::const_iterator cit = vecChild.begin(); cit != vecChild.end(); ++ cit )
	{
		std::string sRedisGroupPath = sRedisGroupsPath + "/" + (*cit);
		MYBOOST_LOG_INFO( OS_KV( "RedisGroupPath", sRedisGroupPath ) );
        std::string sValue          = "";
		_oZkProcess.getData( sRedisGroupPath, sValue );
        vector< string > vecGroupsInfo	= TC_Common::sepstr< string >( sRedisGroupPath, "/" );
        string sGroupsNode				= "";
        if( vecGroupsInfo.size() >= size_t(GROUPS_NODE_INDEX + _nDiffIndex) )	
			sGroupsNode = vecGroupsInfo[GROUPS_NODE_INDEX + _nDiffIndex ];
		/*
		if( vecGroupsInfo.size() > size_t(GROUPS_APPTABLE_INDEX + _nDiffIndex) )
		{
			string sTmpAppTable		= vecGroupsInfo[GROUPS_APPTABLE_INDEX + _nDiffIndex];
			string sApp				= TC_Common::sepstr<string>( sTmpAppTable, "_" )[0];
			string sTable			= TC_Common::sepstr<string>( sTmpAppTable, "_" )[1];
			sAppTable				= sApp + "." + sTable;
		}
		*/
		
		if( sGroupsNode == "failover" )
		{
			map< string, RedisProxyCommon::FailOverInfo > mapHost2FailOverInfo;
			if( DecodeRedisFailOverInfos( sValue, sAppTable, mapHost2FailOverInfo ) == AICommon::RET_SUCC )
			{
				_mapHost2FailOverInfo.swap( mapHost2FailOverInfo );
				RedisProxyKafka::getInstance()->UpdateFailOverTopic( _mapHost2FailOverInfo );
			}
		}
		else if( sGroupsNode == "slotconf" )
		{
			SlotConf oSlotConf;
			if( DecodeSlotConf( sValue, oSlotConf ) == AICommon::RET_SUCC )
			{
				MapStr2SlotConf::iterator mSt	= _mapAppTable2SlotConf.find( sAppTable );
				if( mSt == _mapAppTable2SlotConf.end() )
				{
					_mapAppTable2SlotConf.insert( std::pair< string, SlotConf >( sAppTable, oSlotConf ) );
					MYBOOST_LOG_INFO( "Insert SlotConf Succ." << OS_KV( "apptable", sAppTable ) 
							<< OS_KV( "size", _mapAppTable2SlotConf.size() ) ); 
				}
				else
				{
					mSt->second = oSlotConf;
					MYBOOST_LOG_INFO( "Update SlotConf Succ." << OS_KV( "apptable", sAppTable ) 
							<< OS_KV( "size", _mapAppTable2SlotConf.size() ) ); 
				}
				RedisProxySlot::getInstance()->InitSlot2GroupInfoByZkData( sAppTable, oSlotConf.sgroupslot() );
				CRedisConnectPoolFactory_Queue<string>::getInstance()->InitRedisConnectPoolFactoryByGuard();
			}
		}
        else if( sGroupsNode == "slotconf_rebalance" )
        {
            SlotConf oSlotConf;
            if( DecodeSlotConf( sValue, oSlotConf ) == AICommon::RET_SUCC )
            {
                MapStr2SlotConf::iterator mSt	= _mapAppTable2RebalanceSC.find( sAppTable );
                if( mSt == _mapAppTable2RebalanceSC.end() )
                {
                    _mapAppTable2RebalanceSC.insert( std::pair< string, SlotConf >( sAppTable, oSlotConf ) );
                    MYBOOST_LOG_INFO( "Insert Rebalance SlotConf Succ." << OS_KV( "apptable", sAppTable ) 
                            << OS_KV( "size", _mapAppTable2RebalanceSC.size() ) ); 
                }
                else
                {
                    mSt->second = oSlotConf;
                    MYBOOST_LOG_INFO( "Update Rebalance SlotConf Succ." << OS_KV( "apptable", sAppTable ) 
                            << OS_KV( "size", _mapAppTable2RebalanceSC.size() ) ); 
                }
            }

        }
        else if( sGroupsNode == "slotaction" )
		{
			// 需要解析出多个slotaction
			MapInt2SlotAction mapSlotId2SA;
			if( DecodeSlotAction( sValue, mapSlotId2SA ) == AICommon::RET_SUCC )
			{
				MapStr2ISlotAction::iterator mSt	= _mapAppTable2ISlotAction.find( sAppTable );
				if( mSt == _mapAppTable2ISlotAction.end() )
				{
					_mapAppTable2ISlotAction.insert( std::pair< string, MapInt2SlotAction >( sAppTable, mapSlotId2SA ) );
					MYBOOST_LOG_INFO( "Insert SlotAction Succ." << OS_KV( "apptable", sAppTable ) 
							<< OS_KV( "size", _mapAppTable2ISlotAction.size() ) ); 
					RedisProxySlotAction::getInstance()->InitSlotActions( sAppTable, mapSlotId2SA, true );
				}
				else
				{
					mSt->second = mapSlotId2SA;
					MYBOOST_LOG_INFO( "Update SlotAction Succ." << OS_KV( "apptable", sAppTable ) 
							<< OS_KV( "size", _mapAppTable2ISlotAction.size() ) ); 
					RedisProxySlotAction::getInstance()->UpdateSlotActions( sAppTable, mapSlotId2SA );
				}
			}
		}
		else if( sGroupsNode.find ( "group_" ) != string::npos )
        {
            RedisProxyCommon::GroupInfo oGroupInfo;
            oGroupInfo.set_sapptable( sAppTable );
            if( DecodeRedisGroupsInfo( sValue, oGroupInfo ) == AICommon::RET_SUCC )
            {
                int nGroupId			= oGroupInfo.ngroupid();
                MapStr2GIdGroupInfo::iterator msIt = _mapAppTable2GidGroupInfo.find( sAppTable );
                if( msIt != _mapAppTable2GidGroupInfo.end() )
                {
                    MYBOOST_LOG_INFO( "Update GroupsInfo Start." << OS_KV( "apptable", sAppTable ) 
                            << OS_KV( "groupid", nGroupId ) 
                            << OS_KV( "size", _mapAppTable2GidGroupInfo.size() ));
                    MapInt2GroupInfo::iterator miIt = msIt->second.find( nGroupId );
                    if( miIt != msIt->second.end() )
                    {
                        miIt->second = oGroupInfo;
                    }
                    else
                    {
                        msIt->second.insert( std::pair< int, RedisProxyCommon::GroupInfo >( nGroupId, oGroupInfo ) );
                    }
					RedisProxyFailOver::getInstance()->UpdateGroupsInfo( sAppTable, msIt->second );
                }
                else
                {
                    MYBOOST_LOG_INFO( "Insert GroupsInfo Succ." << OS_KV( "apptable", sAppTable ) 
                            << OS_KV( "groupid", nGroupId ) 
                            << OS_KV( "size", _mapAppTable2GidGroupInfo.size() ));
                    MapInt2GroupInfo mapGid2GroupInfo;
                    mapGid2GroupInfo.insert( std::pair< int, RedisProxyCommon::GroupInfo >( nGroupId, oGroupInfo ) );
                    _mapAppTable2GidGroupInfo.insert( 
                            std::pair< string, map< int, RedisProxyCommon::GroupInfo > >( sAppTable, mapGid2GroupInfo ) );
					RedisProxyFailOver::getInstance()->UpdateGroupsInfo( sAppTable, mapGid2GroupInfo );
                }
            }
            else
            {
                MYBOOST_LOG_INFO( "DecodeRedisGroupsInfo Fail." << OS_KV( "apptable", sAppTable ) ); 
            }
        }
	}
	//DelAppTableGroupData( sAppTable );
	MYBOOST_LOG_INFO( "InitRedisGroupsInfoByGuard Succ." );
}

void RedisProxyZkData::InitRedisGroupsInfo( const std::string & sAppTable, void * pCtx )
{
	// 1. 要获取所有的子路径
	// 2. 把子路径上的KV信息加载到内存中
	// 3. 监听所有子路径，若发生变更时把变更后的内容更新到内存中
	RedisProxyZkData * pZk	= static_cast<RedisProxyZkData *>(pCtx);
	string sRedisGroupsPath = pZk->_sRootPath + "/RedisAppTable/" + sAppTable + "/groups";
	pZk->_oZkProcess.watchChildren( sRedisGroupsPath, RedisProxyZkData::RedisGroupsChildCallback, pZk );
	MYBOOST_LOG_INFO( "InitRedisGroupsInfo Succ." << OS_KV( "groupspath", sRedisGroupsPath ) );
}

