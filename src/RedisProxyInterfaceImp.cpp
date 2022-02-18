/*************************************************************************
    > File Name: RedisProxyInterfaceImp.cpp
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 四 11/ 7 15:09:32 2019
 ************************************************************************/
// Logic and data behind the server's behavior

#include "RedisProxyInterfaceImp.h"
#include "RedisProxyManager.h"
#include "util/tc_common.h"
#include "ServerConfig.h"
#include <grpcpp/grpcpp.h>
#include "util/common.h"
#include <boost/log/core.hpp>
#include "util/TafOsVar.h"
#include "util/myboost_log.h"
#include "RedisProxyPrometheus.h"
#include "RedisProxyZkData.h"
#include "RedisProxyAppFlowControl.h"

using namespace std;
using namespace RedisProxy;
using namespace AICommon;
using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;
	
RedisProxyInterfaceImp::RedisProxyInterfaceImp()
{

}

RedisProxyInterfaceImp::~RedisProxyInterfaceImp()
{

}

bool RedisProxyInterfaceImp::IsValidParam( const string & sMsgid, const RedisProxyRequest & oRedisProxyReq )
{
	// 判断是否输入的参数有效
	std::string sAppId		= oRedisProxyReq.sappid();
	std::string sAppPw		= oRedisProxyReq.sapppw();
	std::string sTableName	= oRedisProxyReq.stablename();

	RedisProxy::RequestHead oReqHead		= oRedisProxyReq.orequesthead();
	if( sAppId.empty() || sAppPw.empty() )
		return false;

	if( oReqHead.ncallmode() == RedisProxy::CALLMODE_NONE 
		|| oReqHead.ncallmode() > RedisProxy::CALLMODE_PIPELINE )
	{
		MYBOOST_LOG_ERROR( OS_KV("Msgid",sMsgid) << OS_KV( "callmode",oReqHead.ncallmode() ) << " InvalidCallMode." );
		return false;
	}

	if( oReqHead.ncalltype() == RedisProxy::CALLTYPE_NONE 
		|| oReqHead.ncalltype() > RedisProxy::CALLTYPE_ASYNC )
	{
		MYBOOST_LOG_ERROR( OS_KV("Msgid",sMsgid) << OS_KV( "calltype",oReqHead.ncalltype() ) << " InvalidCallType." );
		return false;
	}

	if( oRedisProxyReq.orequestbodylist_size() == 0 )
	{
		MYBOOST_LOG_ERROR( OS_KV("Msgid",sMsgid) << " RequestBodyList Size Invalid." );
		return false;
	}
	
	for( size_t i = 0; i < size_t(oRedisProxyReq.orequestbodylist_size()); ++ i )
	{
		RedisProxy::RequestBody oRequestBody	= oRedisProxyReq.orequestbodylist(i);
		if( oRequestBody.nver() == VERSION_NONE )
		{
			MYBOOST_LOG_ERROR( OS_KV("Msgid",sMsgid) << OS_KV( "index", i ) << " Version Invalid." );
			return false;
		}
		else if( oRequestBody.nver() == VERSION_KV )
		{
			if( oRequestBody.scmd().empty() || oRequestBody.skey().empty() )
			{
				MYBOOST_LOG_ERROR( OS_KV("Msgid",sMsgid) << OS_KV( "index", i ) << " KV Invalid." );
				return false;
			}
		}
		else if( oRequestBody.nver() == VERSION_CMD )
		{
			if( oRequestBody.scmd().empty() )
			{
				MYBOOST_LOG_ERROR( OS_KV("Msgid",sMsgid) << OS_KV( "index", i ) << " CMD Invalid." );
				return false;
			}
			vector< string > vecItem;
			vecItem = TC_Common::sepstr<string>( oRequestBody.scmd(), string(1,' ') );
			if( vecItem.size() <= 1 ) 
			{
				MYBOOST_LOG_ERROR( OS_KV("Msgid",sMsgid) << OS_KV( "index", i ) << " CMD least than 1." );
				return false;
			}
			//防止 getbit/setbit offset 过大，导致内存过大
			if( vecItem.size() > CMD_VALUE_INDEX && 
                    ( TC_Common::lower(vecItem[CMD_CMD_INDEX]) == "setbit"
                      || TC_Common::lower(vecItem[CMD_CMD_INDEX]) == "getbit" ) )
			{
				int nValue = TC_Common::strto<int>( vecItem[CMD_VALUE_INDEX] );
				if( nValue > ServerConfigMng::getInstance()->GetIObj( "max_bit" ) )
				{
					MYBOOST_LOG_ERROR( OS_KV("Msgid",sMsgid) << OS_KV( "index", i ) 
							<< OS_KV("setbit ", nValue ) 
							<< OS_KV( "lt than ", ServerConfigMng::getInstance()->GetIObj( "max_bit" ) ) );
					return false;
				}
			}
		}
		else if( oRequestBody.nver() == VERSION_ELEMENTVECTOR )
		{
			if( oRequestBody.orequestelementlist_size() == 0 )
			{
				MYBOOST_LOG_ERROR( OS_KV("Msgid",sMsgid) << OS_KV( "index", i ) << " elementvec Invalid." );
				return false;
			}
		}
		else 
		{
			return false;
		}
	}

	return true;
}

bool RedisProxyInterfaceImp::IsValidAuth( const RedisProxyRequest & oRedisProxyReq )
{
	// 写如果不同业务同个实例，则提供写共同账号，读的时候使用不同账号
	const RedisProxyCommon::AppInfo & oAppInfo
			= RedisProxyZkData::getInstance()->GetAppInfoByAppId( oRedisProxyReq.sappid() );
	MYBOOST_LOG_INFO( OS_KV( "appinfo", oAppInfo.DebugString() ) );

	if( oAppInfo.sappid() == oRedisProxyReq.sappid() && oRedisProxyReq.sapppw() == oAppInfo.sapppw() )
	{
		return true;
	}
	return false;
}

void RedisProxyInterfaceImp::TransferAppNameAndTableName( 
		const std::string & sAppName, 
		const std::string & sTableName,
		RedisProxy::RedisProxyRequest & oRedisProxyReq )
{
	const RedisProxyCommon::AppInfo & oAppInfo
			= RedisProxyZkData::getInstance()->GetAppInfoByAppId( oRedisProxyReq.sappid() );
	if( oAppInfo.sappid() == oRedisProxyReq.sappid() )
	{
		if( sAppName.empty() || sTableName.empty() )
		{
			oRedisProxyReq.set_sappname( oAppInfo.sappname() ); 
			oRedisProxyReq.set_stablename( oAppInfo.stablename() );
		}
	}
}

void RedisProxyInterfaceImp::ReportLatencyStatic( const int & nCost )
{
	string sLabelKey = "Deallatency:";
	if( nCost < 0 )
	{
		sLabelKey += "unknow_cost";
	}
	else if( nCost == 0 )
	{
		sLabelKey += "0";
	}
	else if( nCost > 0 && nCost <= 1 )
	{
		sLabelKey += "(0,1]";
	}
	else if( nCost > 1 && nCost <= 2 )
	{
		sLabelKey += "(1,2]";
	}
	else if( nCost > 2 && nCost <= 5 )
	{
		sLabelKey += "(2,5]";

	}
	else if( nCost > 5 && nCost <= 10 )
	{
		sLabelKey += "(5,10]";

	}
	else if( nCost > 10 && nCost <= 12 )
	{
		sLabelKey += "(10,12]";

	}
	else if( nCost > 12 && nCost <= 15 )
	{
		sLabelKey += "(12,15]";

	}
	else if( nCost > 15 && nCost <= 20 )
	{
		sLabelKey += "(15,20]";

	}
	else if( nCost > 20 )
	{
		sLabelKey += "(20,+&)";
	}
	RedisProxyPrometheus::getInstance()->Report( sLabelKey );
}

Status RedisProxyInterfaceImp::RedisProxyCmd(
		ServerContext * pContext, 
		const RedisProxyRequest * pRedisProxyReq,
		RedisProxyResponse * pRedisProxyRsp )
{
	int64_t nStartTime = TNOWMS;
	string sStartTime = TC_Common::now2str( "%Y-%m-%d %H:%M:%S" ) + "." + TC_Common::tostr(nStartTime % 1000);
	// 生成唯一ID
	string sMsgid					= ::GenerateUUID(); 
	RedisProxyRequest * pReq		= const_cast<RedisProxyRequest*>(pRedisProxyReq);
	RedisProxyRequest &oRedisProxyReq	= *pReq;
	RedisProxyResponse &oRedisProxyRsp	= *pRedisProxyRsp;
	
	std::string sAppId		= oRedisProxyReq.sappid();
	std::string sAppPw		= oRedisProxyReq.sapppw();
	std::string sAppName	= oRedisProxyReq.sappname();
	std::string sTableName	= oRedisProxyReq.stablename();
	MYBOOST_LOG_INFO( OS_KV( "msgid", sMsgid ) << OS_KV( "starttime", sStartTime ) 
			<< OS_KV( "client", pContext->peer() ) ) ;

	RedisProxyPrometheus::getInstance()->Report( "Property:Req" );
	RedisProxyPrometheus::getInstance()->ReportPlus( "RedisProxyAppReq", "appid:"+sAppId );
	RedisProxyPrometheus::getInstance()->ReportPlus( "RedisProxyPipeline", 
			"appid:"+sAppId, oRedisProxyReq.orequestbodylist_size() );
	// ipv4 和 ipv6的解释方式会不一样，现支持ipv4方式
	vector< string > vecClientInfo = TC_Common::sepstr<string>(pContext->peer(), ":");
	string sClientIp		= "";
	if( vecClientInfo.size() == GRPC_CLIENTINFO_LEN ) sClientIp = vecClientInfo[1];
	RedisProxyPrometheus::getInstance()->ReportPlus( "RedisProxyClientCounter",
			"clientip:"+ sClientIp );
	if( RedisProxyAppFlowControl::getInstance()->IsFlowControl( sAppId, oRedisProxyReq.orequestbodylist_size() ) )
	{
		MYBOOST_LOG_ERROR( OS_KV("Msgid",sMsgid)<< OS_KV("app",sAppName) << OS_KV( "table",sTableName )
				<< OS_KV( "appid", sAppId ) << OS_KV( "apppw", sAppPw ) 
				<< " IsFlowControl." );
		oRedisProxyRsp.set_ngret( RedisProxy::GLOBAL_QUOTAEXCEE );

		::google::protobuf::RepeatedPtrField< RedisProxy::RequestBody >vecRequest 
			= oRedisProxyReq.orequestbodylist();	
		for( size_t i = 0; i < size_t(vecRequest.size()); ++ i )
		{
			RedisProxy::ResultBody & oResultBody = *(oRedisProxyRsp.add_oresultbodylist());
			oResultBody.set_nver( vecRequest[i].nver() );
		}
		RedisProxyPrometheus::getInstance()->Report( "InvalidCase:InFlowControl" );

		return Status(static_cast<StatusCode>(RedisProxy::GLOBAL_SUCC),"OK");
	}

	if( ServerConfigMng::getInstance()->GetIObj( "authapp_ratio") && !IsValidAuth( oRedisProxyReq ) )
	{
		MYBOOST_LOG_ERROR( OS_KV("Msgid",sMsgid)<< OS_KV("app",sAppName) << OS_KV( "table",sTableName )
				<< OS_KV( "appid", sAppId ) << OS_KV( "apppw", sAppPw ) 
				<< " IsValidAuth." );
		oRedisProxyRsp.set_ngret( RedisProxy::GLOBAL_INVALID_USERPASSWD );
		::google::protobuf::RepeatedPtrField< RedisProxy::RequestBody >vecRequest 
			= oRedisProxyReq.orequestbodylist();	
		for( size_t i = 0; i < size_t(vecRequest.size()); ++ i )
		{
			RedisProxy::ResultBody & oResultBody = *(oRedisProxyRsp.add_oresultbodylist());
			oResultBody.set_nver( vecRequest[i].nver() );
		}
		RedisProxyPrometheus::getInstance()->Report( "InvalidCase:InvalidAuth" );

		return Status(static_cast<StatusCode>(RedisProxy::GLOBAL_SUCC),"OK");
	}

	TransferAppNameAndTableName( sAppName, sTableName, oRedisProxyReq );

	if( !IsValidParam( sMsgid, oRedisProxyReq ) )
	{
		MYBOOST_LOG_ERROR( OS_KV("Msgid",sMsgid) << OS_KV("app",sAppName) << OS_KV( "table",sTableName )
				<< OS_KV( "appid", sAppId ) << OS_KV( "apppw", sAppPw ) 
				<< " IsValidParam." );
		::google::protobuf::RepeatedPtrField< RedisProxy::RequestBody >vecRequest 
			= oRedisProxyReq.orequestbodylist();	
		oRedisProxyRsp.set_ngret( RedisProxy::GLOBAL_INVALID_PARAM );
		for( size_t i = 0; i < size_t(vecRequest.size()); ++ i )
		{
			RedisProxy::ResultBody & oResultBody = *(oRedisProxyRsp.add_oresultbodylist());
			oResultBody.set_nret( RedisProxy::RESULT_UNKNOWN_VERSION );
			oResultBody.set_nver( vecRequest[i].nver() );
		}
		RedisProxyPrometheus::getInstance()->Report( "InvalidCase:InvalidParam" );
		return Status(static_cast<StatusCode>(RedisProxy::GLOBAL_SUCC),"OK");
	}

	// 返回Redis获取的内容
	RedisProxyManager oRedisProxyManager;
	oRedisProxyManager.DoRedisProxyCmd( sMsgid, oRedisProxyReq, oRedisProxyRsp, pContext );
	int64_t nEndTime	= TNOWMS;
	string sEndTime = TC_Common::now2str( "%Y-%m-%d %H:%M:%S" ) + "." + TC_Common::tostr(nEndTime % 1000);
	int64_t nCost		= nEndTime - nStartTime;
	int64_t nDoCmdTimeout	= ServerConfigMng::getInstance()->GetIObj( "DoCmdTimeout" );
	int64_t nMaxDoReqTimeout	= ServerConfigMng::getInstance()->GetIObj( "MaxDoReqTimeout" );
	RedisProxyPrometheus::getInstance()->Report( "Deallatency:totallatency", nCost );
	ReportLatencyStatic( nCost );
	if( nCost > nDoCmdTimeout )
	{
		RedisProxyPrometheus::getInstance()->Report( "Timeout:DoCmdTimeout" );
		MYBOOST_LOG_ERROR( OS_KV( "Msgid", sMsgid ) 
				<< OS_KV( "starttime", sStartTime ) << OS_KV( "endtime", sEndTime )
				<< OS_KV( "realcost", nCost ) << OS_KV( "nDoCmdTimeout", nDoCmdTimeout ) 
				<< OS_KV( "RedisProxyReq", oRedisProxyReq.DebugString() ) );
		if( nCost > nMaxDoReqTimeout )
		{
			RedisProxyPrometheus::getInstance()->Report( "Timeout:MaxDoReqTimeout" );
			MYBOOST_LOG_ERROR( OS_KV( "Msgid", sMsgid ) 
				<< OS_KV( "starttime", sStartTime ) << OS_KV( "endtime", sEndTime )
				<< OS_KV( "realcost", nCost ) << OS_KV( "maxtimeout", nMaxDoReqTimeout ) 
				<< OS_KV( "RedisProxyRsp", oRedisProxyRsp.DebugString() ) );
		}
	}

	if( pContext->IsCancelled() )
	{
		RedisProxyPrometheus::getInstance()->Report( "Property:IsCancelled" );
        RedisProxyPrometheus::getInstance()->ReportPlus( "RedisProxyCanCell", "clientip:"+sClientIp );
		MYBOOST_LOG_ERROR( "Context Cancell." << OS_KV( "Msgid", sMsgid ) 
				<< OS_KV( "starttime", sStartTime ) << OS_KV( "endtime", sEndTime )
				<< OS_KV( "realcost", nCost ) << OS_KV( "nDoCmdTimeout", nDoCmdTimeout ) 
				<< OS_KV( "RedisProxyReq", oRedisProxyReq.DebugString() ) );
		if( nCost > nMaxDoReqTimeout )
		{
			MYBOOST_LOG_ERROR( "Context Cancell." << OS_KV( "Msgid", sMsgid ) 
				<< OS_KV( "starttime", sStartTime ) << OS_KV( "endtime", sEndTime )
				<< OS_KV( "realcost", nCost ) << OS_KV( "maxtimeout", nMaxDoReqTimeout ) 
				<< OS_KV( "RedisProxyRsp", oRedisProxyRsp.DebugString() ) );
		}
	}
	RedisProxyPrometheus::getInstance()->Report( "Property:Rsp" );
	
	return Status(static_cast<StatusCode>(RedisProxy::GLOBAL_SUCC),"OK");
}

