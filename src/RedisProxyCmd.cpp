/*************************************************************************
    > File Name: RedisProxyCmd.cpp
    > Author: annulsluo

    > Created Time: 一 12/ 2 14:21:29 2019
 ************************************************************************/
#include "AICommon.pb.h"
#include "util/tc_common.h"
#include "RedisProxyBackEnd.h"
#include "redisproxy.pb.h"
#include "ServerDefine.h"
#include "RedisProxySlot.h"
#include "RedisProxyManager.h"
#include "RedisProxyCommon.pb.h"
#include "util/myboost_log.h"
#include "RedisProxyCmd.h"
#include "util/TafOsVar.h"
#include "ServerConfig.h"
#include "DmplmdbInterfaceAgent.h"
#include "RedisProxyFailOver.h"
#include "RedisProxyKafka.h"
#include "RedisProxyPrometheus.h"
#include "RedisProxySlotAction.h"

using namespace RedisProxy;
using namespace RedisProxyCommon;
using namespace std;
using namespace dmp;

RedisProxyCmd::RedisProxyCmd( const string & sMsgid,
		const RedisProxyInnerRequest & oRedisProxyInnerReq ):
	_sMsgid( sMsgid ),
	_oRedisProxyInnerReq( oRedisProxyInnerReq )
{
	// donothing
}

RedisProxyCmd::~RedisProxyCmd()
{

}

// 同步到LMDB
void RedisProxyCmd::ProduceKafka( string sTopicName, const KVOperator & oKvOperReq, string sPartitionKey )
{
	MYBOOST_LOG_DEBUG( OS_KV( "msgid", _sMsgid ) << OS_KV( "KvOperReq", oKvOperReq.DebugString() ) );
	int nMsgLen		= oKvOperReq.ByteSize();
	char * pczMsg	= new char[nMsgLen];
	oKvOperReq.SerializeToArray( pczMsg, nMsgLen );
	MYBOOST_LOG_DEBUG( OS_KV( "msgid", _sMsgid ) << OS_KV( "topicname", sTopicName ) << OS_KV( "msglen", nMsgLen ) );
	int nRet = RedisProxyKafka::getInstance()->Produce( sTopicName, pczMsg, nMsgLen, "", sPartitionKey );
	if( nRet != AICommon::RET_SUCC )
	{
		MYBOOST_LOG_ERROR( OS_KV( "msgid", _sMsgid ) << OS_KV( "topicname", sTopicName ) << OS_KV( "msglen", nMsgLen ) );
	}
	delete [] pczMsg;
}

// 待 redis 节点恢复后进行数据追平
void RedisProxyCmd::ProduceKafka( string sTopicName, const PipeLineRequest & oPipeLineReq, string sHost )
{
    MYBOOST_LOG_DEBUG( OS_KV( "msgid", _sMsgid ) << OS_KV( "PipeLineReq", oPipeLineReq.DebugString() ) );
	int nMsgLen		= oPipeLineReq.ByteSize();
	char * pczMsg	= new char[nMsgLen];
	oPipeLineReq.SerializeToArray( pczMsg, nMsgLen );
	MYBOOST_LOG_DEBUG( OS_KV( "msgid", _sMsgid ) << OS_KV( "topicname", sTopicName ) << OS_KV( "msglen", nMsgLen ) );
	int nRet = RedisProxyKafka::getInstance()->Produce( sTopicName, pczMsg, nMsgLen, sHost );
	if( nRet != AICommon::RET_SUCC )
	{
		MYBOOST_LOG_ERROR( OS_KV( "msgid", _sMsgid ) << OS_KV( "topicname", sTopicName ) << OS_KV( "msglen", nMsgLen ) );
	}
	delete [] pczMsg;
}

int RedisProxyCmd::HandleProcess( RedisProxyInnerResponse & oRedisProxyInnerRsp )
{
	// 4.1 组装阶段：按照key分组pipeline请求 
	//   4.1.1 根据key获取到slot，通过slot获取到groupid，根据请求获取到mapSid2GroupId 和 mapGroupId2GroupInfo;
	//   4.1.2 按照Group拆分成对应的pipeline map< groupid, PipeLineRequest > mapGroupId2Pipe;
	// 4.2 按照GroupId获取groupinfo(ip\port\slot\group等) map< groupId, groupinfo > mapGroupInfo
	// 4.3 通过4.2 groupinfo获取链接池中已经建立的链接
	// 4.4 请求响应阶段：枚举所有group中pipeline请求
	// 4.5 获取pipeline请求并且对返回结果进行合并 map< groupid, vector< string > >mapGroupToPipeRsp
	// 4.6 返回rsp结果

	RedisProxySlot	oRedisProxySlot;	
	MapStr2Int mapSid2GroupId;
	MapInt2GroupInfo mapGroupId2GroupInfo;
	MapInt2PipeLine mapGroupId2Pipeline;
	string sAppName		= _oRedisProxyInnerReq.oappinfo().sappname();	
	string sTableName	= _oRedisProxyInnerReq.oappinfo().stablename();	

	RedisProxySlot::getInstance()->GenSid2GroupInfoByReq( _oRedisProxyInnerReq, 
			mapSid2GroupId, mapGroupId2GroupInfo );

	for( MapInt2GroupInfo::iterator mi2gIt = mapGroupId2GroupInfo.begin(); 
			mi2gIt != mapGroupId2GroupInfo.end(); ++ mi2gIt )
	{
		PipeLineRequest oPipeLineRequest;	
		int nGroupId	= mi2gIt->first;
		for( size_t j = 0; j < size_t(_oRedisProxyInnerReq.oinnerrequestbodylist_size()); ++ j )
		{
			string sSid					= _oRedisProxyInnerReq.oinnerrequestbodylist(j).ssid();
			MapStr2Int::iterator mit	= mapSid2GroupId.find( sSid );
			if( mit != mapSid2GroupId.end() )
			{
				if( mit->second == nGroupId )
				{
					CmdBody			& oCmdBody			= *(oPipeLineRequest.add_ocmdbodylist());
					InnerRequestBody oInnerRequestBody	= _oRedisProxyInnerReq.oinnerrequestbodylist(j);
					RedisProxyManager::InnerRequestBody2CmdBody( oInnerRequestBody, oCmdBody );
				}
			}
		}
		mapGroupId2Pipeline[ nGroupId ] = oPipeLineRequest;
	}

	// 二期：请求响应阶段	pipeline里面多个group的时候需要做并发执行
	oRedisProxyInnerRsp.set_smsgid( _sMsgid );
	for( MapInt2PipeLine::iterator mit = mapGroupId2Pipeline.begin(); mit != mapGroupId2Pipeline.end(); ++mit )
	{
        // 难点和复杂点
        // TODO 先使用rebalance前的映射表判断有没有在迁移中，
        // 1. 如果迁移中则使用备机进行读写
        // 2. 如果未迁移，则使用旧路由&&主机进行读写
        // 3. 如果已经迁移，则使用新路由表&&主机进行读写
        // 4. 什么时候使用rebalance后的映射表？
		int64_t				nStartTime		= TNOWMS;
		GroupInfo		    oGroupInfo		= mapGroupId2GroupInfo[ mit->first ];
		AppTableInfo		oAppTableInfo;
		RedisProxySlot::getInstance()->GetAppTableInfo( oGroupInfo.sapptable(), oAppTableInfo );
		PipeLineResponse	oPipeLineRsp;
		// 增加迁移过程中状态判断，这里欠缺原子性
		if( RedisProxyFailOver::getInstance()->IsGroupOK( oGroupInfo.sapptable(), oGroupInfo.ngroupid() ) 
            && !RedisProxySlotAction::getInstance()->IsGroupMigrate( oGroupInfo.sapptable(), oGroupInfo.ngroupid() ) ) 
		{
			RedisProxyBackEnd	oRedisProxyBackEnd( _sMsgid, oGroupInfo, oAppTableInfo, mit->second );
			int nRet = oRedisProxyBackEnd.DoPipeLine( oPipeLineRsp );
			if( nRet == AICommon::RET_SUCC )
			{
				RedisProxyPrometheus::getInstance()->Report( "Property:PipeLineSucc", mit->second.ocmdbodylist_size() );
				RedisProxyManager::InsertPipeLineRsp2RedisProxyInnerRsp( false, mit->second, oPipeLineRsp, 
						oRedisProxyInnerRsp );
				MYBOOST_LOG_DEBUG( OS_KV( "msgid", _sMsgid ) << OS_KV( "Dopipeline Succ. ret", nRet ) 
						<< OS_KV("RedisProxyInnerRsp", oRedisProxyInnerRsp.DebugString()) ); 
			}
			else
			{
				RedisProxyPrometheus::getInstance()->Report( "Property:PipeLineError", mit->second.ocmdbodylist_size() );
				MYBOOST_LOG_ERROR( OS_KV("msgid", _sMsgid ) << OS_KV( "Dopipeline Error. ret", nRet ) ); 
				RedisProxyManager::InsertPipeLineRsp2RedisProxyInnerRsp( true, 
						mit->second, oPipeLineRsp, oRedisProxyInnerRsp );
			}
		}
        else
        {
            // 迁移阶段/下线阶段和恢复阶段，数据写kafka(fail-topic)
            string sHost = oGroupInfo.omaster().sip() + ":" + TC_Common::tostr(oGroupInfo.omaster().nport());
            PipeLineRequest oFailTopicPipeLineReq;
            if( RedisProxyManager::GenFailTopicPipeLineReq( _sMsgid, mit->second, oFailTopicPipeLineReq ) )
            {
                ProduceKafka( "failtopic", oFailTopicPipeLineReq, sHost );
            }
        }

		// 正常期间，仅将写的数据拆分出来往lmdb-topic发送，避免阻塞  
		if( ServerConfigMng::getInstance()->GetIObj( "transfer2lmdb" ) )
		{
			KVOperator oKvOperReq;
			if( RedisProxyManager::TransferPipelineReq2KvOperReq( 
					_sMsgid, _oRedisProxyInnerReq.oappinfo(), mit->second, oKvOperReq, TRANSFERCMDMARD_ONLY_WRITE ) )
			{
				if( !RedisProxyFailOver::getInstance()->IsGroupFail( oGroupInfo.sapptable(), oGroupInfo.ngroupid() )
                        && !RedisProxySlotAction::getInstance()->IsGroupMigrate( oGroupInfo.sapptable(), oGroupInfo.ngroupid() ) )
				{
                    string sPartitionKey = oGroupInfo.sapptable() + TC_Common::tostr( oGroupInfo.ngroupid() );
					ProduceKafka( "lmdbtopic", oKvOperReq, sPartitionKey );
				}
            }
		}

		if( !RedisProxyFailOver::getInstance()->IsGroupOK( oGroupInfo.sapptable(), oGroupInfo.ngroupid() ) 
                || RedisProxySlotAction::getInstance()->IsGroupMigrate( oGroupInfo.sapptable(), oGroupInfo.ngroupid() ) )
		{
			MYBOOST_LOG_ERROR( "Standby Start." << OS_KV( "msgid", _sMsgid ) 
					<< OS_KV( "apptable", oGroupInfo.sapptable() ) 
					<< OS_KV( "groupid", oGroupInfo.ngroupid() ));
			// 切换为备机的方式进行读取数据，开启备份写数据到KafKa组件
			// 1. 拆分读写请求，避免写数据再发送一份到LMDB，造成重复
			// 2. 对于写请求构造一个默认数据或者空数据
			// 3. 对于读请求由LMDB返回
			KVOperator oKvOperReq;
			KVResponse oKvOperRsp;
			if( RedisProxyManager::TransferPipelineReq2KvOperReq( 
					_sMsgid, _oRedisProxyInnerReq.oappinfo(), mit->second, oKvOperReq, TRANSFERCMDMARD_READ_AND_WRITE ) )
			{
				MYBOOST_LOG_DEBUG( OS_KV( "msgid", _sMsgid ) << OS_KV( "KvOperReq", oKvOperReq.DebugString() ) );

				RedisProxyPrometheus::getInstance()->ReportPlus( "DmplmdbAgent", "Property:Req" );
				int64_t nStartTime	= TNOWMS;
				int nRet			= DmplmdbInterfaceAgent::getInstance()->syncPiplineOper( oKvOperReq, oKvOperRsp );
				int64_t nEndTime	= TNOWMS;
				int64_t nCost		= nEndTime - nStartTime;
				RedisProxyPrometheus::getInstance()->ReportPlus( "DmplmdbAgent", "Property:TotalLatency", nCost );
				if( nRet == AICommon::RET_SUCC )
				{
					RedisProxyPrometheus::getInstance()->ReportPlus( "DmplmdbAgent", "Property:RspSucc" );
					RedisProxyManager::InsertKvOperRsp2RedisProxyInnerRsp( false, mit->second, oKvOperRsp, oRedisProxyInnerRsp );
					MYBOOST_LOG_DEBUG( OS_KV("msgid", _sMsgid ) << OS_KV( "Dmplmdb Succ. ret", nRet ) 
						<< OS_KV( "RedisProxyInnerRsp", oRedisProxyInnerRsp.DebugString()) ); 
				}
				else
				{
					RedisProxyPrometheus::getInstance()->ReportPlus( "DmplmdbAgent", "Property:RspFail" );
					RedisProxyManager::InsertKvOperRsp2RedisProxyInnerRsp( true, mit->second, oKvOperRsp, oRedisProxyInnerRsp );
					RedisProxyPrometheus::getInstance()->Report( "Property:LmdbError", mit->second.ocmdbodylist_size() );
					MYBOOST_LOG_ERROR( OS_KV("msgid", _sMsgid ) << OS_KV( "Dmplmdb Error. ret", nRet ) ); 
				}
			}
			else
			{
				RedisProxyManager::InsertKvOperRsp2RedisProxyInnerRsp( true, mit->second, oKvOperRsp, oRedisProxyInnerRsp );
			}
		}
		int64_t				nEndTime		= TNOWMS;
		int64_t				nCost			= nEndTime - nStartTime;
		int64_t				nDoPipelineTimeout	= ServerConfigMng::getInstance()->GetIObj( "dopipelinetimeout" );

		if( nCost > nDoPipelineTimeout )
		{
			string sAppTableGroupId	= oGroupInfo.sapptable() + "." + TC_Common::tostr( oGroupInfo.ngroupid() );
			MYBOOST_LOG_ERROR( OS_KV("msgid", _sMsgid ) << OS_KV("apptablegroupid", sAppTableGroupId ) 
					<< OS_KV( "realcost", nCost ) << OS_KV( "dopipelinetimeout", nDoPipelineTimeout )
					<< OS_KV( "cmdbody", mit->second.DebugString() ) );
		}
	}
	return AICommon::RET_SUCC;
}
