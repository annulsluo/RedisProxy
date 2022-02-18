/*************************************************************************
    > File Name: RedisProxySlot.cpp
    > Author: annulsluo
    > Mail: annulsluo@gmail.com
    > Created Time: 一 11/11 07:57:46 2019
 ************************************************************************/

#include "ServerConfig.h"
#include "AICommon.pb.h"
#include "redisproxy.pb.h"
#include "RedisProxyCommon.pb.h"
#include "RedisProxySlot.h"
#include "boost/crc.hpp"
#include <vector>
#include <map>
#include "util/TafOsVar.h"
#include "util/myboost_log.h"
#include "util/tc_thread_rwlock.h"
#include "RedisProxyZkData.h"
#include "RedisProxySlotAction.h"

using namespace std;
using namespace AICommon;
using namespace RedisProxy;
using namespace RedisProxyCommon;

int RedisProxySlot::Init() 
{
	// 轮询组合判断App.Table关系 appname=app1|app2|app3 tablename=table1|table2|table3
	// 从配置中解析slotid到groupinfo的关系 
	// 从配置中解析apptableinfo详细信息	
	// apptable详细系如下：
	/*<redis_aiad_userclick>
			redisapp=aiad
			redistable=userclick
			redispasswd=dsp12345
			redisslot=16384
			redisstatus=active
			group_slot=1:1,2:2,3:3:3~16384;
			group_1=172.16.154.51:10020
			group_2=172.16.154.76:10020
			group_3=172.16.154.75:10020
		</redis_aiad_userclick>
	*/
	InitSlot2GroupInfo();
	MYBOOST_LOG_INFO( "RedisProxySlot Init Succ." );
	return 0;
}

void RedisProxySlot::InitAppTable2Slot( MapStr2MStr::iterator m2mstrit )
{
	string sAppTable	= m2mstrit->second["redisapp"] + "." + m2mstrit->second["redistable"];
	RedisProxyCommon::AppTableInfo oAppTableInfo;
	oAppTableInfo.set_sredisapp( m2mstrit->second["redisapp"] );
	oAppTableInfo.set_sredistable( m2mstrit->second["redistable"] ) ;
	oAppTableInfo.set_sredispasswd( m2mstrit->second["redispasswd"] );
	oAppTableInfo.set_nredisslotcnt( TC_Common::strto<int>( m2mstrit->second["redisslot"] ) );
	oAppTableInfo.set_sredisstatus( m2mstrit->second["redisstatus"] );
	oAppTableInfo.set_nredisconnectmaxtrytimes( TC_Common::strto<int>(m2mstrit->second["redisconnectmaxtrytimes"]));
	oAppTableInfo.set_nredistimeoutsec( TC_Common::strto<int>(m2mstrit->second["redistimeoutsec"]) );
	oAppTableInfo.set_nredistimeoutusec( TC_Common::strto<int>(m2mstrit->second["redistimeoutusec"]) );

	RedisConnectPoolConf &oRedisConnectPoolConf = *(oAppTableInfo.mutable_oredisconnectpoolconf());
	oRedisConnectPoolConf.set_nmaxsize( TC_Common::strto<int>(m2mstrit->second["redisconnpoolmaxconnsize"]));
	oRedisConnectPoolConf.set_nminsize( TC_Common::strto<int>(m2mstrit->second["redisconnpoolminconnsize"]));
	oRedisConnectPoolConf.set_nminusedcnt( TC_Common::strto<int>(m2mstrit->second["redisconnpoolminusedcnt"]));
	oRedisConnectPoolConf.set_nmaxidletime( TC_Common::strto<int>(m2mstrit->second["redisconnpoolmaxidletime"]));
	oRedisConnectPoolConf.set_nkeepalivetime( TC_Common::strto<int>(m2mstrit->second["redisconnpoolkeepalivetime"]));
	oRedisConnectPoolConf.set_ncreatesize( TC_Common::strto<int>(m2mstrit->second["redisconnpoolcreatesize"]));
	_mapAppTableInfo.insert( std::pair<string,RedisProxyCommon::AppTableInfo>( sAppTable, oAppTableInfo ) );
}

void RedisProxySlot::InitGroupInfo( 
		MapStr2MStr::iterator m2mstrit,
		const int nGroupId,
		RedisProxyCommon::GroupInfo & oGroupInfo )
{
	// 处理GroupInfo信息
	oGroupInfo.set_ngroupid( nGroupId );
	oGroupInfo.set_sapptable( m2mstrit->second["redisapp"] + "." + m2mstrit->second["redistable"] );
	string sGroupName = "group_" + TC_Common::tostr(nGroupId);
	string sGroupInfo = m2mstrit->second[sGroupName];
	// 默认第一个IP作为Master，一个组多个节点后进行zk选举策略
	vector< string > vecInstance = TC_Common::sepstr< string >( sGroupInfo, "," );
	oGroupInfo.set_ninstancecnt( vecInstance.size() );
	int nInstanceId = 0;
	for( vector< string >::iterator vit = vecInstance.begin(); vit != vecInstance.end(); ++ vit, ++ nInstanceId )
	{
		vector< string> vecIpPort		= TC_Common::sepstr<string>( (*vit), ":" );
		if( vecIpPort.size() == 2 )
		{
			string sIp	= vecIpPort[0];
			int nPort	= TC_Common::strto<int>( vecIpPort[1] );

			RedisProxyCommon::Instance &oInstance = *(oGroupInfo.add_oinstancelist());
			oInstance.set_ninstanceid( nInstanceId );
			oInstance.set_sip( sIp );
			oInstance.set_nport( nPort );
			// 第一个实例作为Master
			if( nInstanceId == 0 )
			{
				Instance & oMaster = *(oGroupInfo.mutable_omaster());
				oMaster.set_ninstanceid( nInstanceId );
				oMaster.set_sip( sIp );
				oMaster.set_nport( nPort );
			}
		}
		else
		{
			MYBOOST_LOG_ERROR( OS_KV("IpPort",(*vit) ) << " Is Empty." );
		}
	}
}

// 存在两种情况
// 1. 服务刚启动时，进行插入操作
// 2. 服务启动后，配置进行变更时，需要进行更新操作
void RedisProxySlot::InitSlot2GroupInfoByZkData( 
		const std::string & sAppTable,
		const std::string & sGroup2Slot )
{
    InsertAppTable2SlotGroupInfo( sAppTable, sGroup2Slot );
    InsertAppTableInfo( sAppTable );
	MYBOOST_LOG_INFO( "RedisProxySlot InitSlot2GroupInfoByZkData Succ." 
            << OS_KV( "apptable", sAppTable )
            << OS_KV( "group2slot", sGroup2Slot ));
}

void RedisProxySlot::InitRebalanceSlot2GroupInfoByZkData( 
		const std::string & sAppTable,
		const std::string & sGroup2Slot )
{
    InsertAppTable2RebalanceSlotGroupInfo( sAppTable, sGroup2Slot );
	MYBOOST_LOG_INFO( "RedisProxySlot InitSlot2RebalanceGroupInfoByZkData Succ." 
            << OS_KV( "apptable", sAppTable )
            << OS_KV( "group2slot", sGroup2Slot ));
}

void RedisProxySlot::InsertAppTableInfo( const std::string & sAppTable )
{
	MYBOOST_LOG_INFO( "InsertAppTable begin." << OS_KV( "apptable", sAppTable ) );
    const RedisConf oRedisConf  
                = RedisProxyZkData::getInstance()->GetRedisConfByAppTable( sAppTable );
    const RedisConnPoolConf oRedisConnPoolConf 
                = RedisProxyZkData::getInstance()->GetRedisConnPoolConfByAppTable( sAppTable );
    const SlotConf  oSlotConf
                = RedisProxyZkData::getInstance()->GetSlotConfByAppTable( sAppTable );
    string sApp     = TC_Common::sepstr< string >( sAppTable, "." )[0];
    string sTable   = TC_Common::sepstr< string >( sAppTable, "." )[1];
    MYBOOST_LOG_INFO( OS_KV("slotconf", oSlotConf.DebugString() ) );

    RedisProxyCommon::AppTableInfo oAppTableInfo;
	oAppTableInfo.set_sredisapp( sApp );
	oAppTableInfo.set_sredistable( sTable ) ;
	oAppTableInfo.set_sredispasswd( oRedisConf._sPasswd );
	oAppTableInfo.set_nredisslotcnt( oSlotConf.nslotcnt() );
    string sLineAppTable = sApp + "_" + sTable;
	oAppTableInfo.set_sredisstatus( RedisProxyZkData::getInstance()->GetAppTableStateByAppTable( sLineAppTable ) );
	oAppTableInfo.set_nredisconnectmaxtrytimes( oRedisConf._nConnectMaxTryTimes );
	oAppTableInfo.set_nredistimeoutsec( oRedisConf._nTimeout.tv_sec );
	oAppTableInfo.set_nredistimeoutusec( oRedisConf._nTimeout.tv_usec );

	RedisConnectPoolConf &oRedisConnectPoolConf = *(oAppTableInfo.mutable_oredisconnectpoolconf());
	oRedisConnectPoolConf.set_nmaxsize( oRedisConnPoolConf._nMaxSize );
	oRedisConnectPoolConf.set_nminsize( oRedisConnPoolConf._nMinSize );
	oRedisConnectPoolConf.set_nminusedcnt( oRedisConnPoolConf._nMinUsedCnt );
	oRedisConnectPoolConf.set_nmaxidletime( oRedisConnPoolConf._nMaxIdleTime );
	oRedisConnectPoolConf.set_nkeepalivetime( oRedisConnPoolConf._nKeepAliveTime );
	oRedisConnectPoolConf.set_ncreatesize( oRedisConnPoolConf._nCreateSize );

    MapStr2AppTableInfo::iterator ms2ati = _mapAppTableInfo.find( sAppTable );
    if( ms2ati != _mapAppTableInfo.end() ) 
    {
        ms2ati->second = oAppTableInfo;
    }
    else
    {
        _mapAppTableInfo.insert( std::pair<std::string, RedisProxyCommon::AppTableInfo>(sAppTable,oAppTableInfo) );
    }
	MYBOOST_LOG_INFO( "InsertAppTableInfo." 
            << OS_KV( "apptableinfosize", _mapAppTableInfo.size() )
            << OS_KV( "apptable", sAppTable ) );
}


void RedisProxySlot::InsertAppTable2SlotGroupInfo( 
		const std::string & sAppTable,
		const std::string & sGroup2Slot )
{
	taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( _olock );
	vector< string > vecGroup2Slot = TC_Common::sepstr<string>( sGroup2Slot, "," ); 

	vector< GroupInfo > vecGroupInfo;
	MapInt2GroupInfo mapSlot2GroupInfo;
	//group_slot=1:1~102|103,2:104~204,3:205~306,4:307~408,5:409~510,6:511~612,7:613~714,8:715~816:9:817~918,10:919~1024
	for( vector< string >::iterator vsit = vecGroup2Slot.begin(); vsit != vecGroup2Slot.end(); ++ vsit )
	{
		vector< string >vecSingleGroupSlot = TC_Common::sepstr<string>( (*vsit), ":" );
		if( vecSingleGroupSlot.size() == 2 )
		{
			int nGroupId = TC_Common::strto<int>( vecSingleGroupSlot[0] );
			// 处理group对应的不连续slot的情况，例如：group_slot=1:1~102|103
			vector< string >vecSingleSlot = TC_Common::sepstr<string>( vecSingleGroupSlot[1], "|" );
			bool bHadSlot = false;
            RedisProxyCommon::GroupInfo oGroupInfo = 
                    RedisProxyZkData::getInstance()->GetGroupInfoByAppTableGid( sAppTable, nGroupId );
            MYBOOST_LOG_INFO( OS_KV( "apptable", sAppTable )
                    << OS_KV( "ngroupid", nGroupId )
                    << OS_KV( "groupinfo", oGroupInfo.DebugString() ) );
			for( vector< string >::iterator vssit = vecSingleSlot.begin(); vssit != vecSingleSlot.end(); ++ vssit )
			{
				// 处理Slot范围
				if( (*vssit).find( "~" ) != string::npos )
				{
					vector< string > vecSlot = TC_Common::sepstr<string>( (*vssit), "~" );
					int nBeginSlot			 = TC_Common::strto<int>( vecSlot[0] );
					int nEndSlot			 = TC_Common::strto<int>( vecSlot[1] );
					for( size_t nSlotId = nBeginSlot; nSlotId <= size_t(nEndSlot); ++ nSlotId )
					{
						mapSlot2GroupInfo.insert(std::pair<int,RedisProxyCommon::GroupInfo>( nSlotId, oGroupInfo ) );
					}
					bHadSlot = true;
				}
				// 处理Slot数值
				else if( !(*vssit).empty() )
				{
					int nSlotId = TC_Common::strto<int>( (*vssit) );
					mapSlot2GroupInfo.insert( std::pair<int,RedisProxyCommon::GroupInfo>( nSlotId, oGroupInfo ) );
					bHadSlot = true;
				}
			}
			if( !bHadSlot )
			{
				MYBOOST_LOG_ERROR( OS_KV("SingleGroupSlot", (*vsit) ) << " Is Empty." );
			}
		}
	}
	// 如果已经存在，则需要进行修改；
	// 如果不存在，则进行插入；
	MapStr2SlotGroupInfo::iterator ms2miIt	= _mapAppTable2SlotGroupInfo.find( sAppTable );
	if( ms2miIt == _mapAppTable2SlotGroupInfo.end() )
	{
		_mapAppTable2SlotGroupInfo.insert(
			std::pair<string,map<int,RedisProxyCommon::GroupInfo> >( sAppTable, mapSlot2GroupInfo ) );
	}
	else 
	{
		ms2miIt->second = mapSlot2GroupInfo;
	}
}
void RedisProxySlot::InsertAppTable2RebalanceSlotGroupInfo( 
		const std::string & sAppTable,
		const std::string & sRebalanceGroup2Slot )
{
	taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( _olock );
	vector< string > vecRebalanceGroup2Slot = TC_Common::sepstr<string>( sRebalanceGroup2Slot, "," ); 

	vector< GroupInfo > vecGroupInfo;
	MapInt2GroupInfo mapSlot2GroupInfo;
	//group_slot=1:1~102|103,2:104~204,3:205~306,4:307~408,5:409~510,6:511~612,7:613~714,8:715~816:9:817~918,10:919~1024
	for( auto vsit = vecRebalanceGroup2Slot.begin(); vsit != vecRebalanceGroup2Slot.end(); ++ vsit )
	{
		vector< string >vecSingleGroupSlot = TC_Common::sepstr<string>( (*vsit), ":" );
		if( vecSingleGroupSlot.size() == 2 )
		{
			int nGroupId = TC_Common::strto<int>( vecSingleGroupSlot[0] );
			// 处理group对应的不连续slot的情况，例如：group_slot=1:1~102|103
			vector< string >vecSingleSlot = TC_Common::sepstr<string>( vecSingleGroupSlot[1], "|" );
			bool bHadSlot = false;
            RedisProxyCommon::GroupInfo oGroupInfo = 
                    RedisProxyZkData::getInstance()->GetGroupInfoByAppTableGid( sAppTable, nGroupId );
            MYBOOST_LOG_INFO( OS_KV( "apptable", sAppTable )
                    << OS_KV( "ngroupid", nGroupId )
                    << OS_KV( "groupinfo", oGroupInfo.DebugString() ) );
			for( vector< string >::iterator vssit = vecSingleSlot.begin(); vssit != vecSingleSlot.end(); ++ vssit )
			{
				// 处理Slot范围
				if( (*vssit).find( "~" ) != string::npos )
				{
					vector< string > vecSlot = TC_Common::sepstr<string>( (*vssit), "~" );
					int nBeginSlot			 = TC_Common::strto<int>( vecSlot[0] );
					int nEndSlot			 = TC_Common::strto<int>( vecSlot[1] );
					for( size_t nSlotId = nBeginSlot; nSlotId <= size_t(nEndSlot); ++ nSlotId )
					{
						mapSlot2GroupInfo.insert(std::pair<int,RedisProxyCommon::GroupInfo>( nSlotId, oGroupInfo ) );
					}
					bHadSlot = true;
				}
				// 处理Slot数值
				else if( !(*vssit).empty() )
				{
					int nSlotId = TC_Common::strto<int>( (*vssit) );
					mapSlot2GroupInfo.insert( std::pair<int,RedisProxyCommon::GroupInfo>( nSlotId, oGroupInfo ) );
					bHadSlot = true;
				}
			}
			if( !bHadSlot )
			{
				MYBOOST_LOG_ERROR( OS_KV("SingleRebalanceGroupSlot", (*vsit) ) << " Is Empty." );
			}
		}
	}
	// 如果已经存在，则需要进行修改；
	// 如果不存在，则进行插入；
	MapStr2SlotGroupInfo::iterator ms2miIt	= _mapAppTable2RebalanceSGI.find( sAppTable );
	if( ms2miIt == _mapAppTable2RebalanceSGI.end() )
	{
		_mapAppTable2RebalanceSGI.insert(
			std::pair<string,map<int,RedisProxyCommon::GroupInfo> >( sAppTable, mapSlot2GroupInfo ) );
	}
	else 
	{
		ms2miIt->second = mapSlot2GroupInfo;
	}
}

void RedisProxySlot::InitSlot2GroupInfo()
{
	for( MapStr2MStr::iterator m2mstrit = ServerConfigMng::getInstance()->GetMapAllRoute().begin();
			m2mstrit != ServerConfigMng::getInstance()->GetMapAllRoute().end(); ++m2mstrit )
	{
		InitAppTable2Slot( m2mstrit );
		string sGroup2Slot = m2mstrit->second["group_slot"];
		vector< string > vecGroup2Slot = TC_Common::sepstr<string>( sGroup2Slot, "," ); 

		vector< GroupInfo > vecGroupInfo;
		MapInt2GroupInfo mapSlot2GroupInfo;
		//group_slot=1:1~102|103,2:104~204,3:205~306,4:307~408,5:409~510,6:511~612,7:613~714,8:715~816:9:817~918,10:919~1024
		for( vector< string >::iterator vsit = vecGroup2Slot.begin(); vsit != vecGroup2Slot.end(); ++ vsit )
		{
			vector< string >vecSingleGroupSlot = TC_Common::sepstr<string>( (*vsit), ":" );
			if( vecSingleGroupSlot.size() == 2 )
			{
				int nGroupId = TC_Common::strto<int>( vecSingleGroupSlot[0] );
				RedisProxyCommon::GroupInfo oGroupInfo;
				InitGroupInfo( m2mstrit, nGroupId, oGroupInfo );
				vecGroupInfo.push_back( oGroupInfo );
				// 处理group对应的不连续slot的情况，例如：group_slot=1:1~102|103
				vector< string >vecSingleSlot = TC_Common::sepstr<string>( vecSingleGroupSlot[1], "|" );
				bool bHadSlot = false;
				for( vector< string >::iterator vssit = vecSingleSlot.begin(); vssit != vecSingleSlot.end(); ++ vssit )
				{
					// 处理Slot范围
					if( (*vssit).find( "~" ) != string::npos )
					{
						vector< string > vecSlot = TC_Common::sepstr<string>( (*vssit), "~" );
						int nBeginSlot			 = TC_Common::strto<int>( vecSlot[0] );
						int nEndSlot			 = TC_Common::strto<int>( vecSlot[1] );
						for( size_t nSlotId = nBeginSlot; nSlotId <= size_t(nEndSlot); ++ nSlotId )
						{
							mapSlot2GroupInfo.insert(std::pair<int,RedisProxyCommon::GroupInfo>( nSlotId, oGroupInfo ) );
						}
						bHadSlot = true;
					}
					// 处理Slot数值
					else if( !(*vssit).empty() )
					{
						int nSlotId = TC_Common::strto<int>( (*vssit) );
						mapSlot2GroupInfo.insert( std::pair<int,RedisProxyCommon::GroupInfo>( nSlotId, oGroupInfo ) );
						bHadSlot = true;
					}
				}
				if( !bHadSlot )
				{
					MYBOOST_LOG_ERROR( OS_KV("SingleGroupSlot", (*vsit) ) << " Is Empty." );
				}
			}
		}
		string sAppTable  = m2mstrit->second["redisapp"] + "." + m2mstrit->second["redistable"];
		_mapAppTable2SlotGroupInfo.insert(
				std::pair<string,map<int,RedisProxyCommon::GroupInfo> >( sAppTable, mapSlot2GroupInfo ) );
	}
}

int RedisProxySlot::GetSlotIdByAppTableKeyReal( 
		const string & sAppTable,
		const string & sKey )
{
	MapStr2SlotGroupInfo::iterator ms2miIt	= _mapAppTable2SlotGroupInfo.find( sAppTable );
	if( ms2miIt != _mapAppTable2SlotGroupInfo.end() )
	{
		MapInt2GroupInfo mapSlotId2GroupInfo = ms2miIt->second;
		boost::crc_32_type oCrcRes;
		oCrcRes.process_bytes( sKey.c_str(), sKey.size() );
		int nSlotId	= oCrcRes.checksum() % _mapAppTableInfo[ sAppTable ].nredisslotcnt() + 1 ;

		if( mapSlotId2GroupInfo.find( nSlotId ) != mapSlotId2GroupInfo.end() )
		{
			return nSlotId;
		}
		else
		{
			MYBOOST_LOG_ERROR( OS_KV( "apptable", sAppTable ) 
				<< OS_KV("key", sKey) << OS_KV( "slot", nSlotId ) << " not in VSlot." );
			return AICommon::RET_INVALID_PARAM;
		}
	}
	else
	{
		MYBOOST_LOG_ERROR( OS_KV("apptable",sAppTable) << " not exist." );
		return AICommon::RET_INVALID_PARAM;
	}
	return AICommon::RET_FAIL;
}

int RedisProxySlot::GetSlotIdByAppTableKey( 
		const string & sAppName,
		const string & sTableName,
		const string & sKey )
{
	if( sAppName.empty() || sTableName.empty() )
	{
		MYBOOST_LOG_ERROR( OS_KV("appname", sAppName ) << OS_KV( "table",sTableName )
			<< OS_KV("key", sKey) << " Is Empty." );
		return AICommon::RET_INVALID_PARAM;
	}
	// app.table 按应用和表名查找是否存在
	string sAppTable				= sAppName + "." + sTableName;
	return GetSlotIdByAppTableKeyReal( sAppTable, sKey );
}

int RedisProxySlot::GetGroupInfoBySlotGroupInfo( 
        const std::string & sAppTable,
        const std::string & sKey,
        MapStr2SlotGroupInfo & mapAppTable2SlotGroupInfo,
        RedisProxyCommon::GroupInfo & oGroupInfo )
{
    MapStr2SlotGroupInfo::iterator ms2miIt	= mapAppTable2SlotGroupInfo.find( sAppTable );

    if( ms2miIt != mapAppTable2SlotGroupInfo.end() )
    {
        boost::crc_32_type oCrcRes;
        oCrcRes.process_bytes( sKey.c_str(), sKey.size() );

        MYBOOST_LOG_INFO( OS_KV("apptable",sAppTable) 
                << OS_KV( "slotcnt", _mapAppTableInfo[sAppTable].nredisslotcnt() ));
        int nSlotId	= oCrcRes.checksum() % _mapAppTableInfo[ sAppTable ].nredisslotcnt() + 1 ;

        MapInt2GroupInfo::iterator mi2gIt	= ms2miIt->second.find( nSlotId );
        if( mi2gIt != ms2miIt->second.end() )
        {
            oGroupInfo = mi2gIt->second;
            MYBOOST_LOG_INFO( OS_KV( "slotid", nSlotId ) << OS_KV( "GroupInfo", oGroupInfo.DebugString() ) );
            return AICommon::RET_SUCC;
        }
        else
        {
            MYBOOST_LOG_ERROR( OS_KV( "apptable", sAppTable )
                    << OS_KV("key", sKey) << OS_KV( "slot", nSlotId ) << " not in VSlot." );
            return AICommon::RET_INVALID_PARAM;
        }
    }
    else
    {
        MYBOOST_LOG_ERROR( OS_KV("apptable",sAppTable) << " not exist." );
        return AICommon::RET_INVALID_PARAM;
    }
    return AICommon::RET_FAIL;
}

int RedisProxySlot::GetGroupInfoByAppTableKey( 
        const string & sAppName,
        const string & sTableName,
        const string & sKey,
        GroupInfo & oGroupInfo )
{
	if( sAppName.empty() || sTableName.empty() )
	{
		MYBOOST_LOG_ERROR( OS_KV("appname", sAppName ) << OS_KV( "table",sTableName )
			<< OS_KV("key", sKey) << " Is Empty." );
		return AICommon::RET_INVALID_PARAM;
	}
	// app.table 按应用和表名查找是否存在
	string sAppTable				= sAppName + "." + sTableName;
    if( !RedisProxySlotAction::getInstance()->IsAppTableRebalance( sAppTable ) )
    {
        MYBOOST_LOG_DEBUG( "IsNotAppTableRebalance GetGroupInfo." 
                << OS_KV( "apptable", sAppTable )
                << OS_KV( "Key", sKey ) );
        return GetGroupInfoBySlotGroupInfo( sAppTable, sKey, _mapAppTable2SlotGroupInfo, oGroupInfo );
    }
    else
    {
        int nRet = GetGroupInfoBySlotGroupInfo( sAppTable, sKey, _mapAppTable2SlotGroupInfo, oGroupInfo );
        if( RedisProxySlotAction::getInstance()->IsGroupMigrate( sAppTable, oGroupInfo.ngroupid() ) )
        {
                MYBOOST_LOG_DEBUG( "IsGroupMigrate GetGroupInfo." 
                        << OS_KV( "apptable", sAppTable )
                        << OS_KV( "groupid", oGroupInfo.ngroupid() )
                        << OS_KV( "Key", sKey ) );
            return nRet;
        }
        else
        {
            if( RedisProxySlotAction::getInstance()->GetGroupMigrateState( sAppTable, oGroupInfo.ngroupid() )
                    == RedisProxyCommon::GROUP_MIGRATE_STATE_FINISH )
            {
                MYBOOST_LOG_DEBUG( "After Rebalance GetGroupInfo." 
                        << OS_KV( "apptable", sAppTable )
                        << OS_KV( "groupid", oGroupInfo.ngroupid() )
                        << OS_KV( "Key", sKey ) );
                return GetGroupInfoBySlotGroupInfo( sAppTable, sKey, _mapAppTable2RebalanceSGI, oGroupInfo );
            }
        }
        return nRet;
    }
	return AICommon::RET_FAIL;
}

int RedisProxySlot::GetSlotIdByKey( const string & sKey )
{
	return 0;
}

int RedisProxySlot::GetGroupIdBySlot( int nSlot )
{
	return 0;
}

int RedisProxySlot::GetGroupIdByKey( const string & sKey )
{
	int nSlot = GetSlotIdByKey(sKey );
	return  GetGroupIdBySlot( nSlot );
}

void RedisProxySlot::GetGroupInfoByAppTableSlot( 
		const string & sAppName,
		const string & sTableName, 
		const int nSlotId, 
		GroupInfo & oGroupInfo )
{
	if( sAppName.empty() || sTableName.empty() )
	{
		MYBOOST_LOG_ERROR( OS_KV("appname",sAppName) << OS_KV( "table", sTableName ) << " Is Empty." ); 
		return;
	}
	// app.table 按应用和表名查找是否存在
	string sAppTable			= sAppName + "." + sTableName;
	MapStr2SlotGroupInfo::iterator ms2miIt	= _mapAppTable2SlotGroupInfo.find( sAppTable );
	if( ms2miIt != _mapAppTable2SlotGroupInfo.end() )
	{
		MapInt2GroupInfo mapSlotId2GroupInfo = ms2miIt->second;
		MapInt2GroupInfo::iterator mi2gIt	= mapSlotId2GroupInfo.find( nSlotId );
		if( mi2gIt != mapSlotId2GroupInfo.end() )
		{
			oGroupInfo = mi2gIt->second;
		}
		else
		{
			MYBOOST_LOG_ERROR( OS_KV("appname",sAppName ) << OS_KV("table", sTableName ) 
					<< OS_KV("slotid", nSlotId ) << " Slot2GroupInfo Empty." );
		}
	}
	else
	{
		MYBOOST_LOG_ERROR( OS_KV("appname",sAppName) << OS_KV( "table", sTableName ) << " AppTable Empty." ); 
	}
}

void RedisProxySlot::GetGroupInfo(  
		const string & sAppName, 
		const string & sTableName,
		const string & sKey, 
		GroupInfo & oGroupInfo )
{
	GetGroupInfoByAppTableKey( sAppName, sTableName, sKey, oGroupInfo );
}

void RedisProxySlot::GenSid2GroupIdByReq( 
		const RedisProxyInnerRequest & oRedisProxyInnerReq,
		MapStr2Int & mapSid2GroupId,
		vector< int > & vecGroupId )
{
	for( size_t i = 0; i < size_t(oRedisProxyInnerReq.oinnerrequestbodylist_size()); ++ i )
	{
		InnerRequestBody oInnerRequestBody	= oRedisProxyInnerReq.oinnerrequestbodylist(i);
		string sKey		= oInnerRequestBody.skey();
		string sSid		= oInnerRequestBody.ssid();
		int nGroupId	= GetGroupIdByKey( sKey );
		mapSid2GroupId[ sSid ] = nGroupId;
		if( find( vecGroupId.begin(), vecGroupId.end(), nGroupId ) == vecGroupId.end() )
		{
			vecGroupId.push_back( nGroupId );
		}
	}
}

// TODO 加入rebalance判断
void RedisProxySlot::GenSid2GroupInfoByReq( 
		const RedisProxyInnerRequest & oRedisProxyInnerReq,
		MapStr2Int & mapSid2GroupId,
		MapInt2GroupInfo & mapGroupId2GroupInfo )
{
	for( size_t i = 0; i < size_t(oRedisProxyInnerReq.oinnerrequestbodylist_size()); ++ i )
	{
		InnerRequestBody oInnerRequestBody	= oRedisProxyInnerReq.oinnerrequestbodylist(i);
		string sKey		= oInnerRequestBody.skey();
		string sSid		= oInnerRequestBody.ssid();
		string sAppName = oRedisProxyInnerReq.oappinfo().sappname();
		string sTableName = oRedisProxyInnerReq.oappinfo().stablename();

		GroupInfo oGroupInfo;
		GetGroupInfo( sAppName, sTableName, sKey, oGroupInfo );
		int nGroupId	= oGroupInfo.ngroupid();
		mapSid2GroupId[ sSid ]				= nGroupId;
		mapGroupId2GroupInfo[ nGroupId ]	= oGroupInfo;
	}
}

void RedisProxySlot::GetAppTableInfo( const string & sAppTable, AppTableInfo & oAppTableInfo )
{
	MapStr2AppTableInfo::iterator mit = _mapAppTableInfo.find( sAppTable );
	if( mit != _mapAppTableInfo.end() )
	{
		oAppTableInfo = mit->second;
	}
}
