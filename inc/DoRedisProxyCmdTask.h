/*************************************************************************
    > File Name: DoProxyCmdTask.h
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 二 11/12 15:30:24 2019
 ************************************************************************/

#ifndef _DoRedisProxyCmdTask_H_
#define _DoRedisProxyCmdTask_H_

#include "AICommon.pb.h"
#include "util/ProcessThreadPool.h"
#include "util/tc_common.h"
#include "RedisProxyBackEnd.h"
#include "redisproxy.pb.h"
#include "ServerDefine.h"
#include "RedisProxySlot.h"
#include "RedisProxyManager.h"
#include "RedisProxyCommon.pb.h"
#include "util/myboost_log.h"

using namespace RedisProxy;
using namespace RedisProxyCommon;
using namespace std;

struct DoRedisProxyCmdTask : public ProcessThreadPoolTask {
public:

    DoRedisProxyCmdTask( const string & sMsgid,
						const RedisProxyInnerRequest & oRedisProxyInnerReq,
						RedisProxyInnerResponse & oRedisProxyInnerRsp,
                       ServerContext * pContext ):
                       _sMsgid( sMsgid ),
                       _oRedisProxyInnerReq( oRedisProxyInnerReq ),
                       _oRedisProxyInnerRsp( oRedisProxyInnerRsp ),
                       _pContext( pContext ) {};

	virtual int HandleProcess()
	{
		// 4.1 组装阶段：按照key分组pipeline请求 
		//   4.1.1 根据key获取到slot，通过slot获取到groupid，根据请求获取到mapSid2GroupId 和 mapGroupId2GroupInfo;
		//   4.1.2 按照Group拆分成对应的pipeline map< groupid, PipeLineRequest > mapGroupId2Pipe;
		// 4.2 按照GroupId获取groupinfo(ip\port\slot\group等) map< groupId, groupinfo > mapGroupInfo
		// 4.3 通过4.2 groupinfo获取链接池中已经建立的链接
		// 4.4 请求响应阶段：枚举所有group中pipeline请求
		// 4.5 获取pipeline请求并且对返回结果进行合并 map< groupid, vector< string > >mapGroupToPipeRsp
		// 4.6 返回rsp结果
		// 4.7 私有对象保存链接上下文
		RedisProxySlot	oRedisProxySlot;	
		MapStr2Int mapSid2GroupId;
		MapInt2GroupInfo mapGroupId2GroupInfo;
		MapInt2PipeLine mapGroupId2Pipeline;
		string sAppName		= _oRedisProxyInnerReq.oappinfo().sappname();	
		string sTableName	= _oRedisProxyInnerReq.oappinfo().stablename();	
		MYBOOST_LOG_DEBUG( OS_KV( "msgid", _sMsgid ) << OS_KV( "appname", sAppName ) << OS_KV( "table", sTableName ) );

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
			MYBOOST_LOG_DEBUG( "PipelineRequest:" << OS_KV( "msgid", _sMsgid ) << oPipeLineRequest.DebugString().c_str() );
		}

		// 请求响应阶段	
		_oRedisProxyInnerRsp.set_smsgid( _sMsgid );
		for( MapInt2PipeLine::iterator mit = mapGroupId2Pipeline.begin(); mit != mapGroupId2Pipeline.end(); ++mit )
		{
			GroupInfo		    oGroupInfo		= mapGroupId2GroupInfo[ mit->first ];
			AppTableInfo		oAppTableInfo;
			RedisProxySlot::getInstance()->GetAppTableInfo( oGroupInfo.sapptable(), oAppTableInfo );
			PipeLineResponse	oPipeLineRsp;
			RedisProxyBackEnd	oRedisProxyBackEnd( _sMsgid, oGroupInfo, oAppTableInfo, mit->second );
			int nRet = oRedisProxyBackEnd.DoPipeLine( oPipeLineRsp );
			MYBOOST_LOG_DEBUG( "DoPipeLine PipeLineRsp " << oPipeLineRsp.DebugString() );
			if( nRet == AICommon::RET_SUCC )
			{
				MYBOOST_LOG_DEBUG( OS_KV("msgid", _sMsgid ) << OS_KV( "dopipeline ret", nRet ) ); 
				RedisProxyManager::InsertPipeLineRsp2RedisProxyInnerRsp( false, mit->second, oPipeLineRsp, 
						_oRedisProxyInnerRsp );
			}
			else
			{
				MYBOOST_LOG_DEBUG( OS_KV("msgid", _sMsgid ) << OS_KV( "dopipeline ret", nRet ) ); 
				RedisProxyManager::InsertPipeLineRsp2RedisProxyInnerRsp( true, 
						mit->second, oPipeLineRsp, _oRedisProxyInnerRsp );
			}

			MYBOOST_LOG_DEBUG( OS_KV("groupidsize", mapGroupId2Pipeline.size() ) 
					<< OS_KV( "redisproxyinnerrsp", _oRedisProxyInnerRsp.DebugString() ) ); 
		}
		return AICommon::RET_SUCC;

	}

	virtual void HandleQueueFull() { MYBOOST_LOG_ERROR( "HandleQueueFull" ); }
	virtual void HandleTimeout() { MYBOOST_LOG_ERROR( "HandleTimeout" ); }
	virtual void HandleErrorMuch() { MYBOOST_LOG_ERROR( "HandleErrorMuch" ); }
	virtual void HandleExp() { MYBOOST_LOG_ERROR( "HandleExp" );  }
	virtual void DestroySelf() { delete this; }
	void LogError(const char *errmsg) const {
	}

private:
	string				_sMsgid;
	ServerContext	*	_pContext;
	RedisProxyInnerRequest	_oRedisProxyInnerReq;
	RedisProxyInnerResponse	_oRedisProxyInnerRsp;

};

#endif // _DoRedisProxyCmdTask_H_
