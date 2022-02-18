/*************************************************************************
    > File Name: RedisProxyManager.cpp
    > Author: annulsluo
    > Mail: annulsluo@gmail.com
    > Created Time: 四 11/ 7 15:07:35 2019
 ************************************************************************/

#include "RedisProxyManager.h"
#include <grpc/impl/codegen/log.h>
#include "RedisProxyCommon.pb.h"
#include "DoRedisProxyCmdTask.h"
#include "util/ProcessThreadPool.h"
#include "ServerDefine.h"
#include "util/common.h"
#include "util/TafOsVar.h"
#include "util/myboost_log.h"
#include "RedisProxyCmd.h"
#include "DmpService.pb.h"
#include "ServerConfig.h"
#include "DmplmdbInterfaceAgent.h"
#include "RedisProxyZkData.h"

using namespace std;
using namespace RedisProxy;
using namespace RedisProxyCommon;

RedisProxyManager::RedisProxyManager()
{
}

RedisProxyManager::~RedisProxyManager()
{

}

void RedisProxyManager::TransferRedisProxyReq2InnerReq(
		const string & sMsgid,
		const RedisProxyRequest & oRedisProxyReq, 
		RedisProxyInnerRequest & oRedisProxyInnerReq )
{
	AppInfo & oAppInfo = *(oRedisProxyInnerReq.mutable_oappinfo());
	oAppInfo.set_sappid( oRedisProxyReq.sappid() );
	oAppInfo.set_sapppw( oRedisProxyReq.sapppw() );
	oAppInfo.set_sredisid( "" );			
	oAppInfo.set_sredispw( "" );			
	oAppInfo.set_sappname( oRedisProxyReq.sappname() );			
	oAppInfo.set_stablename( oRedisProxyReq.stablename() );
	
	RequestHead & oReqHead = *(oRedisProxyInnerReq.mutable_orequesthead());
	oReqHead.set_ncallmode( oRedisProxyReq.orequesthead().ncallmode() );
	oReqHead.set_ncalltype( oRedisProxyReq.orequesthead().ncalltype() );
	oRedisProxyInnerReq.set_smsgid( sMsgid );

	for( size_t i = 0; i < size_t(oRedisProxyReq.orequestbodylist_size()); ++ i )
	{
		const RedisProxy::RequestBody & oRequestBody		= oRedisProxyReq.orequestbodylist(i);
		InnerRequestBody & oInnerRequestBody	= *(oRedisProxyInnerReq.add_oinnerrequestbodylist());
		// 区分不同版本获取Key的方法
		if( oRequestBody.skey().size() != 0 && oRequestBody.nver() == VERSION_KV )
		{
			oInnerRequestBody.set_skey( oRequestBody.skey() );
		}
		else if( oRequestBody.nver() == VERSION_CMD )
		{
			vector< string > vecItem;
			vecItem = TC_Common::sepstr<string>( oRequestBody.scmd(), string(1,' ') );
			if( vecItem.size() > 1 ) oInnerRequestBody.set_skey( 
					vecItem[CMD_KEY_INDEX].c_str(), vecItem[CMD_KEY_INDEX].size() );
		}
		else if( oRequestBody.nver() == VERSION_ELEMENTVECTOR )
		{
			if( oRequestBody.orequestelementlist_size() >= ELE_KEY_INDEX )
			{
				const RedisProxy::RequestElement & oRequestElement	
					= oRequestBody.orequestelementlist(ELE_KEY_INDEX);
				oInnerRequestBody.set_skey( oRequestElement.srequestelement().c_str(), 
						oRequestElement.srequestelement().size() );
			}
		}
		oInnerRequestBody.set_scmd( oRequestBody.scmd() );
		oInnerRequestBody.set_svalue( oRequestBody.svalue().c_str(), oRequestBody.svalue().size() );
		oInnerRequestBody.set_ssid( ::GenerateUUID() );			// 创建session 
		oInnerRequestBody.set_nver( oRequestBody.nver() );
		
		// 向量元素拷贝到内部结构中
		for( size_t j = 0; j < size_t(oRequestBody.orequestelementlist_size()); ++ j )
		{
			const RedisProxy::RequestElement & oRequestElement	= oRequestBody.orequestelementlist(j);
			RedisProxy::RequestElement & oInnerRequestElement	= *( oInnerRequestBody.add_orequestelementlist() );
			oInnerRequestElement.set_nrequestelementtype( oRequestElement.nrequestelementtype() );
			oInnerRequestElement.set_nchartype( oRequestElement.nchartype() );
			oInnerRequestElement.set_srequestelement( oRequestElement.srequestelement().c_str(), 
					oRequestElement.srequestelement().size() );
		}
	}
}

bool RedisProxyManager::GenFailTopicPipeLineReq(
        const string & sMsgid,
        const PipeLineRequest & oPipeLineReq,
        PipeLineRequest & oFailTopicPipeLineReq )
{
    bool bGenFailTopic = false;
	for( size_t i = 0; i < size_t(oPipeLineReq.ocmdbodylist_size()); ++ i )
    {
		const RedisProxyCommon::CmdBody & oCmdBody	        = oPipeLineReq.ocmdbodylist(i);
        if( oCmdBody.skey().size() != 0 && oCmdBody.nver() == VERSION_KV )
		{
			// donothing
		}
		else if( oCmdBody.nver() == VERSION_CMD )
		{
			vector< string > vecItem;
			vecItem = TC_Common::sepstr<string>( oCmdBody.scmd(), string(1,' ') );
            if( vecItem.size() > 0 )
            {
                string sCmd( vecItem[0].c_str(), vecItem[0].size() );
                if( IsWriteCmd( sCmd ) )
                {
                    bGenFailTopic = true;
                    RedisProxyCommon::CmdBody & oFailTopicCmdBody	= *(oFailTopicPipeLineReq.add_ocmdbodylist());
                    oFailTopicCmdBody.set_skey( oCmdBody.skey().c_str(), oCmdBody.skey().size() );
                    oFailTopicCmdBody.set_scmd( oCmdBody.scmd().c_str(), oCmdBody.scmd().size() );
                    oFailTopicCmdBody.set_svalue( oCmdBody.svalue().c_str(), oCmdBody.svalue().size() );
                    oFailTopicCmdBody.set_ssid( oCmdBody.ssid() );
                    oFailTopicCmdBody.set_nver( oCmdBody.nver() );
                }
            }
		}
		else if( oCmdBody.nver() == VERSION_ELEMENTVECTOR )
        {
            string sCmd         = "";
			for( size_t j = 0; j < size_t(oCmdBody.orequestelementlist_size()); ++ j )
			{
				const RedisProxy::RequestElement & oRequestElement	= oCmdBody.orequestelementlist(j);
                if( j == 0 )
                {
					sCmd.assign( 
                            oRequestElement.srequestelement().c_str(), oRequestElement.srequestelement().size() );
                    sCmd    = TC_Common::lower( sCmd );
                    if( IsWriteCmd( sCmd ) )
                    {
                        bGenFailTopic = true;
                        const RedisProxy::RequestElement & oCmdBodyElement    = oCmdBody.orequestelementlist(j);
                        RedisProxyCommon::CmdBody & oFailTopicCmdBody	= *(oFailTopicPipeLineReq.add_ocmdbodylist());
                        RedisProxy::RequestElement & oFailTopicElement  = *(oFailTopicCmdBody.add_orequestelementlist());
                        oFailTopicElement.set_nrequestelementtype( oCmdBodyElement.nrequestelementtype() );
                        oFailTopicElement.set_nchartype( oCmdBodyElement.nchartype() );
                        oFailTopicElement.set_srequestelement( oCmdBodyElement.srequestelement().c_str(),
                                oCmdBodyElement.srequestelement().size() );
                        oFailTopicCmdBody.set_skey( oCmdBody.skey().c_str(), oCmdBody.skey().size() );
                        oFailTopicCmdBody.set_scmd( oCmdBody.scmd().c_str(), oCmdBody.scmd().size() );
                        oFailTopicCmdBody.set_svalue( oCmdBody.svalue().c_str(), oCmdBody.svalue().size() );
                        oFailTopicCmdBody.set_ssid( oCmdBody.ssid() );
                        oFailTopicCmdBody.set_nver( oCmdBody.nver() );
                    }
                }
            }
        }
    }
    return bGenFailTopic;
}

void RedisProxyManager::TransferRedisProxyAppInfo2LmdbAppInfo(
		const RedisProxyCommon::AppInfo & oReqAppInfo,
		KVOperator & oKvOperReq )
{
	const RedisProxyCommon::AppInfo & oAppInfo = 
			RedisProxyZkData::getInstance()->GetAppInfoByAppId( oReqAppInfo.sappid() );
	oKvOperReq.set_sappid( oAppInfo.slmdbid() );
	oKvOperReq.set_sapppw( oAppInfo.slmdbpw() );
	oKvOperReq.set_sappname( oAppInfo.sappname() );
	oKvOperReq.set_stablename( oAppInfo.stablename() );
}

bool RedisProxyManager::TransferPipelineReq2KvOperReq(
		const string & sMsgid,
		const RedisProxyCommon::AppInfo & oAppInfo,
		const PipeLineRequest & oPipeLineReq,
		KVOperator & oKvOperReq,
		int32_t nTransferCmdMark )
{
	bool bTransferSucc = false;
	TransferRedisProxyAppInfo2LmdbAppInfo( oAppInfo, oKvOperReq );
	for( size_t i = 0; i < size_t(oPipeLineReq.ocmdbodylist_size()); ++ i )
	{
		const RedisProxyCommon::CmdBody & oCmdBody	= oPipeLineReq.ocmdbodylist(i);
		dmp::RequestBody & oKvRequestBody			= *(oKvOperReq.add_orequestbodylist());
		if( oCmdBody.skey().size() != 0 && oCmdBody.nver() == VERSION_KV )
		{
			// donothing
		}
		else if( oCmdBody.nver() == VERSION_CMD )
		{
			vector< string > vecItem;
			vecItem = TC_Common::sepstr<string>( oCmdBody.scmd(), string(1,' ') );
			// 解释对应的字段匹配ElementType，因为在LMDB解释不了
			if( TransferCmd2KvElement( vecItem, oKvRequestBody, nTransferCmdMark ) )
				bTransferSucc = true;
		}
		else if( oCmdBody.nver() == VERSION_ELEMENTVECTOR )
		{
			// 向量元素拷贝到Dmp内部结构中，只有二进制数据使用该版本
			bool bIsFilterCmd   = false;
            string sCmd         = "";
			for( size_t j = 0; j < size_t(oCmdBody.orequestelementlist_size()); ++ j )
			{
				const RedisProxy::RequestElement & oRequestElement	= oCmdBody.orequestelementlist(j);
				dmp::RequestElement & oDmpRequestElement	= *( oKvRequestBody.add_orequestelementlist() );
                if( j == 0 )
                {
                    oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_CMD );
                }
                else if( j == 1 )
                {
                    oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_KEY );
                }
                else if( j >= 2 )
                {
                    if( sCmd == "setex" )
                    {
                        if( j % 2 == 0 )
                        {
                            oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_TIMEOUT );
                        }
                        else 
                        {
                            oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
                        }
                    }
                }
				oDmpRequestElement.set_nchartype( oRequestElement.nchartype() );
				oDmpRequestElement.set_srequestelement( oRequestElement.srequestelement().c_str(), 
						oRequestElement.srequestelement().size() );
				if( j == 0 )
				{
					sCmd.assign( oRequestElement.srequestelement().c_str(), oRequestElement.srequestelement().size() );
                    sCmd    = TC_Common::lower( sCmd );
					if( nTransferCmdMark & TRANSFERCMDMARD_ONLY_WRITE )
					{
						if( !IsWriteCmd( sCmd ) )
						{
							bIsFilterCmd = true;
							break;
						}
					}
					else if( nTransferCmdMark & TRANSFERCMDMARD_ONLY_READ )
					{
						if( IsWriteCmd( sCmd ) )
						{
							bIsFilterCmd = true;
							break;
						}
					}
				}
			}
			if( bIsFilterCmd ) continue;
			bTransferSucc = true;
		}
	}
	return bTransferSucc;
}

void RedisProxyManager::InnerRequestBody2CmdBody( 
		const InnerRequestBody & oInnerRequestBody, 
		CmdBody & oCmdBody )
{
	string sCmd	= "";
	switch( oInnerRequestBody.nver() )
	{
		case RedisProxy::VERSION_KV:
			sCmd = 
				oInnerRequestBody.scmd() + " " + oInnerRequestBody.skey() + " " + oInnerRequestBody.svalue();
			break;
		case RedisProxy::VERSION_CMD:
			sCmd = oInnerRequestBody.scmd();
			break;
		case RedisProxy::VERSION_ELEMENTVECTOR:
			for( size_t i = 0; i < size_t(oInnerRequestBody.orequestelementlist_size()); ++ i )
			{
				const RedisProxy::RequestElement & oInnerRequestElement	= oInnerRequestBody.orequestelementlist(i);
				RedisProxy::RequestElement & oCmdBodyElement		= *(oCmdBody.add_orequestelementlist());
				oCmdBodyElement.set_nrequestelementtype( oInnerRequestElement.nrequestelementtype() );
				oCmdBodyElement.set_nchartype( oInnerRequestElement.nchartype() );
				oCmdBodyElement.set_srequestelement( oInnerRequestElement.srequestelement().c_str(), 
						oInnerRequestElement.srequestelement().size() );
			}
		default:
			break;
	}

	oCmdBody.set_skey( oInnerRequestBody.skey().c_str(), oInnerRequestBody.skey().size() );
	oCmdBody.set_scmd( sCmd );
	oCmdBody.set_svalue( oInnerRequestBody.svalue().c_str(), oInnerRequestBody.svalue().size() );
	oCmdBody.set_ssid( oInnerRequestBody.ssid() );
	oCmdBody.set_nver( oInnerRequestBody.nver() );
}

void RedisProxyManager::InsertPipeLineRsp2RedisProxyInnerRsp( 
		bool bIsEmptyRsp,
		const PipeLineRequest & oPipeLineReq,
		const PipeLineResponse & oPipeLineRsp, 
		RedisProxyInnerResponse & oRedisProxyInnerRsp )
{
	MYBOOST_LOG_INFO( "pipelinersp:" << oPipeLineRsp.DebugString() );
	if( !bIsEmptyRsp )
	{
		::google::protobuf::Map< string, RedisProxyInnerResult >&mapInnerResult = 
					*(oRedisProxyInnerRsp.mutable_mapredisproxyinnerresult() );

		for( size_t i = 0; i < size_t( oPipeLineRsp.opipelineresultlist_size() ); ++ i )
		{
			const PipeLineResult & oPipeLineResult = oPipeLineRsp.opipelineresultlist(i);
			
			RedisProxyInnerResult oRedisProxyInnerResult;
			oRedisProxyInnerResult.set_nret( oPipeLineResult.nret() );
			oRedisProxyInnerResult.set_sresult( oPipeLineResult.sresult() );
			oRedisProxyInnerResult.set_nresulttype( oPipeLineResult.nresulttype() );
			if( size_t(oPipeLineReq.ocmdbodylist_size()) > i )
			{
				oRedisProxyInnerResult.set_nver( oPipeLineReq.ocmdbodylist(i).nver() );
			}
			else
			{
				oRedisProxyInnerResult.set_nver( oPipeLineResult.nver() );
			}
			// 构造回报向量元素
			for( size_t j = 0; j < size_t( oPipeLineResult.oresultelementlist_size() ); ++ j )
			{
				const RedisProxy::ResultElement & oPipeResultElement = oPipeLineResult.oresultelementlist(j);
				
				RedisProxy::ResultElement &oInnerResultElement = *( oRedisProxyInnerResult.add_oresultelementlist() );
				oInnerResultElement.set_nret( oPipeResultElement.nret() );
				oInnerResultElement.set_nresulttype( oPipeResultElement.nresulttype() );
				oInnerResultElement.set_sresultelement( oPipeResultElement.sresultelement().c_str(),
						oPipeResultElement.sresultelement().size() );
			}

			mapInnerResult.insert( 
				::google::protobuf::MapPair< string, RedisProxyInnerResult >( oPipeLineResult.ssid(), oRedisProxyInnerResult ));
		}
	}
	else
	{
		//预设置大小并且内容为空，而不用去Add
		::google::protobuf::Map< string, RedisProxyInnerResult >&mapInnerResult = 
					*(oRedisProxyInnerRsp.mutable_mapredisproxyinnerresult() );
		for( size_t i = 0; i < size_t(oPipeLineReq.ocmdbodylist_size()); ++ i )
		{
			RedisProxyInnerResult oRedisProxyInnerResult;
			oRedisProxyInnerResult.set_nver( oPipeLineReq.ocmdbodylist(i).nver() );
			mapInnerResult.insert( 
				::google::protobuf::MapPair< string, RedisProxyInnerResult >( oPipeLineReq.ocmdbodylist(i).ssid(), oRedisProxyInnerResult ));
		}
	}
}

void RedisProxyManager::InsertKvOperRsp2RedisProxyInnerRsp(
			bool bIsEmptyRsp,
			const PipeLineRequest & oPipeLineReq,
			const KVResponse & oKvOperRsp,
			RedisProxyInnerResponse & oRedisProxyInnerRsp )
{
	MYBOOST_LOG_INFO( "KvOperrsp:" << oKvOperRsp.DebugString() );
	if( !bIsEmptyRsp )
	{
		::google::protobuf::Map< string, RedisProxyInnerResult >&mapInnerResult = 
					*(oRedisProxyInnerRsp.mutable_mapredisproxyinnerresult() );

		for( size_t i = 0; i < size_t( oKvOperRsp.oresultbodylist_size() ); ++ i )
		{
			const dmp::ResultBody & oKvOperResult = oKvOperRsp.oresultbodylist(i);
			
			RedisProxyInnerResult oRedisProxyInnerResult;
			oRedisProxyInnerResult.set_nret( oKvOperResult.nret() );
            if( oKvOperResult.nret() < dmp::RESULT_SUCC )
            {
                oRedisProxyInnerResult.set_sresult( "" );
            }
            else oRedisProxyInnerResult.set_sresult( oKvOperResult.svalue() );
			oRedisProxyInnerResult.set_nresulttype( oKvOperResult.nresulttype() );
			if( size_t(oPipeLineReq.ocmdbodylist_size()) > i )
			{
				oRedisProxyInnerResult.set_nver( oPipeLineReq.ocmdbodylist(i).nver() );
			}
			else
			{
				oRedisProxyInnerResult.set_nver( oKvOperResult.nver() );
			}
			// 构造回报向量元素
			for( size_t j = 0; j < size_t( oKvOperResult.oarrayresultlist_size() ); ++ j )
			{
				const dmp::ArrayResult & oArrayResult = oKvOperResult.oarrayresultlist(j);
				
				for( size_t k = 0; k < size_t( oArrayResult.oresultelementlist_size() ); ++ k )
				{
                    const dmp::ResultElement &oResultElement = oArrayResult.oresultelementlist(k);
                    // 保持LMDB和redis返回数据一致性
                    if( oResultElement.nresultelementtype() == dmp::REQUESTELEMENTTYPE_VALUE ) continue;
					RedisProxy::ResultElement &oInnerResultElement = *( oRedisProxyInnerResult.add_oresultelementlist() );
					oInnerResultElement.set_nret( oArrayResult.nret() );
					oInnerResultElement.set_nresulttype( oResultElement.nresulttype() );
					oInnerResultElement.set_sresultelement( oResultElement.sresultelement().c_str(),
							oResultElement.sresultelement().size() );
				}
			}

			mapInnerResult.insert( 
				::google::protobuf::MapPair< string, RedisProxyInnerResult >( oPipeLineReq.ocmdbodylist(i).ssid(), oRedisProxyInnerResult ));
		}
	}
	else
	{
		// 预设置大小并且内容为空，而不用去Add
		::google::protobuf::Map< string, RedisProxyInnerResult >&mapInnerResult = 
					*(oRedisProxyInnerRsp.mutable_mapredisproxyinnerresult() );
		for( size_t i = 0; i < size_t(oPipeLineReq.ocmdbodylist_size()); ++ i )
		{
			RedisProxyInnerResult oRedisProxyInnerResult;
			oRedisProxyInnerResult.set_nver( oPipeLineReq.ocmdbodylist(i).nver() );
			mapInnerResult.insert( 
				::google::protobuf::MapPair< string, RedisProxyInnerResult >( oPipeLineReq.ocmdbodylist(i).ssid(), oRedisProxyInnerResult ));
		}
	}
}

void RedisProxyManager::TransferInnerRsp2sRedisProxyRsp( 
		const RedisProxyInnerRequest & oInnerReq,
		const RedisProxyInnerResponse & oInnerRsp,
		RedisProxyResponse & oRedisProxyRsp )
{
	// 1. 按照InnerReq遍历每个ResultBody
	// 2. 依据Item中的sid查找InnerRsp(使用map的结构)中对应结果
	// 3. 把结果按顺序push_back到RedisProxyRsp中
	for( size_t i = 0; i < size_t(oInnerReq.oinnerrequestbodylist_size()); ++ i )
	{
		RedisProxy::ResultBody & oResultBody = *(oRedisProxyRsp.add_oresultbodylist());

		google::protobuf::Map<string,RedisProxyInnerResult>::const_iterator mit 
				= oInnerRsp.mapredisproxyinnerresult().find( oInnerReq.oinnerrequestbodylist(i).ssid());
		if( mit != oInnerRsp.mapredisproxyinnerresult().end() )
		{
			oResultBody.set_nret( mit->second.nret() );
			oResultBody.set_svalue( mit->second.sresult().c_str(), mit->second.sresult().size() );
			oResultBody.set_nresulttype( mit->second.nresulttype() );
			oResultBody.set_nver( mit->second.nver() );
			// 构造回报向量元素
			for( size_t j = 0; j < size_t(mit->second.oresultelementlist_size()); ++ j )
			{
				RedisProxy::ResultElement &oResultElement = *( oResultBody.add_oresultelementlist() );
				oResultElement.set_nret( mit->second.oresultelementlist(j).nret() );
				oResultElement.set_nresulttype( mit->second.oresultelementlist(j).nresulttype() );
				oResultElement.set_sresultelement( mit->second.oresultelementlist(j).sresultelement().c_str(),
						mit->second.oresultelementlist(j).sresultelement().size() );
			}
		}
		else
		{
			oResultBody.set_nret( RedisProxy::RESULT_DATA_NOTFOUND );
			oResultBody.set_svalue( "" );
			oResultBody.set_nver( oInnerReq.oinnerrequestbodylist(i).nver() );
		}
	}
}

int RedisProxyManager::DoRedisProxyCmd(
		const string & sMsgid,
		const RedisProxyRequest & oRedisProxyReq,
		RedisProxyResponse & oRedisProxyRsp, 
		ServerContext * pContext )
{
	int nRet = AICommon::RET_SUCC;
	MYBOOST_LOG_INFO( OS_KV( "msgid",sMsgid ) << " DoRedisProxyCmd Begin. RedisProxyReq:" << oRedisProxyReq.DebugString()  );
	// 1. 创建Sessoin。单key命令请求转发到后端服务；如果是MGET、MSET、DEL多Key命令，需要拆分和分发对应的redis-server服务；如果是PipeLine的模式，则需要拆分和分发对应的redis-server服务
	// 2.每次完成应答接收，会发送完成信号，告知一次应答完成
	// 3.阻塞等待应答，等到所有应答收到后合并结果，并发送结果给客户端
	// 4.支持Pipeline的使用方式，二期再支持事务命令(但理论只是调用redis接口)
	RedisProxyInnerRequest oRedisProxyInnerReq;
	RedisProxyInnerResponse oRedisProxyInnerRsp;
	TransferRedisProxyReq2InnerReq( sMsgid, oRedisProxyReq, oRedisProxyInnerReq );
	MYBOOST_LOG_INFO( OS_KV( "msgid",sMsgid ) << " DoRedisProxyCmd Begin. RedisProxyInnerReq:" << oRedisProxyInnerReq.DebugString()  );

	if( oRedisProxyInnerReq.orequesthead().ncalltype() == RedisProxy::CALLTYPE_ASYNC )
	{
		DoRedisProxyCmdTask * pTask	= new DoRedisProxyCmdTask( 
				sMsgid, oRedisProxyInnerReq, oRedisProxyInnerRsp, pContext );
		if( pTask == NULL ||
				ProcessThreadPool::getInstance()->AddTask( pTask ) != ProcessThreadPool::ADD_TASK_RET_SUCC )
		{
			MYBOOST_LOG_ERROR( OS_KV( "msgid",sMsgid ) << " DoRedisProxyCmdTask OOM." );

			return AICommon::RET_SYSTEM_ERROR;
		}
	}
	else if( oRedisProxyInnerReq.orequesthead().ncalltype() == RedisProxy::CALLTYPE_SYNC )
	{
		RedisProxyCmd oRedisProxyCmd( sMsgid, oRedisProxyInnerReq );
		nRet = oRedisProxyCmd.HandleProcess( oRedisProxyInnerRsp );
		TransferInnerRsp2sRedisProxyRsp( oRedisProxyInnerReq, oRedisProxyInnerRsp, oRedisProxyRsp );
	}
	int64_t nMyLogStartTime = TNOWMS;
	MYBOOST_LOG_INFO( "RedisProxyRsp:" << OS_KV( "msgid", sMsgid ) << oRedisProxyRsp.DebugString()  );
	int64_t nMyLogEndTime   = TNOWMS;
	MYBOOST_LOG_INFO( OS_KV( "msgid", sMsgid ) << OS_KV( "mylogtime", nMyLogEndTime - nMyLogStartTime ) );

	return nRet;
}

bool RedisProxyManager::IsWriteCmd( string & sCmd )
{
	if( sCmd.find( "set" ) != string::npos || sCmd.find( "del" ) != string::npos
			|| sCmd.find( "zadd" ) != string::npos || sCmd.find( "incrby" ) != string::npos
			|| sCmd.find( "zremrange" ) != string::npos || sCmd.find( "expire") != string::npos )
	{
		return true;
	}
	return false;
}

bool RedisProxyManager::TransferCmd2KvElement( 
		vector< string >& vecItem, 
		dmp::RequestBody & oKvRequestBody,
		int32_t nTransferCmdMark )
{
	std::string sCmd    = "";
	for( size_t i = 0; i < size_t( vecItem.size() ); ++ i )
	{
		dmp::RequestElement & oDmpRequestElement	= *( oKvRequestBody.add_orequestelementlist() );
		if( i == 0 )
		{
			string sTmpCmd( vecItem[i].c_str(), vecItem[i].size() );
			sCmd	 = TC_Common::lower( sTmpCmd );
			if( sCmd == "hmset" ) sCmd = "hset";
			// 对于正常期间，写命令同步到Kafka
			if( nTransferCmdMark & TRANSFERCMDMARD_ONLY_WRITE ) 
			{
				if( !IsWriteCmd( sCmd ) ) return false;
			}
			// 对于下线/恢复期间，只有读命令请求到LMDB
			else if( nTransferCmdMark & TRANSFERCMDMARD_ONLY_READ )
			{
				if( IsWriteCmd( sCmd ) ) return false;
			}
			oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_CMD );
			oDmpRequestElement.set_srequestelement( sCmd.c_str(), sCmd.size() );
			oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
		}
		else if( i == 1 )
		{
			oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_KEY );
			oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
			oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
		}
		else if( i >= 2 )
		{
			if( sCmd == "set" || sCmd == "incrby" /*|| sCmd == "setex" */|| sCmd == "incrbyfloat" || sCmd == "expire" )
			{
				oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
				oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
				oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
			}
            else if( sCmd == "setbit" || sCmd == "setex" )
            {
                if( i % 2 == 0)
				{
                    if( sCmd == "setbit" ) oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_OFFSET );
                    else if( sCmd == "setex" ) oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_TIMEOUT );
					oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
					oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
				}
				else
				{
					oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
					oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
					oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
				}
            }
			else if( sCmd == "hset" || sCmd == "hmset" || sCmd == "hincrby" || sCmd == "hincrbyfloat" )
			{
				if( i % 2 == 0)
				{
					oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_FILED );
					oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
					oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
				}
				else
				{
					oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
					oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
					oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
				}
			}
			else if( sCmd == "hget" || sCmd == "hmget" || sCmd == "hdel" || sCmd == "getbit" )
			{
				if( sCmd == "getbit" ) oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_OFFSET );
                else oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_FILED );
				oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
				oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
			}
			else if( sCmd == "zadd" )
			{
                string sOp  = TC_Common::lower( vecItem[i] );
				if( sOp == "nx") 
				{
					oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_NX );
					oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
					continue;
				}
				else if( sOp == "xx") 
				{
					oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_XX );
					oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
					continue;
				}
				if( i % 2 == 0 )
				{
					oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_SCORE );
					oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
					oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
				}
				else
				{
					oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_MEMBER );
					oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
					oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
				}
			}
			else if( sCmd == "zrangebyscore" || sCmd == "zrange" || sCmd == "zrevrangebyscore" || sCmd == "zcount"
					|| sCmd == "zremrangebyscore" || sCmd == "zremrangebyrank" )
			{
                string sOp  = TC_Common::lower( vecItem[i] );
				if( sOp == "withscores" )
				{
					oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_SCORE );
					oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
					oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
					continue;
				}
				else if( sOp == "limit" )
				{
					i++;
					if( vecItem.size() > i )
					{
						oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_OFFSET );
						oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
						oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
					}
					i++;
					if( vecItem.size() > i )
					{
						oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_COUNT );
						oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
						oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
					}
					continue;
				}

				if( i % 2 == 0 )
				{
					oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_START );
					oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
					oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
				}
				else
				{
					oDmpRequestElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_STOP );
					oDmpRequestElement.set_srequestelement( vecItem[i].c_str(), vecItem[i].size() );
					oDmpRequestElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
				}
			}
		}
	}
	return true;
}

void RedisProxyManager::GenMigratePipeLineReq( 
        vector< std::string > & vecMigrateKey, 
        GroupInfo & oGroupInfo,
        MigrateConf & oMigrateConf,
        RedisConf & oRedisConf,
        PipeLineRequest & oPipeLineReq )
{
    RedisProxyCommon::CmdBody & oMigrateCmdBody	= *(oPipeLineReq.add_ocmdbodylist());

    RedisProxy::RequestElement & oMigrateCmdElement  = *(oMigrateCmdBody.add_orequestelementlist());
    oMigrateCmdElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_CMD );
    oMigrateCmdElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
    oMigrateCmdElement.set_srequestelement( "migrate" );

    RedisProxy::RequestElement & oIpElement  = *(oMigrateCmdBody.add_orequestelementlist());
    oIpElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
    oIpElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
    oIpElement.set_srequestelement( oGroupInfo.omaster().sip() );

    RedisProxy::RequestElement & oPortElement  = *(oMigrateCmdBody.add_orequestelementlist());
    oPortElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
    oPortElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
    oPortElement.set_srequestelement( TC_Common::tostr(oGroupInfo.omaster().nport() ) );

    RedisProxy::RequestElement & oEmptyElement  = *(oMigrateCmdBody.add_orequestelementlist());
    oEmptyElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
    oEmptyElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
    oEmptyElement.set_srequestelement( "" );

    RedisProxy::RequestElement & oDbElement  = *(oMigrateCmdBody.add_orequestelementlist());
    oDbElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
    oDbElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
    oDbElement.set_srequestelement( TC_Common::tostr(oGroupInfo.omaster().ndatabase() ) );

    RedisProxy::RequestElement & oTimeoutElement  = *(oMigrateCmdBody.add_orequestelementlist());
    oTimeoutElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
    oTimeoutElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
    oTimeoutElement.set_srequestelement( TC_Common::tostr( oMigrateConf.nMigrateTimeout ) );

    RedisProxy::RequestElement & oReplaceElement  = *(oMigrateCmdBody.add_orequestelementlist());
    oReplaceElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
    oReplaceElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
    oReplaceElement.set_srequestelement( "replace" );

    RedisProxy::RequestElement & oAuthElement  = *(oMigrateCmdBody.add_orequestelementlist());
    oAuthElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
    oAuthElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
    oAuthElement.set_srequestelement( "auth" );

    RedisProxy::RequestElement & oPasswdElement  = *(oMigrateCmdBody.add_orequestelementlist());
    oPasswdElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
    oPasswdElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
    oPasswdElement.set_srequestelement( oRedisConf._sPasswd.c_str(), oRedisConf._sPasswd.size() );

    RedisProxy::RequestElement & oKeysElement  = *(oMigrateCmdBody.add_orequestelementlist());
    oKeysElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
    oKeysElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
    oKeysElement.set_srequestelement( "KEYS" );

    for( vector< string >::iterator it = vecMigrateKey.begin(); it != vecMigrateKey.end(); ++ it )
    {
        RedisProxy::RequestElement & oMigrateKeyElement  = *(oMigrateCmdBody.add_orequestelementlist());
        oMigrateKeyElement.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
        oMigrateKeyElement.set_nchartype( RedisProxy::CHARTYPE_STRING );
        oMigrateKeyElement.set_srequestelement( (*it).c_str(), (*it).size() );
    }
    oMigrateCmdBody.set_skey( "" );
    oMigrateCmdBody.set_scmd( "" );
    oMigrateCmdBody.set_svalue( "" );
    oMigrateCmdBody.set_ssid( "" );
    oMigrateCmdBody.set_nver( RedisProxy::VERSION_ELEMENTVECTOR );
}

int RedisProxyManager::TransferCmdStrToInt( const string & sCmd )
{
	return 0;
}

int RedisProxyManager::DoCmdAgent( 
		const string & sMsgid,
		const RedisProxy::RequestBody & oRequestBody )
{
	int nCmd = TransferCmdStrToInt( oRequestBody.scmd() );
	switch( nCmd )
	{
		case RedisProxyCommon::REDIS_CMD_GET:
			break;
		case RedisProxyCommon::REDIS_CMD_EXISTS:
			break;
		case RedisProxyCommon::REDIS_CMD_ZCOUNT:
			break;
		case RedisProxyCommon::REDIS_CMD_ZRANGEBYSCORE:
			break;
		case RedisProxyCommon::REDIS_CMD_EXPIRE:
			break;
		case RedisProxyCommon::REDIS_CMD_SET:
			break;
		case RedisProxyCommon::REDIS_CMD_ZADD:
			break;
		case RedisProxyCommon::REDIS_CMD_ZREMRANGEBYSCORE:
			break;
		case RedisProxyCommon::REDIS_CMD_ZREMRANGEBYRANK:
			break;
		case RedisProxyCommon::REDIS_CMD_HGET:
			break;
		case RedisProxyCommon::REDIS_CMD_HMGET:
			break;
		case RedisProxyCommon::REDIS_CMD_HGETALL:
			break;
		case RedisProxyCommon::REDIS_CMD_HDEL:
			break;
		case RedisProxyCommon::REDIS_CMD_HSET:
			break;
		case RedisProxyCommon::REDIS_CMD_HMSET:
			break;
		case RedisProxyCommon::REDIS_CMD_HINCRBY:
			break;
		case RedisProxyCommon::REDIS_CMD_HINCRBYFLOAT:
			break;
		default:
			break;
	}
	return AICommon::RET_SUCC;
}
