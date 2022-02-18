/*************************************************************************
    > File Name: redisproxythreadclient_single.cpp
    > Author: annulsluo
    > Mail: annulsluo@gmail.com
    > Created Time: 五  5/ 8 11:37:42 2020
 ************************************************************************/
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <string.h>
#include "util/myboost_log.h"
#include "util/tc_common.h"
#include "util/common.h"
#include <pthread.h>
#include <random>
#include "util/TafOsVar.h"
#include "util/tc_timeprovider.h"
#include "util/tc_base64.h"

#include "redisproxy.pb.h"
#include "redisproxy.grpc.pb.h"
#include <sys/syscall.h>


using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using RedisProxy::RedisProxyRequest;
using RedisProxy::RedisProxyResponse;
using RedisProxy::RedisProxyService;
using namespace RedisProxy;

using namespace std;
using namespace taf;

struct Data
{
	int nMemberNum;
	int nExpireTime;
	int nTimeout;
	bool bBase64flag;
	string sIp;
	string sPort;
	string sAppId;
	string sAppPw;
	string sKey;
	string sVal;
};

void StubSend( RedisProxyRequest oReq, struct Data & oData, std::unique_ptr< RedisProxyService::Stub > &pStub )
{
	try{
		RedisProxyResponse oRsp;
		ClientContext context;
		gpr_timespec timespec;
		timespec.tv_sec		= 0;
		timespec.tv_nsec	= oData.nTimeout * 1000 * 1000;
		timespec.clock_type	= GPR_TIMESPAN;
		context.set_deadline( timespec );

		int64_t nStartTime	= TNOWMS;
		Status status		= pStub->RedisProxyCmd( &context, oReq, &oRsp );
		int64_t nEndTime	= TNOWMS;
		int64_t nLatency	= nEndTime - nStartTime;
		printf( "latency:%lld req:%s rsp:%s", nLatency, oReq.DebugString().c_str(), oRsp.DebugString().c_str() );
		
		if (status.ok()) {
			printf( "ok bodylist_size:%d\n" , oRsp.oresultbodylist_size() );
		} else {
			printf( "fail.errorcode:%d msg:%s\n", status.error_code(), status.error_message().c_str() );
		}
	}
	catch( exception & e )
	{
		std::cout << "StubSend exception:" << e.what() << endl;
	}
}

void CreateChannel( struct Data & oData, std::unique_ptr< RedisProxyService::Stub> & pStub )
{
	try{
		std::shared_ptr<Channel> oChannel(
				grpc::CreateChannel(oData.sIp+":"+ oData.sPort,grpc::InsecureChannelCredentials()));
		pStub = RedisProxyService::NewStub( oChannel ); 
	}
	catch( exception & e )
	{
		std::cout << "createchannel exception:" << e.what() << endl;
	}
}

void GenRequest( struct Data & oData, RedisProxyRequest & oReq )
{
	oReq.set_sappid( oData.sAppId );
	oReq.set_sapppw( oData.sAppPw );

	RequestHead &oRequestHead = *(oReq.mutable_orequesthead() );
	oRequestHead.set_ncallmode( CALLMODE_SINGLE );
	oRequestHead.set_ncalltype( CALLTYPE_SYNC);
}

void GenRequestBody( string & sCmd, RequestBody & oRequestBody, struct Data & oData )
{
	RequestElement &oRequestElementCmd = *(oRequestBody.add_orequestelementlist());
	oRequestElementCmd.set_srequestelement( sCmd );
	oRequestElementCmd.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_CMD );

	RequestElement &oRequestElementKey = *(oRequestBody.add_orequestelementlist());
	oRequestElementKey.set_nchartype( CHARTYPE_STRING );
	oRequestElementKey.set_srequestelement( oData.sKey );
	oRequestElementKey.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_KEY );

	oRequestBody.set_nver( VERSION_ELEMENTVECTOR );
}

void GenRequestSetExElement( RequestBody & oRequestBody, struct Data & oData )
{
	int nSeconds = oData.nExpireTime;
	RequestElement &oRequestElementSeconds = *(oRequestBody.add_orequestelementlist());
	oRequestElementSeconds.set_nchartype( CHARTYPE_STRING );
	oRequestElementSeconds.set_srequestelement( TC_Common::tostr( nSeconds ) );
	oRequestElementSeconds.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_TIMEOUT );

	RequestElement &oRequestElementVal = *(oRequestBody.add_orequestelementlist());
	oRequestElementVal.set_nchartype( CHARTYPE_BINARY );
	unsigned char * pDst = NULL;
	pDst = new unsigned char[oData.sVal.size()];
	int nDstLen = 0;
	if( oData.bBase64flag )
	{
		nDstLen	= TC_Base64::decode( oData.sVal.c_str(),oData.sVal.size(), pDst );
	}
	oRequestElementVal.set_srequestelement( (char*)pDst, nDstLen );
	oRequestElementVal.set_nrequestelementtype( RedisProxy::REQUESTELEMENTTYPE_VALUE );
	delete pDst;
}

void PrintfSingleRequest( struct Data & oData )
{
	printf( "start! membernum:%d expiretime:%d timeout:%d base64floag:%d \
			ip:%s port:%s appid:%s apppw:%s key:%s value:%s\n",
			oData.nMemberNum, oData.nExpireTime, oData.nTimeout, oData.bBase64flag,
			oData.sIp.c_str(), oData.sPort.c_str(), oData.sAppId.c_str(), oData.sAppPw.c_str(),
			oData.sKey.c_str(), oData.sVal.c_str() );
}

void DoRedisProxySetex( struct Data & oData ) {
	PrintfSingleRequest( oData );
	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( oData, pStub );

	RedisProxyRequest oReq;
	GenRequest( oData, oReq );
	RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
	string sCmd = "setex";
	GenRequestBody( sCmd, oRequestBody, oData );
	GenRequestSetExElement( oRequestBody, oData );

	StubSend( oReq, oData, pStub );
}

void DoRedisProxyGet( struct Data & oData ) {
	PrintfSingleRequest( oData );
	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( oData, pStub );

	RedisProxyRequest oReq;
	GenRequest( oData, oReq );
	RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
	string sCmd = "get";
	GenRequestBody( sCmd, oRequestBody, oData );
	StubSend( oReq, oData, pStub );
}

int main(int argc, char** argv) {
	std::string sName0	= "test1";
	std::string * sName	= new string();
	printf( "%s size:%d\n", sName->c_str(), sName->size() );
	*sName = sName0;
	printf( "%s size:%d\n", sName->c_str(), sName->size() );
	if( sName != NULL ) delete sName;
	if( argc < 3 )
	{
		printf( "Usage: exe logpath logname ip port cmd[set|get|ping|exist|zadd|zcount|hset|hmset|hget|hgetall| \
				hmget|hincBy|setex|incryBy|zrangeByScore|zremrangeByScore] \
				appid apppw key value expiretime timeout base64flag\n ");
		return -1;
	}
	//struct Data * oData = (struct Data *)malloc( sizeof( struct Data ) );
	Data oData;
	std::string sPath	=argv[1];
	std::string sBaseLogName = argv[2];
	string  sIp			= argv[3];
	string sPort		= argv[4];
	string sOpt			= argv[5];
	string  sAppId		= argv[6];
	string sAppPw		= argv[7];
	string sKey			= argv[8];
	string sValue		= "";
	int nExpireTime		= 0;
	int nTimeout		= 0;
	int nBase64flag		= 0;
	
	if( argc > 8 )		sValue	= argv[9];
	if( argc > 9 )		nExpireTime	= atoi( argv[10] );
	if( argc > 10 )		nTimeout = atoi( argv[11] );
	if( argc > 11 )		nBase64flag = atoi( argv[12] );
	oData.sIp			= sIp;
	oData.sPort		= sPort;
	oData.sAppId		= sAppId;
	oData.sAppPw		= sAppPw;
	oData.sKey			= sKey;
	oData.sVal			= sValue;
	oData.nExpireTime	= nExpireTime;
	oData.nTimeout		= nTimeout;
	oData.bBase64flag	= nBase64flag==0?false:true;

	//g_InitMyBoostLog( sPath, sBaseLogName, "debug" );
	//MYBOOST_LOG_INFO(  "An informational severity message" );
	if( sOpt == "setex" )
	{
		DoRedisProxySetex( oData );
	}
	else if( sOpt == "get")
	{
		DoRedisProxyGet( oData );
	}
	return 0;
}


