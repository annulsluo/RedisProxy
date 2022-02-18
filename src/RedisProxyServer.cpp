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
#include <grpcpp/resource_quota.h>

#include "redisproxy.grpc.pb.h"
#include "RedisProxyInterfaceImp.h"
#include "ServerDefine.h"
#include "RedisProxySlot.h"
#include "RedisProxyCommon.pb.h"
#include "ServerConfig.h"
#include "redisproxy.pb.h"
#include <boost/log/core.hpp>
#include "util/myboost_log.h"
#include "RedisConnectPoolFactory.h"
#include "RedisProxyRegister.h"
#include "RedisProxyZkData.h"
#include "RedisProxyPrometheus.h"
#include "DmplmdbInterfaceAgent.h"
#include "RedisProxyFailOver.h"
#include "RedisConnectPoolFactory_Queue.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using namespace RedisProxy;
using RedisProxy::RedisProxyRequest;
using RedisProxy::RedisProxyResponse;
using RedisProxy::RedisProxyService;
using namespace std;
using namespace taf;

BOOSTLogLevel glogLevel = BOOST_LOG_LEVEL_DEBUG;

void RunServer( const string & sFileName, const string & sIp, const string & sPort, 
		const string & sBasePath, const string & sLogLevel ) 
{
	// 通过服务配置的方式读取IP和端口
	TC_Config oConfig; 	
	if( sBasePath.empty() )
		string sBasePath	= "/data/dmp/app/RedisProxyServer/";
	string sServerName	= "RedisProxyServer";
	string sConfPath	= sBasePath + "/conf/" + sServerName + ".conf"; 
	if( !sFileName.empty() )
	{
		sConfPath = sFileName;
	}

	MY_TRY
		g_InitMyBoostLog( sBasePath, sServerName, TC_Common::lower( sLogLevel ) );
	MYBOOST_CATCH( "InitBoostLog Error.", LOG_ERROR );

	MY_TRY
		oConfig.parseFile( sConfPath );
	MYBOOST_CATCH( "Config ParseFile Error.", LOG_ERROR );
	
	ServerConfigMng::getInstance()->Init( oConfig );
	// 本机的Ip和端口需要先设置，然后consul 信息需要通过zk获取
	RedisProxyRegister::getInstance()->SetIp( sIp );
	RedisProxyRegister::getInstance()->SetPort( sPort );
	RedisProxyPrometheus::getInstance()->Init( sIp, sPort );
	RedisProxyZkData::getInstance()->Init();
    // zk 配置异步初始化所以需要等待一定时间
    sleep( SERVER_START_WAIT_TIME );
	DmplmdbInterfaceAgent::getInstance()->Init();

	string sSvrPort = sPort.empty()?ServerConfigMng::getInstance()->GetSObj( "svr_port" ):sPort;
	string sSvrIp	= sIp.empty()?ServerConfigMng::getInstance()->GetSObj( "svr_ip" ):sIp;
	std::string sSvrAddr( sSvrIp + ":" + sSvrPort );		
	RedisProxyInterfaceImp service;

	ServerBuilder builder;
	builder.AddListeningPort(sSvrAddr, grpc::InsecureServerCredentials());
	builder.RegisterService(&service);

	//  启动的时候启动线程数和链接数配置，但因为grpc对于线程管理机制存在bug(线程不会被回收导致大量资源耗光)，暂不使用该配置
    //  定期去githup上确认是有解决方案 TODO
	/*
	int nCqs = ServerConfigMng::getInstance()->GetIObj( "cqs" ) != 0?ServerConfigMng::getInstance()->GetIObj( "cqs" ):5;
	int nMinPollers = ServerConfigMng::getInstance()->GetIObj( "minpollers" ) != 0?ServerConfigMng::getInstance()->GetIObj( "minpollers" ):1;
	int nMaxPollers = ServerConfigMng::getInstance()->GetIObj( "maxpollers" ) != 0?ServerConfigMng::getInstance()->GetIObj( "maxpollers" ):5;
	//int nCqTimeoutMsec = ServerConfigMng::getInstance()->GetIObj( "cqstimeoutmsec" ) != 0?ServerConfigMng::getInstance()->GetIObj( "cqstimeoutmsec" ):10000;
	builder.SetSyncServerOption(ServerBuilder::SyncServerOption::NUM_CQS, nCqs);
	builder.SetSyncServerOption(ServerBuilder::SyncServerOption::MIN_POLLERS, nMinPollers );
	builder.SetSyncServerOption(ServerBuilder::SyncServerOption::MAX_POLLERS, nMaxPollers );
	//builder.SetSyncServerOption(ServerBuilder::SyncServerOption::CQ_TIMEOUT_MSEC, nCqTimeoutMsec );
	int nMaxThreads  = ServerConfigMng::getInstance()->GetIObj( "maxthreads" ) != 0?ServerConfigMng::getInstance()->GetIObj( "maxthreads" ):10;
	grpc::ResourceQuota oResourceQuota;
	oResourceQuota.SetMaxThreads( nMaxThreads );
	builder.SetResourceQuota( oResourceQuota );
	*/

	// 通过builder的方式设置服务，如何对server进行参数设置呢？
	std::unique_ptr<Server> server(builder.BuildAndStart());
	MYBOOST_LOG_INFO( "Init Server Succ." << OS_KV( "addr", sSvrIp ) << OS_KV( "port", sSvrPort ) );

	server->Wait();
}

int main(int argc, char** argv) {
	if( argc == 6 )
	{
		RunServer( argv[1], argv[2], argv[3], argv[4], argv[5] );
	}
	else 
	{
		printf( "exe conf ip port path loglevel[debug|warn|info|error].\n" );
		return -1;
	}
	return 0;
}
