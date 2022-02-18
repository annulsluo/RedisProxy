/*************************************************************************
    > File Name: RedisProxyFailOver.cpp
    > Author: annulsluo
    > Mail: annulsluo@gmail.com
    > Created Time: 五  3/20 10:33:16 2020
 ************************************************************************/

#include "RedisProxyFailOver.h"
#include "AICommon.pb.h"
#include "RedisProxyZkData.h"
#include "RedisProxyKafka.h"
#include "RedisProxyPrometheus.h"

void RedisProxyFailOver::Init()
{

}

int RedisProxyFailOver::InitGroupsInfo( 
		MapStr2GIdGroupInfo & mapAppTable2GidGroupInfo )
{
	// 从ZK里面
	return AICommon::RET_SUCC;
}

void RedisProxyFailOver::UpdateGroupsInfo( 
		const std::string & sAppTable,
		MapInt2GroupInfo & mapGid2GroupInfo )
{
	MapStr2GIdGroupInfo::iterator msIt = _mapAppTable2GidGroupInfo.find( sAppTable );
	if( msIt != _mapAppTable2GidGroupInfo.end()  )
	{
		msIt->second = mapGid2GroupInfo;
		MapInt2GroupInfo::iterator mibIt = mapGid2GroupInfo.begin();
		for( ;mibIt != mapGid2GroupInfo.end(); ++ mibIt )
		{
			MYBOOST_LOG_INFO( "Update GroupsInfo Exist." << OS_KV( "apptable", sAppTable ) 
				<< OS_KV( "gid", mibIt->first ) );
			MapInt2GroupInfo::iterator miIt = msIt->second.find( mibIt->first );
			if( miIt != msIt->second.end() )
			{
				miIt->second = mibIt->second;
			}
			else
			{
				msIt->second.insert( std::pair< int, RedisProxyCommon::GroupInfo >( mibIt->first, mibIt->second ) );
			}
		}
	}
	else
	{
		MYBOOST_LOG_INFO( "Update GroupsInfo Succ." << OS_KV( "apptable", sAppTable ) 
				<< OS_KV( "size", mapGid2GroupInfo.size() ) );
		_mapAppTable2GidGroupInfo.insert( 
				std::pair< string, map< int, RedisProxyCommon::GroupInfo > >( sAppTable, mapGid2GroupInfo ) );
	}
		MYBOOST_LOG_INFO( "Update GroupsInfo Succ." << OS_KV( "apptable", sAppTable ) 
				<< OS_KV( "size", _mapAppTable2GidGroupInfo[sAppTable].size() ) );
}


bool RedisProxyFailOver::IsGroupFail( 
		const std::string & sAppTable, 
		const int & nGroupId )
{
	MapStr2GIdGroupInfo::iterator msIt = _mapAppTable2GidGroupInfo.find( sAppTable );
	if( msIt != _mapAppTable2GidGroupInfo.end()  )
	{
		MapInt2GroupInfo::iterator miIt = msIt->second.find( nGroupId );
		if( miIt != msIt->second.end() )
		{
			if( miIt->second.ngroupstate() == RedisProxyCommon::GROUP_STATE_FAIL )
				return true;
		}
	}
	return false;
}

// 主节点使用判断接口
bool RedisProxyFailOver::IsGroupOK(
		const std::string & sAppTable, 
		const int & nGroupId )
{
    // 节点状态正常判断条件：链接正常和数据一致(failtopic数据消费完成)
    string sError;
    MapStr2GIdGroupInfo::iterator msIt = _mapAppTable2GidGroupInfo.find( sAppTable );
	if( msIt != _mapAppTable2GidGroupInfo.end()  )
	{
		MapInt2GroupInfo::iterator miIt = msIt->second.find( nGroupId );
		if( miIt != msIt->second.end() )
		{
			if( miIt->second.ngroupstate() == RedisProxyCommon::GROUP_STATE_OK )
            {
                string sHost = miIt->second.omaster().sip() + ":" + TC_Common::tostr(miIt->second.omaster().nport());
                // failtopic消费完成会更新到zk中
                MapStr2FailOverInfo mapHost2FailOverInfo = RedisProxyZkData::getInstance()->GetHost2FailOverInfos();
                if( mapHost2FailOverInfo.find( sHost ) == mapHost2FailOverInfo.end() )
                    return true;
                else
                    sError = "failtopic " + sHost;
            }
            else
            {
                sError = "group state is " + TC_Common::tostr(miIt->second.ngroupstate());
            }
		}
        else
        {
            sError = "GroupId Not Found.";
        }
	}
    else
    {
        sError = "AppTable Not Found.";
    }
    MYBOOST_LOG_ERROR( "Group Not OK." 
            << OS_KV( "apptable", sAppTable ) 
            << OS_KV( "groupid",nGroupId ) 
            << "|" << sError );
    return false;
}

int RedisProxyFailOver::GetGroupState(
        const std::string & sAppTable, 
        const int & nGroupId )
{
	MapStr2GIdGroupInfo::iterator msIt = _mapAppTable2GidGroupInfo.find( sAppTable );
	if( msIt != _mapAppTable2GidGroupInfo.end()  )
	{
		MapInt2GroupInfo::iterator miIt = msIt->second.find( nGroupId );
		if( miIt != msIt->second.end() )
		{
			return miIt->second.ngroupstate();
		}
	}
	return -1;
}

RedisProxyCommon::FailOverInfo & RedisProxyFailOver::GenFailOverInfo( 
		RedisProxyCommon::GroupInfo & oGroupInfo, 
		RedisProxyCommon::FailOverInfo & oFailOverInfo,
		int nPartition )
{
	//RedisProxyCommon::FailOverInfo oFailOverInfo;
	oFailOverInfo.set_ninstanceid( oGroupInfo.omaster().ninstanceid() );
	oFailOverInfo.set_shost( oGroupInfo.omaster().sip() + ":" + TC_Common::tostr(oGroupInfo.omaster().nport()) );
	oFailOverInfo.set_ngroupid( oGroupInfo.ngroupid() );
	oFailOverInfo.set_ngroupstate( oGroupInfo.ngroupstate() );
	oFailOverInfo.set_stopic( FAILOVER_TOPIC );
	oFailOverInfo.set_npartition( nPartition );		
	oFailOverInfo.set_slftime( oGroupInfo.omaster().slftime() );
    oFailOverInfo.set_sapptable( oGroupInfo.sapptable() );
	return oFailOverInfo;
}

// 只有 Guard 才会去调用
int RedisProxyFailOver::UpdateGroupState(
		const std::string & sAppTable, 
		const int nGroupId,
		int nGroupState )
{
	MYBOOST_LOG_DEBUG( "GroupStateChange Begin." << OS_KV( "apptable", sAppTable ) 
			<< OS_KV( "groupid", nGroupId ) << OS_KV( "size", _mapAppTable2GidGroupInfo.size() )
			<< OS_KV( "groupstate", nGroupState ) );
	MapStr2GIdGroupInfo::iterator msIt = _mapAppTable2GidGroupInfo.find( sAppTable );
	if( msIt != _mapAppTable2GidGroupInfo.end()  )
	{
		MYBOOST_LOG_DEBUG( "GroupStateChange In." << OS_KV( "apptable", sAppTable ) );
		MapInt2GroupInfo::iterator miIt = msIt->second.find( nGroupId );
		if( miIt != msIt->second.end() )
		{
			RedisProxyCommon::GroupInfo & oGroupInfo = miIt->second;
			if( miIt->second.ngroupstate() != nGroupState )
			{
				RedisProxyCommon::Instance & oMaster = *(oGroupInfo.mutable_omaster());
				oMaster.set_ninstancestate( nGroupState );
				oMaster.set_slftime( TC_Common::now2str("%Y-%m-%d %H:%M:%S") );

				miIt->second.set_ngroupstate( nGroupState );
				std::string sTableName = TC_Common::replace( sAppTable, ".", "" );
				RedisProxyPrometheus::getInstance()->ReportPlusGauge( "RedisConnPoolGauge", 
						sTableName+":groupstate", nGroupState );
				if( RedisProxyZkData::getInstance()->SetGroupData( miIt->second ) )
				{
					MYBOOST_LOG_INFO( "GroupStateChange Succ." 
							<< OS_KV( "apptable", sAppTable ) << OS_KV( "groupid", nGroupId ) 
							<< OS_KV( "groupstate", nGroupState ));
					if( nGroupState == RedisProxyCommon::GROUP_STATE_FAIL )
					{
						// 更新FailOver到Zk中以便Proxy感知
						// 更新Kafka中的节点失败状态以便Guard 进行失败节点消费
						int nPartition = RedisProxyKafka::getInstance()->GetIdlePartition();
						RedisProxyCommon::FailOverInfo oFailOverInfo;
						oFailOverInfo = GenFailOverInfo( miIt->second, oFailOverInfo, nPartition );
						if( RedisProxyZkData::getInstance()->SetFailOvers( oFailOverInfo ) )
						{
							MYBOOST_LOG_INFO( "SetFailOvers Succ." 
								<< OS_KV( "apptable", sAppTable ) << OS_KV( "groupid", nGroupId ) 
								<< OS_KV( "groupstate", nGroupState ) << OS_KV( "partition", nPartition ) );
						}
						else
						{
							MYBOOST_LOG_INFO( "SetFailOvers fail." 
								<< OS_KV( "apptable", sAppTable ) << OS_KV( "groupid", nGroupId ) 
								<< OS_KV( "groupstate", nGroupState ) << OS_KV( "partition", nPartition ) );
						}
					}
					return AICommon::RET_SUCC;
				}
				else 
				{
					MYBOOST_LOG_ERROR( "GroupStateChange Fail." 
							<< OS_KV( "apptable", sAppTable ) << OS_KV( "groupid", nGroupId ) 
							<< OS_KV( "groupstate", nGroupState ));
					return AICommon::RET_FAIL;
				}
			}
		}
	}
	return AICommon::RET_SUCC;
}

bool RedisProxyFailOver::IsInstanceFail( 
		const std::string & sAppTable, 
		const int & nGroupId,
		const std::string & Ip, 
		const int & nPort )
{
	return false;
}

void RedisProxyFailOver::run()
{
	/*
	while( 1 )
	{

	}
	*/
}

