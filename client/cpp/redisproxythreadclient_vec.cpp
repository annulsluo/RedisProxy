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
	int nFieMem;
};

void* DoRedisProxyPing(void * pTmpData) {
	printf( "pthread start!\n" );
	struct Data *pData = (struct Data *)pTmpData;
	printf( "ProxySet pid:%u pipelinenum:%d qps:%d threadnum:%d ip:%s port:%s pData_addr:%d\n", 
			syscall(SYS_gettid), pData->nPipeLineNum, pData->nQps, pData->nThreadNum, pData->sIp, pData->sPort, pData );
	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps/pData->nThreadNum; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		oReq.set_sappid("redis_test");
		oReq.set_sapppw("redis_teset");
		oReq.set_sappname( "aiad" );
		oReq.set_stablename( "userprofile" );

		RequestHead &oRequestHead = *(oReq.mutable_orequesthead() );
		oRequestHead.set_ncallmode( CALLMODE_SINGLE );
		oRequestHead.set_ncalltype( CALLTYPE_SYNC);
		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			int nKey = 100000 + rand() % 100 + i;
			string sVal = ::GenerateUUID();
			oRequestBody.set_skey( "" );
			oRequestBody.set_scmd( "PING" );
			oRequestBody.set_svalue( "" );
			printf( "tid:%u index:%d key:%d cmd:set value:%s\n", syscall(SYS_gettid), int(i), nKey, sVal.c_str() );
		}

		try{
			RedisProxyResponse oRsp;
			ClientContext context;
			std::shared_ptr<Channel> oChannel(
					grpc::CreateChannel(string(pData->sIp)+":"+string(pData->sPort),grpc::InsecureChannelCredentials()));
			std::unique_ptr<RedisProxyService::Stub> oStub( RedisProxyService::NewStub( oChannel ) );
			Status status = oStub->RedisProxyCmd( &context, oReq, &oRsp );

			if (status.ok()) {

				printf( "tid:%u ok bodylist_size:%d\n" , syscall(SYS_gettid), oRsp.oresultbodylist_size() );
			} else {
				printf( "tid:%u fail.errorcode:%d msg:%s\n", syscall(SYS_gettid), status.error_code(), status.error_message().c_str() );
			}
		}
		catch( exception & e )
		{
			std::cout << "exception:" << e.what() << endl;
		}
	}
	int64_t nEndTime = TNOWMS;
	std::cout << OS_KV( "tid", syscall(SYS_gettid) ) 
		<< OS_KV( "totalreq", pData->nQps ) << OS_KV( "threadnum", pData->nThreadNum ) 
		<< OS_KV( "pipelinenum", pData->nPipeLineNum ) << OS_KV( "totalcost", nEndTime - nStartTime )
		<< OS_KV( "percost", double(nEndTime-nStartTime)/double(pData->nQps) ) 
		<< OS_KV( "qps", double(1)/(double( nEndTime - nStartTime )/1000/(pData->nQps*1)) ) << endl; 
}

void* DoRedisProxyGet(void * pTmpData) {
	struct Data *pData = (struct Data*)pTmpData;
	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps/pData->nThreadNum; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		oReq.set_sappid("redis_test");
		oReq.set_sapppw("redis_teset");
		oReq.set_sappname( "aiad" );
		oReq.set_stablename( "userprofile" );

		RequestHead &oRequestHead = *(oReq.mutable_orequesthead() );
		oRequestHead.set_ncallmode( CALLMODE_SINGLE );
		oRequestHead.set_ncalltype( CALLTYPE_SYNC);

		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			string sCmd = "";
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			oRequestBody.set_nver( VERSION_ELEMENTVECTOR );
			int nKey = 100000 + rand() % 10000 + i;

			RequestElement &oRequestElement	= *(oRequestBody.add_orequestelementlist());
			oRequestElement.set_nrequestelementtype( REQUESTELEMENTTYPE_CMD );
			oRequestElement.set_srequestelement( "get" );

			RequestElement &oRequestElementKey	= *(oRequestBody.add_orequestelementlist());
			oRequestElementKey.set_nrequestelementtype( REQUESTELEMENTTYPE_KEY );
			oRequestElementKey.set_nchartype( CHARTYPE_STRING );
			oRequestElementKey.set_srequestelement( TC_Common::tostr( nKey ) );
			
			for( size_t nEle = 0; nEle < oRequestBody.orequestelementlist_size(); ++ nEle )
			{
				sCmd = sCmd + oRequestBody.orequestelementlist(nEle).srequestelement() + " ";
			}
			printf( "tid:%u index:%d elelistsize:%d cmd:%s \n", 
					syscall(SYS_gettid), int(i), oRequestBody.orequestelementlist_size(), sCmd.c_str() );

		} 
		std::cout << "start RedisProxyCmd" << endl;
		try{
			RedisProxyResponse oRsp;
			ClientContext context;
			std::shared_ptr<Channel> oChannel(
					grpc::CreateChannel(string(pData->sIp)+":"+string(pData->sPort),
						grpc::InsecureChannelCredentials()));
			std::unique_ptr<RedisProxyService::Stub> oStub( RedisProxyService::NewStub( oChannel ) );
			Status status = oStub->RedisProxyCmd( &context, oReq, &oRsp );

			if (status.ok()) {
				printf( "tid:%u ok bodylist_size:%d\n" , syscall(SYS_gettid), oRsp.oresultbodylist_size() );
			} else {
				printf( "tid:%u fail.errorcode:%d msg:%s\n", syscall(SYS_gettid), status.error_code(), status.error_message().c_str() );
			}
		}
		catch( exception & e )
		{
			std::cout << "exception:" << e.what() << endl;
		}
	}
	int64_t nEndTime = TNOWMS;
	std::cout << OS_KV( "tid", syscall(SYS_gettid) ) 
		<< OS_KV( "totalreq", pData->nQps ) << OS_KV( "threadnum", pData->nThreadNum ) 
		<< OS_KV( "pipelinenum", pData->nPipeLineNum ) << OS_KV( "totalcost", nEndTime - nStartTime )
		<< OS_KV( "percost", double(nEndTime-nStartTime)/double(pData->nQps) ) 
		<< OS_KV( "qps", double(1)/(double( nEndTime - nStartTime )/1000/(pData->nQps*1)) ) << endl; 
}

void* DoRedisProxyExpire(void * pTmpData) {
	struct Data *pData = (struct Data*)pTmpData;
	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps/pData->nThreadNum; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		oReq.set_sappid("redis_test");
		oReq.set_sapppw("redis_teset");
		oReq.set_sappname( "aiad" );
		oReq.set_stablename( "userprofile" );

		RequestHead &oRequestHead = *(oReq.mutable_orequesthead() );
		oRequestHead.set_ncallmode( CALLMODE_PIPELINE );
		oRequestHead.set_ncalltype( CALLTYPE_SYNC);

		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			oRequestBody.set_nver( VERSION_ELEMENTVECTOR );
			int nKey = 100000 + rand() % 10000 + i;

			RequestElement &oRequestElement	= *(oRequestBody.add_orequestelementlist());
			oRequestElement.set_nrequestelementtype( REQUESTELEMENTTYPE_CMD );
			oRequestElement.set_srequestelement( "expire" );

			RequestElement &oRequestElementKey	= *(oRequestBody.add_orequestelementlist());
			oRequestElementKey.set_nrequestelementtype( REQUESTELEMENTTYPE_KEY );
			oRequestElementKey.set_nchartype( CHARTYPE_STRING );
			oRequestElementKey.set_srequestelement( TC_Common::tostr( nKey ) );

			RequestElement &oRequestElementSecond = *(oRequestBody.add_orequestelementlist());
			oRequestElementSecond.set_nchartype( CHARTYPE_STRING );
			oRequestElementSecond.set_srequestelement( TC_Common::tostr( nKey ) );
			
			printf( "tid:%u index:%d key:%d cmd:get\n", syscall(SYS_gettid), int(i), nKey ); 
		} 
		std::cout << "start RedisProxyCmd" << endl;
		try{
			RedisProxyResponse oRsp;
			ClientContext context;
			std::shared_ptr<Channel> oChannel(
					grpc::CreateChannel(string(pData->sIp)+":"+string(pData->sPort),
						grpc::InsecureChannelCredentials()));
			std::unique_ptr<RedisProxyService::Stub> oStub( RedisProxyService::NewStub( oChannel ) );
			Status status = oStub->RedisProxyCmd( &context, oReq, &oRsp );

			if (status.ok()) {
				printf( "tid:%u ok bodylist_size:%d\n" , syscall(SYS_gettid), oRsp.oresultbodylist_size() );
			} else {
				printf( "tid:%u fail.errorcode:%d msg:%s\n", syscall(SYS_gettid), status.error_code(), status.error_message().c_str() );
			}
		}
		catch( exception & e )
		{
			std::cout << "exception:" << e.what() << endl;
		}
	}
	int64_t nEndTime = TNOWMS;
	std::cout << OS_KV( "tid", syscall(SYS_gettid) ) 
		<< OS_KV( "totalreq", pData->nQps ) << OS_KV( "threadnum", pData->nThreadNum ) 
		<< OS_KV( "pipelinenum", pData->nPipeLineNum ) << OS_KV( "totalcost", nEndTime - nStartTime )
		<< OS_KV( "percost", double(nEndTime-nStartTime)/double(pData->nQps) ) 
		<< OS_KV( "qps", double(1)/(double( nEndTime - nStartTime )/1000/(pData->nQps*1)) ) << endl; 
}

void* DoRedisProxySet(void * pTmpData) {
	printf( "pthread start!\n" );
	// 子线程中获取的地址是一样的，但获取相应的结果却不一样
	struct Data *pData = (struct Data*)pTmpData;
	printf( "ProxySet pid:%u pipelinenum:%d qps:%d threadnum:%d ip:%s port:%s pData_addr:%d\n", 
			syscall(SYS_gettid), pData->nPipeLineNum, pData->nQps, pData->nThreadNum, pData->sIp, pData->sPort, pData );
	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps/pData->nThreadNum; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		oReq.set_sappid("redis_test");
		oReq.set_sapppw("redis_teset");
		oReq.set_sappname( "aiad" );
		oReq.set_stablename( "userprofile" );

		RequestHead &oRequestHead = *(oReq.mutable_orequesthead() );
		oRequestHead.set_ncallmode( CALLMODE_PIPELINE );
		oRequestHead.set_ncalltype( CALLTYPE_SYNC );

		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			string sCmd = "";
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			oRequestBody.set_nver( VERSION_ELEMENTVECTOR );
			int nKey = 100000 + rand() % 1000 + i;

			string sVal = ::GenerateUUID();
			
			RequestElement &oRequestElement	= *(oRequestBody.add_orequestelementlist());
			oRequestElement.set_nrequestelementtype( REQUESTELEMENTTYPE_CMD );
			oRequestElement.set_srequestelement( "set" );

			RequestElement &oRequestElementKey	= *(oRequestBody.add_orequestelementlist());
			oRequestElementKey.set_nrequestelementtype( REQUESTELEMENTTYPE_KEY );
			oRequestElementKey.set_nchartype( CHARTYPE_STRING );
			oRequestElementKey.set_srequestelement( TC_Common::tostr( nKey ) );

			RequestElement &oRequestElementValue	= *(oRequestBody.add_orequestelementlist());
			oRequestElementValue.set_nrequestelementtype( REQUESTELEMENTTYPE_VALUE );
			oRequestElementValue.set_nchartype( CHARTYPE_STRING );
			oRequestElementValue.set_srequestelement( sVal.c_str(), sVal.size() );

			int nExpire = 86400;
			RequestElement &oRequestElementEx	= *(oRequestBody.add_orequestelementlist());
			oRequestElementEx.set_nrequestelementtype( REQUESTELEMENTTYPE_EX );
			oRequestElementEx.set_nchartype( CHARTYPE_STRING );
			oRequestElementEx.set_srequestelement( TC_Common::tostr( nExpire ) );

			for( size_t nEle = 0; nEle < oRequestBody.orequestelementlist_size(); ++ nEle )
			{
				sCmd = sCmd + oRequestBody.orequestelementlist(nEle).srequestelement() + " ";
			}

			printf( "tid:%u index:%d cmd:%s \n", syscall(SYS_gettid), int(i), sCmd.c_str() );
		}
		std::cout << "start RedisProxyCmd" << endl;
		try{
			RedisProxyResponse oRsp;
			ClientContext context;
			std::shared_ptr<Channel> oChannel(
					grpc::CreateChannel(string(pData->sIp)+":"+string(pData->sPort),grpc::InsecureChannelCredentials()));
			std::unique_ptr<RedisProxyService::Stub> oStub( RedisProxyService::NewStub( oChannel ) );
			Status status = oStub->RedisProxyCmd( &context, oReq, &oRsp );

			if (status.ok()) {
				printf( "tid:%u ok bodylist_size:%d\n" , syscall(SYS_gettid), oRsp.oresultbodylist_size() );
			} else {
				printf( "tid:%u fail.errorcode:%d msg:%s\n", syscall(SYS_gettid), status.error_code(), status.error_message().c_str() );
			}
		}
		catch( exception & e )
		{
			std::cout << "exception:" << e.what() << endl;
		}
	}
	int64_t nEndTime = TNOWMS;
	std::cout << OS_KV( "tid", syscall(SYS_gettid) ) 
		<< OS_KV( "totalreq", pData->nQps ) << OS_KV( "threadnum", pData->nThreadNum ) 
		<< OS_KV( "pipelinenum", pData->nPipeLineNum ) << OS_KV( "totalcost", nEndTime - nStartTime )
		<< OS_KV( "percost", double(nEndTime-nStartTime)/double(pData->nQps) ) 
		<< OS_KV( "qps", double(1)/(double( nEndTime - nStartTime )/1000/(pData->nQps*1)) ) << endl; 
}

void* DoRedisProxyZAdd(void * pTmpData) {
	printf( "pthread start!\n" );
	// 子线程中获取的地址是一样的，但获取相应的结果却不一样
	struct Data *pData = (struct Data*)pTmpData;
	printf( "ProxySet pid:%u pipelinenum:%d qps:%d threadnum:%d fiemem:%d ip:%s port:%s pData_addr:%d\n", 
			syscall(SYS_gettid), pData->nPipeLineNum, pData->nQps, pData->nThreadNum, 
			pData->nFieMem, pData->sIp, pData->sPort, pData );
	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps/pData->nThreadNum; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		oReq.set_sappid("redis_test");
		oReq.set_sapppw("redis_teset");
		oReq.set_sappname( "aiad" );
		oReq.set_stablename( "userprofile" );

		RequestHead &oRequestHead = *(oReq.mutable_orequesthead() );
		oRequestHead.set_ncallmode( CALLMODE_PIPELINE );
		oRequestHead.set_ncalltype( CALLTYPE_SYNC );

		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			string sCmd = "";
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			oRequestBody.set_nver( VERSION_ELEMENTVECTOR );
			int nKey = 100000 + rand() % 1000 + i;

			RequestElement &oRequestElement	= *(oRequestBody.add_orequestelementlist());
			oRequestElement.set_srequestelement( "zadd" );

			RequestElement &oRequestElementKey	= *(oRequestBody.add_orequestelementlist());
			oRequestElementKey.set_nchartype( CHARTYPE_STRING );
			oRequestElementKey.set_srequestelement( TC_Common::tostr( nKey ) );

			string sXX = "XX";
			RequestElement &oRequestElementXX	= *(oRequestBody.add_orequestelementlist());
			oRequestElementXX.set_nchartype( CHARTYPE_STRING );
			oRequestElementXX.set_srequestelement( sXX.c_str(), sXX.size() );

			for( size_t nFieMem = 0; nFieMem < pData->nFieMem; ++ nFieMem )
			{
				int nScore = nFieMem;
				RequestElement &oRequestElementScore = *(oRequestBody.add_orequestelementlist());
				oRequestElementScore.set_nchartype( CHARTYPE_STRING );
				oRequestElementScore.set_srequestelement( TC_Common::tostr( nScore ) );

				string sMember = ::GenerateUUID();
				RequestElement &oRequestElementMember = *(oRequestBody.add_orequestelementlist());
				oRequestElementMember.set_nchartype( CHARTYPE_STRING );
				oRequestElementMember.set_srequestelement( sMember.c_str(), sMember.size() );
			}

			for( size_t nEle = 0; nEle < oRequestBody.orequestelementlist_size(); ++ nEle )
			{
				sCmd = sCmd + oRequestBody.orequestelementlist(nEle).srequestelement() + " ";
			}
			printf( "tid:%u index:%d cmd:%s \n", syscall(SYS_gettid), int(i), sCmd.c_str() );
		}
		std::cout << "start RedisProxyCmd" << endl;
		try{
			RedisProxyResponse oRsp;
			ClientContext context;
			std::shared_ptr<Channel> oChannel(
					grpc::CreateChannel(string(pData->sIp)+":"+string(pData->sPort),grpc::InsecureChannelCredentials()));
			std::unique_ptr<RedisProxyService::Stub> oStub( RedisProxyService::NewStub( oChannel ) );
			Status status = oStub->RedisProxyCmd( &context, oReq, &oRsp );

			if (status.ok()) {
				printf( "tid:%u ok bodylist_size:%d\n" , syscall(SYS_gettid), oRsp.oresultbodylist_size() );
			} else {
				printf( "tid:%u fail.errorcode:%d msg:%s\n", syscall(SYS_gettid), status.error_code(), status.error_message().c_str() );
			}
		}
		catch( exception & e )
		{
			std::cout << "exception:" << e.what() << endl;
		}
	}
	int64_t nEndTime = TNOWMS;
	std::cout << OS_KV( "tid", syscall(SYS_gettid) ) 
		<< OS_KV( "totalreq", pData->nQps ) << OS_KV( "threadnum", pData->nThreadNum ) 
		<< OS_KV( "pipelinenum", pData->nPipeLineNum ) << OS_KV( "totalcost", nEndTime - nStartTime )
		<< OS_KV( "percost", double(nEndTime-nStartTime)/double(pData->nQps) ) 
		<< OS_KV( "qps", double(1)/(double( nEndTime - nStartTime )/1000/(pData->nQps*1)) ) << endl; 

}
void* DoRedisProxyZcount(void * pTmpData) {
}
void* DoRedisProxyHset(void * pTmpData) {
	printf( "pthread start!\n" );
	// 子线程中获取的地址是一样的，但获取相应的结果却不一样
	struct Data *pData = (struct Data*)pTmpData;
	printf( "ProxySet pid:%u pipelinenum:%d qps:%d threadnum:%d fiemem:%d ip:%s port:%s pData_addr:%d\n", 
			syscall(SYS_gettid), pData->nPipeLineNum, pData->nQps, pData->nThreadNum, 
			pData->nFieMem, pData->sIp, pData->sPort, pData );
	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps/pData->nThreadNum; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		oReq.set_sappid("redis_test");
		oReq.set_sapppw("redis_teset");
		oReq.set_sappname( "aiad" );
		oReq.set_stablename( "userprofile" );

		RequestHead &oRequestHead = *(oReq.mutable_orequesthead() );
		oRequestHead.set_ncallmode( CALLMODE_PIPELINE );
		oRequestHead.set_ncalltype( CALLTYPE_SYNC );

		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			string sCmd = "";
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			oRequestBody.set_nver( VERSION_ELEMENTVECTOR );
			int nKey = 100000 + rand() % 1000 + i;

			RequestElement &oRequestElement	= *(oRequestBody.add_orequestelementlist());
			oRequestElement.set_srequestelement( "hset" );

			RequestElement &oRequestElementKey	= *(oRequestBody.add_orequestelementlist());
			oRequestElementKey.set_nchartype( CHARTYPE_STRING );
			oRequestElementKey.set_srequestelement( TC_Common::tostr( nKey ) );

			for( size_t nFieMem = 0; nFieMem < pData->nFieMem; ++ nFieMem )
			{
				string sFieldName = "Field" + TC_Common::tostr( nFieMem );
				RequestElement &oRequestElementField = *(oRequestBody.add_orequestelementlist());
				oRequestElementField.set_nchartype( CHARTYPE_STRING );
				oRequestElementField.set_srequestelement( sFieldName );

				string sValue	= ::GenerateUUID();
				RequestElement &oRequestElementValue = *(oRequestBody.add_orequestelementlist());
				oRequestElementValue.set_nchartype( CHARTYPE_STRING );
				oRequestElementValue.set_srequestelement( sValue.c_str(), sValue.size() );
			}

			for( size_t nEle = 0; nEle < oRequestBody.orequestelementlist_size(); ++ nEle )
			{
				sCmd = sCmd + oRequestBody.orequestelementlist(nEle).srequestelement() + " ";
			}
			printf( "tid:%u index:%d cmd:%s \n", syscall(SYS_gettid), int(i), sCmd.c_str() );
		}
		std::cout << "start RedisProxyCmd" << endl;
		try{
			RedisProxyResponse oRsp;
			ClientContext context;
			std::shared_ptr<Channel> oChannel(
					grpc::CreateChannel(string(pData->sIp)+":"+string(pData->sPort),grpc::InsecureChannelCredentials()));
			std::unique_ptr<RedisProxyService::Stub> oStub( RedisProxyService::NewStub( oChannel ) );
			Status status = oStub->RedisProxyCmd( &context, oReq, &oRsp );

			if (status.ok()) {
				printf( "tid:%u ok bodylist_size:%d\n" , syscall(SYS_gettid), oRsp.oresultbodylist_size() );
			} else {
				printf( "tid:%u fail.errorcode:%d msg:%s\n", syscall(SYS_gettid), status.error_code(), status.error_message().c_str() );
			}
		}
		catch( exception & e )
		{
			std::cout << "exception:" << e.what() << endl;
		}
	}
	int64_t nEndTime = TNOWMS;
	std::cout << OS_KV( "tid", syscall(SYS_gettid) ) 
		<< OS_KV( "totalreq", pData->nQps ) << OS_KV( "threadnum", pData->nThreadNum ) 
		<< OS_KV( "pipelinenum", pData->nPipeLineNum ) << OS_KV( "totalcost", nEndTime - nStartTime )
		<< OS_KV( "percost", double(nEndTime-nStartTime)/double(pData->nQps) ) 
		<< OS_KV( "qps", double(1)/(double( nEndTime - nStartTime )/1000/(pData->nQps*1)) ) << endl; 


}
void* DoRedisProxyExists(void * pTmpData) {
}
void* DoRedisProxyHmset(void * pTmpData) {
}
void* DoRedisProxyHget(void * pTmpData) {
}
void* DoRedisProxyHmget(void * pTmpData) {
}
void* DoRedisProxyHincBy(void * pTmpData) {
}
void* DoRedisProxySetex(void * pTmpData) {
}
void* DoRedisProxyIncryBy(void * pTmpData) {
}
void* DoRedisProxyZrangeByScore(void * pTmpData) {
}
void* DoRedisProxyZremrangeByScore(void * pTmpData) {
}
int main(int argc, char** argv) {
	if( argc < 3 )
	{
		printf( "Usage: exe logpath logname ip port cmd[set|get|ping|exists|zadd|zcount|hset|hmset|hget|hmget|hincBy|setex|incryBy|zrangeByScore|zremrangeByScore] threadnum pipelinenum qps field/member[hash/zadd]\n ");
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
	int nFieMem			= 0;
	if( argc > 9 )		nFieMem = atoi( argv[9] );
	
	g_InitMyBoostLog( sPath, sBaseLogName );
	MYBOOST_LOG_INFO(  "An informational severity message" );
	struct Data ** pData = (struct Data **)malloc( sizeof( struct Data * ) * nThreadNum );
	int nErr;
	pthread_t *nTid = (pthread_t*)malloc(sizeof(pthread_t)*nThreadNum );

	int64_t nStartTime = TNOWMS;
	for( size_t i = 0; i < nThreadNum; ++ i )
	{
		pData[i] = (struct Data*)malloc(sizeof(struct Data));
		pData[i]->nPipeLineNum 	= nPipeLineNum;
		pData[i]->nQps			= nQps;
		pData[i]->nThreadNum 	= nThreadNum;
		pData[i]->sIp		= argv[3];
		pData[i]->sPort		= argv[4];
		pData[i]->nFieMem	= nFieMem;

		printf( "pipelinenum:%d qps:%d threadnum:%d fieldmem:%d ip:%s port:%s pData->addr:%d\n", 
				pData[i]->nPipeLineNum, pData[i]->nQps, pData[i]->nThreadNum, pData[i]->nFieMem,
				pData[i]->sIp, pData[i]->sPort, pData[i] );
		if( sOpt == "set" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxySet, (void*)pData[i]);
		else if( sOpt == "get" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyGet, (void*)pData[i]);
		else if( sOpt == "exists" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyExists, (void*)pData[i]);
		else if( sOpt == "zadd" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyZAdd, (void*)pData[i]);
		else if( sOpt == "zcount" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyZcount, (void*)pData[i]);
		else if( sOpt == "hset" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyHset, (void*)pData[i]);
		else if( sOpt == "hmset" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyHmset, (void*)pData[i]);
		else if( sOpt == "hget" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyHget, (void*)pData[i]);
		else if( sOpt == "hmget" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyHmget, (void*)pData[i]);
		else if( sOpt == "hincBy" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyHincBy, (void*)pData[i]);
		else if( sOpt == "setex" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxySetex, (void*)pData[i]);
		else if( sOpt == "incryBy" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyIncryBy, (void*)pData[i]);
		else if( sOpt == "zrangeByScore" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyZrangeByScore, (void*)pData[i]);
		else if( sOpt == "zremrangeByScore" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyZremrangeByScore, (void*)pData[i]);
		else if( sOpt == "expire" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyExpire, (void*)pData[i]);
	}
	for( size_t i = 0; i < nThreadNum; ++ i )
	{
		int nRet;
		if( (nRet = pthread_join( nTid[i], NULL )) != 0 )
		{
			printf( "threadid %u is not exit.ret:%d\n", nTid[i], nRet );
		}
		else
		{
			printf( "threadid %u join succ.ret:%d\n", nTid[i], nRet );
		}
	}
	int64_t nEndTime = TNOWMS;
	// QPS=并发数/平均响应时间
	// 平均响应时间=总时间/总请求数
		cout << OS_KV( "totalreq", nQps*nThreadNum ) << OS_KV( "threadnum", nThreadNum ) 
		<< OS_KV( "pipelinenum", nPipeLineNum ) << OS_KV( "totalcost", nEndTime - nStartTime )
		<< OS_KV( "percost", double(nEndTime-nStartTime) ) 
		<< OS_KV( "qps", double(nThreadNum)/(double( nEndTime - nStartTime )/1000/(nQps*nThreadNum)) ) << endl; 
	return 0;
}
