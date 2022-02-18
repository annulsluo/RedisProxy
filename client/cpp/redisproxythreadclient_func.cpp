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
#include <sys/syscall.h>
#include <time.h>


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
	char * sIp;
	char * sPort;
	char * sStatus;
};

		

		void* DoRedisProxySet(void * pData) {
			printf( "pthread start!\n" );
			// 子线程中获取的地址是一样的，但获取相应的结果却不一样
			struct Data *oData = (struct Data*)pData;
			printf( "ProxySet pid:%ld pipelinenum:%d qps:%d threadnum:%d ip:%s port:%s pData_addr:%d\n", 
				(long int)syscall(224), oData->nPipeLineNum, oData->nQps, oData->nThreadNum, oData->sIp, oData->sPort, oData );
			//int64_t nStartTime = time(NULL);
			int64_t nStartTime = TNOWMS;
			for( size_t nReqNum = 0; nReqNum < oData->nQps/oData->nThreadNum; ++ nReqNum )	
			{
				RedisProxyRequest oReq;
				oReq.set_sappid("redis_test");
				oReq.set_sapppw("redis_teset");
				oReq.set_sappname( "aiad" );
				oReq.set_stablename( "userprofile" );

				RequestHead &oRequestHead = *(oReq.mutable_orequesthead() );
				oRequestHead.set_ncallmode( CALLMODE_SINGLE );
				oRequestHead.set_ncalltype( CALLTYPE_SYNC);

				for( size_t i = 0; i < oData->nPipeLineNum; ++ i )
				{
					RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
					int nKey = 100000 + rand() % 100 + i;
					string sVal = ::GenerateUUID();
					oRequestBody.set_skey( TC_Common::tostr( nKey ) );
					oRequestBody.set_scmd( "set" );
					oRequestBody.set_svalue( sVal );
					printf( "pthreadid:%d index:%d key:%d cmd:set value:%s\n", pthread_self(), int(i), nKey, sVal.c_str() );
					//MYBOOST_LOG_DEBUG( OS_KV("ptheadid", pthread_self() ) << OS_KV( "index", int(i) ) 
				//			<< OS_KV("key",nKey)<<"cmd:set"<< OS_KV("value",sVal) );
				}
				std::cout << "start RedisProxyCmd" << endl;
				try{
				RedisProxyResponse oRsp;
				ClientContext context;
				std::shared_ptr<Channel> oChannel(grpc::CreateChannel(string(oData->sIp)+":"+string(oData->sPort),grpc::InsecureChannelCredentials()));
				std::unique_ptr<RedisProxyService::Stub> oStub( RedisProxyService::NewStub( oChannel ) );
				Status status = oStub->RedisProxyCmd( &context, oReq, &oRsp );

				if (status.ok()) {
					printf( "ok bodylist_size:%d" , oRsp.oresultbodylist_size() );
					oData->sStatus = "ok";
			//		MYBOOST_LOG_DEBUG( OS_KV( "pthreadid", pthread_self()) << OS_KV( "bodylistsize", oRsp.oresultbodylist_size() ) );
				} else {
					std::cout << "fail "<< OS_KV( "code", status.error_code() ) << OS_KV( "msg", status.error_message() )
						<< std::endl;
					oData->sStatus = "fail";
			//		MYBOOST_LOG_DEBUG( OS_KV( "pthreadid", pthread_self() )
			//				<< OS_KV( "code", status.error_code() ) << OS_KV( "msg", status.error_message() ) );
				}
				}
				catch( exception & e )
				{
					std::cout << "exception:" << e.what() << endl;
				}
			}
			int64_t nEndTime = TNOWMS;
			//MYBOOST_LOG_DEBUG( OS_KV( "pthreadid", pthread_self() ) 
		//			<< OS_KV( "totalreq", oData->nQps ) << OS_KV( "threadnum", oData->nThreadNum ) 
		//			<< OS_KV( "pipelinenum", oData->nPipeLineNum ) << OS_KV( "totalcost", nEndTime - nStartTime )
		//			<< OS_KV( "percost", double(nEndTime-nStartTime)/double(oData->nQps) ) 
		//			<< OS_KV( "qps", double(oData->nQps)/(nEndTime - nStartTime )/1000 ) ); 
			std::cout << OS_KV( "pthreadid", syscall(SYS_gettid) ) 
					<< OS_KV( "totalreq", oData->nQps ) << OS_KV( "threadnum", oData->nThreadNum ) 
					<< OS_KV( "pipelinenum", oData->nPipeLineNum ) << OS_KV( "totalcost", nEndTime - nStartTime )
					<< OS_KV( "percost", double(nEndTime-nStartTime)/double(oData->nQps) ) << endl;
					//<< OS_KV( "qps", double(oData->nQps)/(nEndTime - nStartTime )/1000 ) << endl; 

		}

	int main(int argc, char** argv) {
	if( argc < 3 )
	{
		printf( "Usage: exe logpath logname ip port op[set|get|ping] threadnum pipelinenum qps\n ");
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
/*
	struct Data oData;
	oData->nPipeLineNum	= nPipeLineNum;
	oData->nQps			= nQps;
	oData->nThreadNum	= nThreadNum;
	oData->sIp			= sIp;
	oData->sPort			= sPort;
*/
	struct Data * oData ;
	oData = (struct Data*)malloc(sizeof( struct Data) );
	oData->nPipeLineNum 	= nPipeLineNum;
	oData->nQps 		= nQps;
	oData->nThreadNum 	= nThreadNum;
	oData->sIp		= argv[3];
	oData->sPort		= argv[4];
        printf( "pipelinenum:%d qps:%d threadnum:%d ip:%s port:%s oData->addr:%d\n", oData->nPipeLineNum, oData->nQps, oData->nThreadNum, oData->sIp, oData->sPort, oData );
	int nErr;
	// TODO 子线程的静态函数, 获取主线程结构变量(结构变量区分在类中声明，在类中没有声明)
	pthread_t nTid;
	for( size_t i = 0; i < nThreadNum; ++ i )
	{
		if( sOpt == "set" )
			nErr = pthread_create( &nTid, NULL, DoRedisProxySet, (void*)oData);
	}
	if(pthread_join(nTid, NULL)){printf( "thread is not exit..." ); return -2;}
        printf( "pipelinenum:%d qps:%d threadnum:%d ip:%s port:%s Status:%s\n", oData->nPipeLineNum, oData->nQps, oData->nThreadNum, oData->sIp, oData->sPort, oData->sStatus );
/*
	for( size_t i = 0; i < nThreadNum; ++ i )
	{
		pthread_join( 	
	}
*/

	return 0;
}
