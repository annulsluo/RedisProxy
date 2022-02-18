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

#define MAX_LATENCY_LEN 100
struct Data
{
	int nThreadNum;
	int nPipeLineNum;
	int nMemberNum;
	int nQps;
	int nExpire;
	int nRandKey;
	int nTimeout;
	char * sIp;
	char * sPort;
	char * sAppId;
	char * sAppPw;
	int nLatencyCnt[MAX_LATENCY_LEN];
};
string sDefaultValue = "0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-012345678-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789 -0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789";

BOOSTLogLevel glogLevel = BOOST_LOG_LEVEL_DEBUG;

void GenRequest( struct Data * pData, RedisProxyRequest & oReq )
{
	oReq.set_sappid( pData->sAppId );
	oReq.set_sapppw( pData->sAppPw );

	RequestHead &oRequestHead = *(oReq.mutable_orequesthead() );
	oRequestHead.set_ncallmode( CALLMODE_SINGLE );
	oRequestHead.set_ncalltype( CALLTYPE_SYNC);
}

void GenRequestBody( string & sKeyPre, string & sCmd, RequestBody & oRequestBody, 
		struct Data * pData, int nIndex, string &sKey )
{
	if( sKey.empty() )
	{
		sKey = sKeyPre + TC_Common::tostr(rand() % pData->nRandKey + nIndex);
	}
	string sVal = ::GenerateUUID();
		
	RequestElement &oRequestElementCmd = *(oRequestBody.add_orequestelementlist());
	oRequestElementCmd.set_srequestelement( sCmd );

	RequestElement &oRequestElementKey = *(oRequestBody.add_orequestelementlist());
	oRequestElementKey.set_nchartype( CHARTYPE_STRING );
	oRequestElementKey.set_srequestelement( sKey );

	oRequestBody.set_nver( VERSION_ELEMENTVECTOR );
}

void GenRequestExpireElement( RequestBody & oRequestBody, struct Data * pData, int nIndex )
{
	int nSeconds = 3600 + nIndex;
	RequestElement &oRequestElementScore = *(oRequestBody.add_orequestelementlist());
	oRequestElementScore.set_nchartype( CHARTYPE_STRING );
	oRequestElementScore.set_srequestelement( TC_Common::tostr( nSeconds ) );
}

void GenRequestSetElement( RequestBody & oRequestBody, struct Data * pData )
{
	string sVal = ::GenerateUUID();

	RequestElement &oRequestElementVal = *(oRequestBody.add_orequestelementlist());
	oRequestElementVal.set_nchartype( CHARTYPE_STRING );
	oRequestElementVal.set_srequestelement( sVal );
}

void GenRequestSetExElement( RequestBody & oRequestBody, struct Data * pData, int nIndex )
{
	string sVal = ::GenerateUUID();

	int nSeconds = 3600 +  nIndex;
	RequestElement &oRequestElementSeconds = *(oRequestBody.add_orequestelementlist());
	oRequestElementSeconds.set_nchartype( CHARTYPE_STRING );
	oRequestElementSeconds.set_srequestelement( TC_Common::tostr( nSeconds ) );

	RequestElement &oRequestElementVal = *(oRequestBody.add_orequestelementlist());
	oRequestElementVal.set_nchartype( CHARTYPE_STRING );
	oRequestElementVal.set_srequestelement( sVal );
}

void GenRequestSortSetElement( RequestBody & oRequestBody, struct Data * pData, int nIndex )
{
	int nScore = rand() % pData->nMemberNum + nIndex;
	string sVal = ::GenerateUUID();
	RequestElement &oRequestElementScore = *(oRequestBody.add_orequestelementlist());
	oRequestElementScore.set_nchartype( CHARTYPE_STRING );
	oRequestElementScore.set_srequestelement( TC_Common::tostr( nScore ) );

	RequestElement &oRequestElementVal = *(oRequestBody.add_orequestelementlist());
	oRequestElementVal.set_nchartype( CHARTYPE_STRING );
	oRequestElementVal.set_srequestelement( sVal );
}

void GenRequestHashElement( RequestBody & oRequestBody, struct Data * pData, int nIndex )
{
	string sField = "field" + TC_Common::tostr( nIndex );
	string sVal = ::GenerateUUID() + TC_Common::tostr(TNOW);
	RequestElement &oRequestElementScore = *(oRequestBody.add_orequestelementlist());
	oRequestElementScore.set_nchartype( CHARTYPE_STRING );
	oRequestElementScore.set_srequestelement( sField );

	RequestElement &oRequestElementVal = *(oRequestBody.add_orequestelementlist());
	oRequestElementVal.set_nchartype( CHARTYPE_STRING );
	oRequestElementVal.set_srequestelement( sVal );
}

void GenRequestGetHashElement( RequestBody & oRequestBody, struct Data * pData, int nIndex )
{
	string sField = "field" + TC_Common::tostr( nIndex );
	string sVal = ::GenerateUUID() + TC_Common::tostr(TNOW);
	RequestElement &oRequestElementScore = *(oRequestBody.add_orequestelementlist());
	oRequestElementScore.set_nchartype( CHARTYPE_STRING );
	oRequestElementScore.set_srequestelement( sField );

}

void GenRequestHincrElement( RequestBody & oRequestBody, struct Data * pData, int nIndex )
{
	string sField = "field" + TC_Common::tostr( nIndex );
	int nIncrement	= rand() % pData->nMemberNum + nIndex;
	RequestElement &oRequestElementScore = *(oRequestBody.add_orequestelementlist());
	oRequestElementScore.set_nchartype( CHARTYPE_STRING );
	oRequestElementScore.set_srequestelement( sField );

	RequestElement &oRequestElementVal = *(oRequestBody.add_orequestelementlist());
	oRequestElementVal.set_nchartype( CHARTYPE_STRING );
	oRequestElementVal.set_srequestelement( TC_Common::tostr( nIncrement ) );
}

void GenRequestZrangeByScoreElement( RequestBody & oRequestBody, struct Data * pData, int nIndex )
{
	int nIncrement	= rand() % pData->nMemberNum + nIndex;
	RequestElement &oRequestElementScore = *(oRequestBody.add_orequestelementlist());
	oRequestElementScore.set_nchartype( CHARTYPE_STRING );
	oRequestElementScore.set_srequestelement( "-inf" );

	RequestElement &oRequestElementVal = *(oRequestBody.add_orequestelementlist());
	oRequestElementVal.set_nchartype( CHARTYPE_STRING );
	oRequestElementVal.set_srequestelement( "+inf" );
}

void Send( RedisProxyRequest & oReq, struct Data * pData )
{
	try{
		RedisProxyResponse oRsp;
		ClientContext context;
		std::shared_ptr<Channel> oChannel(
				grpc::CreateChannel(string(pData->sIp)+":"+string(pData->sPort),grpc::InsecureChannelCredentials()));
		std::unique_ptr<RedisProxyService::Stub> oStub( RedisProxyService::NewStub( oChannel ) );
		Status status = oStub->RedisProxyCmd( &context, oReq, &oRsp );

		if (status.ok()) {
			//printf( "tid:%u ok bodylist_size:%d\n" , syscall(SYS_gettid), oRsp.oresultbodylist_size() );
		} else {
			printf( "tid:%u fail.errorcode:%d msg:%s\n", syscall(SYS_gettid), status.error_code(), status.error_message().c_str() );
		}
	}
	catch( exception & e )
	{
		std::cout << "exception:" << e.what() << endl;
	}
}

void CreateChannel( struct Data * pData, std::unique_ptr< RedisProxyService::Stub> &pStub )
{
	try{
		std::shared_ptr<Channel> oChannel(
				grpc::CreateChannel(string(pData->sIp)+":"+string(pData->sPort),grpc::InsecureChannelCredentials()));
		pStub = RedisProxyService::NewStub( oChannel ); 
	}
	catch( exception & e )
	{
		std::cout << "createchannel exception:" << e.what() << endl;
	}
}

void InitLatencyCnt( void * pTmpData )
{
	struct Data *pData = (struct Data*)pTmpData;
	for( size_t i = 0; i < MAX_LATENCY_LEN; ++ i )
	{
		pData->nLatencyCnt[i] = 0;
	}
}

void StubSend( RedisProxyRequest oReq, struct Data * pData, std::unique_ptr< RedisProxyService::Stub > &pStub )
{
	try{
		RedisProxyResponse oRsp;
		ClientContext context;
		gpr_timespec timespec;
		timespec.tv_sec		= 0;
		timespec.tv_nsec	= pData->nTimeout * 1000 * 1000;
		timespec.clock_type	= GPR_TIMESPAN;
		context.set_deadline( timespec );

		int64_t nStartTime	= TNOWMS;
		Status status		= pStub->RedisProxyCmd( &context, oReq, &oRsp );
		int64_t nEndTime	= TNOWMS;
		int64_t nLatency	= nEndTime - nStartTime;
		if( nLatency < MAX_LATENCY_LEN - 1  )
		{
			pData->nLatencyCnt[ nLatency ] ++;
			//printf( "latency:%d latencycnt:%d \n", nLatency, pData->nLatencyCnt[nLatency] );
		}
		else
		{
			pData->nLatencyCnt[ MAX_LATENCY_LEN - 1 ] ++;
			//printf( "latency:%d latencycnt:%d \n", nLatency, pData->nLatencyCnt[MAX_LATENCY_LEN-1] );
		}
		// 解释Rsp，判断每条的结果，如果有异常-1 则打印出来并且中断
		for( size_t i = 0; i < oRsp.oresultbodylist_size(); ++i )
		{
			if( oRsp.oresultbodylist(i).nret() < 0 || oRsp.oresultbodylist(i).svalue() == "Resource temporarily unavailable" )
			{
				printf( "tid:%u error ret:%d errmsg:%s\n" , syscall(SYS_gettid), 
						oRsp.oresultbodylist(i).nret(), oRsp.oresultbodylist(i).svalue().c_str() );
				exit(-1);
			}
			assert( oRsp.oresultbodylist(i).nret() >= 0 );
		}

		if( pData->nTimeout > 0 && nLatency > pData->nTimeout )
		{
			printf( "latency:%d req:%s rsp:%s", nLatency, oReq.DebugString().c_str(), oRsp.DebugString().c_str() );
		}

		if (status.ok()) {
			//printf( "tid:%u ok bodylist_size:%d\n" , syscall(SYS_gettid), oRsp.oresultbodylist_size() );
		} else {
			printf( "tid:%u fail.errorcode:%d msg:%s\n", syscall(SYS_gettid), status.error_code(), status.error_message().c_str() );
		}
	}
	catch( exception & e )
	{
		std::cout << "StubSend exception:" << e.what() << endl;
	}
}

void ExpireKey( vector< string > & vecKey, std::unique_ptr< RedisProxyService::Stub> &pStub, struct Data * pData )
{
	//std::cout << "start RedisProxyExpireKey." << endl;
	if( pData->nExpire )
	{
		for( size_t i = 0; i < vecKey.size(); ++ i )
		{
			RedisProxyRequest oReq;
			GenRequest( pData, oReq );
			//printf( "expire Key:%s", vecKey[i].c_str() );

			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = vecKey[i], sCmd = "expire", sKeyPre = "";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );

			GenRequestExpireElement( oRequestBody, pData, i );
			StubSend( oReq, pData, pStub );
		}
	}
}

void PrintfSingleRequest( struct Data * pData )
{
	printf( "pthread start! pid:%u pipelinenum:%d qps:%d threadnum:%d membernum:%d rankey:%d ip:%s \
			port:%s pData_addr:%d\n", 
			syscall(SYS_gettid), pData->nPipeLineNum, pData->nQps, pData->nThreadNum, 
			pData->nMemberNum, pData->nRandKey, pData->sIp, pData->sPort, pData );
}

void PrintfSingleResult( struct Data * pData, int64_t nStartTime, int64_t nEndTime )
{
	int64_t nSum = 0;
	for( size_t i = 0; i < 21; ++ i )
	{
		nSum += pData->nLatencyCnt[i];	
		if( i == 0 || i == 1 || i == 2 || i == 5 || i == 10 || i == 12 || i == 15 || i == 20 ){
			printf( "%.2f% <= %d millisecons\n", double(nSum)/pData->nQps*100, i );
		}
	}
	printf( "%.2f% > %d millisecons\n", double(pData->nQps-nSum)/pData->nQps*100, 20 );
	std::cout << OS_KV( "Single tid", syscall(SYS_gettid) ) 
		<< OS_KV( "totalreq", pData->nQps ) << OS_KV( "threadnum", pData->nThreadNum ) 
		<< OS_KV( "pipelinenum", pData->nPipeLineNum ) << OS_KV( "membernum", pData->nMemberNum )
		<< OS_KV( "randkey", pData->nRandKey ) << OS_KV( "totalcost", nEndTime - nStartTime )
		<< OS_KV( "percost", double(nEndTime-nStartTime)/double(pData->nQps) ) 
		<< OS_KV( "qps", double(1)/(double( nEndTime - nStartTime )/1000/(pData->nQps)) ) << endl; 
}

void* DoRedisProxyPing(void * pTmpData) {
	printf( "pthread start!\n" );
	struct Data *pData = (struct Data *)pTmpData;
	printf( "ProxySet pid:%u pipelinenum:%d qps:%d threadnum:%d ip:%s port:%s pData_addr:%d\n", 
			syscall(SYS_gettid), pData->nPipeLineNum, pData->nQps, pData->nThreadNum, pData->sIp, pData->sPort, pData );
	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		oReq.set_sappid("redis_test");
		oReq.set_sapppw("redis_teset");

		RequestHead &oRequestHead = *(oReq.mutable_orequesthead() );
		oRequestHead.set_ncallmode( CALLMODE_SINGLE );
		oRequestHead.set_ncalltype( CALLTYPE_SYNC);
		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			int nKey = rand() % pData->nQps + i;
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
	PrintfSingleRequest( pData );

	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );
		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "get", sKeyPre = "str_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
		} 
		StubSend( oReq, pData, pStub );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );
}

void* DoRedisProxySet(void * pTmpData) {
	struct Data *pData = (struct Data*)pTmpData;
	PrintfSingleRequest( pData );

	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	vector< string > vecKey;
	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );

		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "set", sKeyPre = "str_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
			vecKey.push_back( sKey );
			GenRequestSetElement( oRequestBody, pData );
		}
		StubSend( oReq, pData, pStub );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );
	ExpireKey( vecKey, pStub, pData );
}

void* DoRedisProxyExist(void * pTmpData) {
	struct Data *pData = (struct Data*)pTmpData;
	PrintfSingleRequest( pData );

	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );
		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "exist", sKeyPre = "str_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
		} 
		StubSend( oReq, pData, pStub );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );

}

void* DoRedisProxyZAdd(void * pTmpData) {
	printf( "pthread start!\n" );
	struct Data *pData = (struct Data*)pTmpData;
	printf( "ProxySet pid:%u pipelinenum:%d qps:%d threadnum:%d member:%d ip:%s port:%s pData_addr:%d\n", 
			syscall(SYS_gettid), pData->nPipeLineNum, pData->nQps, pData->nThreadNum, pData->nMemberNum, pData->sIp, pData->sPort, pData );

	vector< string > vecKey;

	int64_t nStartTime = TNOWMS;
	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );
		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "zadd", sKeyPre = "sortset_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
			vecKey.push_back( sKey );
			for( size_t j = 0; j < pData->nMemberNum; ++ j )
			{
				GenRequestSortSetElement( oRequestBody, pData, j );
			}
			printf( "tid:%u index:%d key:%s\n", syscall(SYS_gettid), int(i), sKey.c_str() );
		}
		Send( oReq, pData );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );

	ExpireKey( vecKey, pStub, pData );
}

void* DoRedisProxyZcount(void * pTmpData) {
	printf( "pthread start!\n" );
	struct Data *pData = (struct Data*)pTmpData;
	printf( "ProxySet pid:%u pipelinenum:%d qps:%d threadnum:%d membernum:%d ip:%s port:%s pData_addr:%d\n", 
			syscall(SYS_gettid), pData->nPipeLineNum, pData->nQps, pData->nThreadNum, 
			pData->nMemberNum, pData->sIp, pData->sPort, pData );

	int64_t nStartTime = TNOWMS;
	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );
		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "zcount", sKeyPre = "sortset_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
			printf( "tid:%u index:%d key:%s\n", syscall(SYS_gettid), int(i), sKey.c_str() );
		}
		StubSend( oReq, pData, pStub );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );

}

void* DoRedisProxyHset(void * pTmpData) {
	struct Data *pData = (struct Data*)pTmpData;
	PrintfSingleRequest( pData );

	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	vector< string > vecKey;
	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );
		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "hset", sKeyPre = "hash_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
			vecKey.push_back( sKey );
			for( size_t j = 0; j < pData->nMemberNum; ++ j )
			{
				GenRequestHashElement( oRequestBody, pData, j );
			}
		}
		StubSend( oReq, pData, pStub );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );

	ExpireKey( vecKey, pStub, pData );
}

void* DoRedisProxyHmset(void * pTmpData) {
	struct Data *pData = (struct Data*)pTmpData;
	PrintfSingleRequest( pData );

	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	vector< string > vecKey;
	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );
		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "hmset", sKeyPre = "hash_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
			vecKey.push_back( sKey );
			for( size_t j = 0; j < pData->nMemberNum; ++ j )
			{
				GenRequestHashElement( oRequestBody, pData, j );
			}
		}
		StubSend( oReq, pData, pStub );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );

	ExpireKey( vecKey, pStub, pData );
}

void* DoRedisProxyExpire(void * pTmpData) {
	struct Data *pData = (struct Data*)pTmpData;
	PrintfSingleRequest( pData );

	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );
		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "expire", sKeyPre = "str_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
		} 
		StubSend( oReq, pData, pStub );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );
}

void* DoRedisProxyHget(void * pTmpData) {
	struct Data *pData = (struct Data*)pTmpData;
	PrintfSingleRequest( pData );

	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );
		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "hget", sKeyPre = "hash_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
			for( size_t j = 0; j < pData->nMemberNum; ++ j )
			{
				GenRequestGetHashElement( oRequestBody, pData, j );
			}
		} 
		StubSend( oReq, pData, pStub );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );
}

void* DoRedisProxyHgetAll(void * pTmpData) {
	struct Data *pData = (struct Data*)pTmpData;
	PrintfSingleRequest( pData );

	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );
		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "hgetall", sKeyPre = "hash_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
		} 
		StubSend( oReq, pData, pStub );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );

}
void* DoRedisProxyHmget(void * pTmpData) {
	struct Data *pData = (struct Data*)pTmpData;
	PrintfSingleRequest( pData );

	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );
		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "hmget", sKeyPre = "hash_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
			for( size_t j = 0; j < pData->nMemberNum; ++ j )
			{
				GenRequestGetHashElement( oRequestBody, pData, j );
			}
		} 
		StubSend( oReq, pData, pStub );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );
}

void* DoRedisProxyHincrBy(void * pTmpData) {
	// 因为插入的字段不是数字，这种情况下，会返回什么呢？	
	struct Data *pData = (struct Data*)pTmpData;
	PrintfSingleRequest( pData );

	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	vector< string > vecKey;
	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );
		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "hicrby", sKeyPre = "hash_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
			vecKey.push_back( sKey );
			for( size_t j = 0; j < pData->nMemberNum; ++ j )
			{
				GenRequestHincrElement( oRequestBody, pData, j );
			}
		}
		StubSend( oReq, pData, pStub );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );
}

void* DoRedisProxySetex(void * pTmpData) {
	struct Data *pData = (struct Data*)pTmpData;
	PrintfSingleRequest( pData );

	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );

		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "setex", sKeyPre = "str_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
			GenRequestSetExElement( oRequestBody, pData, i );
		}
		StubSend( oReq, pData, pStub );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );

}

void* DoRedisProxyIncryBy(void * pTmpData) {
}

void* DoRedisProxyZrangeByScore(void * pTmpData) {
	struct Data *pData = (struct Data*)pTmpData;
	PrintfSingleRequest( pData );

	std::unique_ptr< RedisProxyService::Stub > pStub;
	CreateChannel( pData, pStub );

	int64_t nStartTime = TNOWMS;
	for( size_t nReqNum = 0; nReqNum < pData->nQps; ++ nReqNum )	
	{
		RedisProxyRequest oReq;
		GenRequest( pData, oReq );

		for( size_t i = 0; i < pData->nPipeLineNum; ++ i )
		{
			RequestBody &oRequestBody	= *(oReq.add_orequestbodylist());
			string sKey = "", sCmd = "zrangebyscore", sKeyPre = "sortset_";
			GenRequestBody( sKeyPre, sCmd, oRequestBody, pData, i, sKey );
			GenRequestZrangeByScoreElement( oRequestBody, pData, i );
		}
		StubSend( oReq, pData, pStub );
	}
	int64_t nEndTime = TNOWMS;
	PrintfSingleResult( pData, nStartTime, nEndTime );
}

class A
{
	public:
		A(){}

};
void* DoRedisProxyZremrangeByScore(void * pTmpData) {
}

void func()
{

}
int main(int argc, char** argv) {
	if( argc < 3 )
	{
		printf( "Usage: exe logpath logname ip port cmd[set|get|ping|exist|zadd|zcount|hset|hmset|hget|hgetall| \
				hmget|hincBy|setex|incryBy|zrangeByScore|zremrangeByScore] \
				threadnum pipelinenum qps appid apppw expireflag membernum randkey timeout\n ");
		return -1;
	}
	auto registry = std::make_shared<A>();
	std::thread thr(func);
	thr.join();

	std::string sPath	=argv[1];
	std::string sBaseLogName = argv[2];
	std::string sIp		= argv[3];
	std::string sPort	= argv[4];
	std::string sOpt	= argv[5];
	int nThreadNum		= atoi(argv[6]);
	int nPipeLineNum	= atoi(argv[7]);
	int nQps			= atoi(argv[8]);
	string sAppId		= argv[9];
	string sAppPw		= argv[10];
	int nMemberNum		= 0;
	int nExpire			= -1;
	int nRandKey		= 0;
	int nTimeout		= 0;
	if( argc > 10 )	nExpire = atoi( argv[11] );
	if( argc > 11 ) nMemberNum		= atoi(argv[12]);
	if( argc > 12 ) nRandKey	= atoi( argv[13] );
	if( argc > 13 ) nTimeout	= atoi( argv[14] );
	g_InitMyBoostLog( sPath, sBaseLogName, "debug" );
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
		pData[i]->nMemberNum	= nMemberNum;
		pData[i]->nExpire		= nExpire;
		pData[i]->nRandKey		= nRandKey;
		pData[i]->nTimeout		= nTimeout;
		pData[i]->sIp		= argv[3];
		pData[i]->sPort		= argv[4];
		pData[i]->sAppId	= argv[9];
		pData[i]->sAppPw	= argv[10];
		InitLatencyCnt( (void*)pData[i] );

		printf( "pipelinenum:%d qps:%d threadnum:%d membernum:%d randkey:%d timeout:%d appid:%s apppw:%s ip:%s port:%s pData->addr:%d\n", 
				pData[i]->nPipeLineNum, pData[i]->nQps, pData[i]->nThreadNum, pData[i]->nMemberNum, 
				pData[i]->nRandKey, pData[i]->nTimeout, pData[i]->sAppId, pData[i]->sAppPw, pData[i]->sIp, pData[i]->sPort, pData[i] );
		if( sOpt == "set" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxySet, (void*)pData[i]);
		else if( sOpt == "get" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyGet, (void*)pData[i]);
		else if( sOpt == "ping" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyPing, (void*)pData[i]);
		else if( sOpt == "exist" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyExist, (void*)pData[i]);
		else if( sOpt == "expire" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyExpire, (void*)pData[i]);
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
		else if( sOpt == "hgetall" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyHgetAll, (void*)pData[i]);
		else if( sOpt == "hmget" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyHmget, (void*)pData[i]);
		else if( sOpt == "hincrBy" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyHincrBy, (void*)pData[i]);
		else if( sOpt == "setex" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxySetex, (void*)pData[i]);
		else if( sOpt == "incryBy" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyIncryBy, (void*)pData[i]);
		else if( sOpt == "zrangeByScore" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyZrangeByScore, (void*)pData[i]);
		else if( sOpt == "zremrangeByScore" )
			nErr = pthread_create( &nTid[i], NULL, DoRedisProxyZremrangeByScore, (void*)pData[i]);
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
			//printf( "threadid %u join succ.ret:%d\n", nTid[i], nRet );
		}
	}
	int64_t nEndTime = TNOWMS;
	// QPS=并发数/(总耗时/总请求)
	// 平均响应时间=单并发耗时/单并发总请求
	// 对于设置过期的函数不能在这里统计，会漏算过期设置时占时间
		cout << OS_KV( "totalreq", nQps*nThreadNum ) << OS_KV( "threadnum", nThreadNum ) 
		<< OS_KV( "pipelinenum", nPipeLineNum ) << OS_KV( "totalcost", nEndTime - nStartTime )
		<< OS_KV( "totalpercost", double(nEndTime-nStartTime)/(nQps*nThreadNum) ) 
		<< OS_KV( "qps", double(nThreadNum)/(double( nEndTime - nStartTime )/1000/(nQps*nThreadNum)) ) << endl; 
	return 0;
}
