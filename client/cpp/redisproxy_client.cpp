/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include <string.h>
#include "util/myboost_log.h"
#include "util/tc_common.h"
#include "util/common.h"
#include <pthread.h>
#include <random>
#include "util/TafOsVar.h"
#include "util/tc_timeprovider.h"

#include "redisproxy.pb.h"
#include "redisproxy.grpc.pb.h"

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
	int nThreadNum;
	int nPipeLineNum;
	int nQps;
};

class RedisProxyClient {
	public:
		RedisProxyClient(){
		}

		static void DoRedisProxyPing(void *pData) {
			RedisProxyRequest oReq;
			oReq.set_sappid("redis_test");
			oReq.set_sapppw("redis_teset");
			oReq.set_sappname( "aiad" );
			oReq.set_stablename( "userprofile" );

			RequestHead &oRequestHead = *(oReq.mutable_orequesthead() );
			oRequestHead.set_ncallmode( CALLMODE_SINGLE );
			oRequestHead.set_ncalltype( CALLTYPE_SYNC);

			struct Data oData = (struct Data*)pData;
			int64_t nStartTime = TNOWMS;
			for( size_t nReqNum = 0; i < oData.nQps/oData.nThreadNum; ++ i )	
			{
				for( size_t i = 0; i < oData.nPipeLineNum; ++ i )
				{
					RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
					int nKey = 100000 + rand() % 100 + i;
					string sVal = ::GenerateUUID();
					oRequestBody.set_skey( "" );
					oRequestBody.set_scmd( "PING" );
					oRequestBody.set_svalue( "" );
					printf( "index:%d key:%d cmd:set value:%s\n", int(i), nKey, sVal.c_str() );
				}

				RedisProxyResponse oRsp;
				ClientContext context;
				std::unique_ptr<RedisProxyService::Stub> oStub;
				oStub = RedisProxyService::NewStub(grpc::CreateChannel(sIp+":"+sPort, grpc::InsecureChannelCredentials()));
				Status status = oStub->RedisProxyCmd( &context, oReq, &oRsp );

				if (status.ok()) {
					printf( "bodylist_size:%d" , oRsp.oresultbodylist_size() );
					MYBOOST_LOG_DEBUG( OS_KV( "pthreadid", pthread_self()) << OS_KV( "bodylistsize", oRsp.oresultbodylist_size() ) );
				} else {
					std::cout << OS_KV( "code", status.error_code() ) << OS_KV( "msg", status.error_message() ) << std::endl;
					MYBOOST_LOG_DEBUG( OS_KV( "pthreadid", pthread_self() )
							<< OS_KV( "code", status.error_code() ) << OS_KV( "msg", status.error_message() ) );
				}
			}
			int64_t nEndTime = TNOWMS;
			MYBOOST_LOG_DEBUG( OS_KV( "pthreadid", pthread_self() ) 
					<< OS_KV( "totalreq", oData.nQps ) << OS_KV( "threadnum", oData.nThreadNum ) 
					<< OS_KV( "pipelinenum", oData.nPipeLineNum ) << OS_KV( "totalcost", nEndTime - nStartTime )
					<< OS_KV( "percost", double(nEndTime-nStartTime)/double(oData.nQps) << OS_Kv( "qps", double(oData.nQps)/(nEndTime - nStartTime )/1000 ) ) ); 

		}

		static void DoRedisProxySet() {
			RedisProxyRequest oReq;
			oReq.set_sappid("redis_test");
			oReq.set_sapppw("redis_teset");
			oReq.set_sappname( "aiad" );
			oReq.set_stablename( "userprofile" );

			RequestHead &oRequestHead = *(oReq.mutable_orequesthead() );
			oRequestHead.set_ncallmode( CALLMODE_SINGLE );
			oRequestHead.set_ncalltype( CALLTYPE_SYNC);

			struct Data oData = (struct Data*)pData;
			int64_t nStartTime = TNOWMS;
			for( size_t nReqNum = 0; i < oData.nQps/oData.nThreadNum; ++ i )	
			{

				for( size_t i = 0; i < 10; ++ i )
				{
					RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
					int nKey = 100000 + rand() % 100 + i;
					string sVal = ::GenerateUUID();
					oRequestBody.set_skey( TC_Common::tostr( nKey ) );
					oRequestBody.set_scmd( "set" );
					oRequestBody.set_svalue( sVal );
					printf( "pthreadid:%lu index:%d key:%d cmd:set value:%s\n", pthread_self(), int(i), nKey, sVal.c_str() );
					MYBOOST_LOG_DEBUG( OS_KV("ptheadid", pthread_self() ) << OS_KV( "index", int(i) ) 
							<< OS_KV("key",nKey)<<"cmd:set"<< OS_KV("value",sValue) );
				}

				RedisProxyResponse oRsp;
				ClientContext context;
				std::unique_ptr<RedisProxyService::Stub> oStub;
				oStub = RedisProxyService::NewStub(grpc::CreateChannel(sIp+":"+sPort, grpc::InsecureChannelCredentials()));
				Status status = oStub->RedisProxyCmd( &context, oReq, &oRsp );

				if (status.ok()) {
					printf( "bodylist_size:%d" , oRsp.oresultbodylist_size() );
					MYBOOST_LOG_DEBUG( OS_KV( "pthreadid", pthread_self()) << OS_KV( "bodylistsize", oRsp.oresultbodylist_size() ) );
				} else {
					std::cout << OS_KV( "code", status.error_code() ) << OS_KV( "msg", status.error_message() )
						<< std::endl;
					MYBOOST_LOG_DEBUG( OS_KV( "pthreadid", pthread_self() )
							<< OS_KV( "code", status.error_code() ) << OS_KV( "msg", status.error_message() ) );
				}
			}
			int64_t nEndTime = TNOWMS;
			MYBOOST_LOG_DEBUG( OS_KV( "pthreadid", pthread_self() ) 
					<< OS_KV( "totalreq", oData.nQps ) << OS_KV( "threadnum", oData.nThreadNum ) 
					<< OS_KV( "pipelinenum", oData.nPipeLineNum ) << OS_KV( "totalcost", nEndTime - nStartTime )
					<< OS_KV( "percost", double(nEndTime-nStartTime)/double(oData.nQps) << OS_Kv( "qps", double(oData.nQps)/(nEndTime - nStartTime )/1000 ) ) ); 

		}

		void DoRedisProxyGet() {
			// Data we are sending to the server.
			RedisProxyRequest oReq;
			oReq.set_sappid("redis_test");
			oReq.set_sapppw("redis_teset");
			oReq.set_sappname( "aiad" );
			oReq.set_stablename( "userprofile" );

			RequestHead &oRequestHead = *(oReq.mutable_orequesthead() );
			oRequestHead.set_ncallmode( CALLMODE_SINGLE );
			oRequestHead.set_ncalltype( CALLTYPE_SYNC);

			struct Data oData = (struct Data*)pData;
			int64_t nStartTime = TNOWMS;
			for( size_t nReqNum = 0; i < oData.nQps/oData.nThreadNum; ++ i )	
			{
				for( size_t i = 0; i < oData.nPipeLineNum; ++ i )
				{
					RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
					int nKey = 100000 + rand() % 100 + i;
					oRequestBody.set_skey( TC_Common::tostr( nKey ) );
					oRequestBody.set_scmd( "get" );
					oRequestBody.set_svalue( "" );
					printf( "index:%d key:%d cmd:get\n", int(i), nKey );
				}

				RedisProxyResponse oRsp;
				ClientContext context;
				std::unique_ptr<RedisProxyService::Stub> oStub;
				oStub = RedisProxyService::NewStub(grpc::CreateChannel(sIp+":"+sPort, grpc::InsecureChannelCredentials()));
				Status status = oStub->RedisProxyCmd( &context, oReq, &oRsp );

				if (status.ok()) {
					printf( "bodylist_size:%d" , oRsp.oresultbodylist_size() );
					MYBOOST_LOG_DEBUG( OS_KV( "pthreadid", pthread_self() ) << OS_KV( "bodylistsize", oRsp.oresultbodylist_size() ) );
				} else {
					std::cout << OS_KV( "code", status.error_code() ) << OS_KV( "msg", status.error_message() ) << std::endl;
					MYBOOST_LOG_DEBUG( OS_KV( "pthreadid", pthread_self() ) 
							<< OS_KV( "code", status.error_code() ) << OS_KV( "msg", status.error_message() ) );
				}
			}
			int64_t nEndTime = TNOWMS;
			MYBOOST_LOG_DEBUG( OS_KV( "pthreadid", pthread_self() ) 
					<< OS_KV( "totalreq", oData.nQps ) << OS_KV( "threadnum", oData.nThreadNum ) 
					<< OS_KV( "pipelinenum", oData.nPipeLineNum ) << OS_KV( "totalcost", nEndTime - nStartTime )
					<< OS_KV( "percost", double(nEndTime-nStartTime)/double(oData.nQps) << OS_Kv( "qps", double(oData.nQps)/(nEndTime - nStartTime )/1000 ) ) ); 


		}
	private:
		std::unique_ptr<RedisProxyService::Stub> stub_;
};

int main(int argc, char** argv) {
	if( argc < 3 )
	{
		printf( "Usage: exe logpath logname ip port op[set|get|ping]\n ");
		return -1;
	}

	std::string sPath	=argv[1];
	std::string sBaseLogName = argv[2];
	std::string sIp		= argv[3];
	std::string sPort	= argv[4];
	std::string sOpt	= argv[5];
	int nThreadNum		= atoi(argv[6]);
	int nPipeLineNum	= atoi(argv[7]);
	int nQps			= atoi(argv[8]);
	g_InitMyBoostLog( sPath, sBaseLogName );
	MYBOOST_LOG_INFO(  "An informational severity message" );
	struct Data oData;
	oData.nPipeLineNum	= nPipeLineNum;
	oData.nQps			= nQps;
	int nErr;
	pthread_t nTid;
	for( size_t i = 0; i < nThreadNum; ++ i )
	{
		if( sOpt == "set" )
			nErr = pthread_create( &nTid, NULL, RedisProxyClient::DoRedisProxySet, (void*)&oData);
		else if( sOpt == "get" )
			nErr = pthread_create( &nTid, NULL, RedisProxyClient::DoRedisProxyGet, (void*)&oData);
		else if( sOpt == "ping" )
			nErr = pthread_create( &nTid, NULL, RedisProxyClient::DoRedisProxyPing, (void*)&oData);
	}

	return 0;
}
