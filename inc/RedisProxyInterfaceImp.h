/*************************************************************************
    > File Name: RedisProxyInterfaceImp.h
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 四 11/ 7 15:09:43 2019
 ************************************************************************/
#ifndef _RedisProxyInterfaceImp_H_
#define _RedisProxyInterfaceImp_H_
#include "redisproxy.grpc.pb.h"
#include "RedisProxyManager.h"
#include "redisproxy.pb.h"
#include "AICommon.pb.h"
#include <grpcpp/grpcpp.h>

using namespace std;
using namespace RedisProxy;
using namespace RedisProxyCommon;
using grpc::Status;

class RedisProxyInterfaceImp :public RedisProxyService::Service
{
	public:
		RedisProxyInterfaceImp();
		virtual ~RedisProxyInterfaceImp();

	private:
		bool	IsValidParam( const string & sMsgid, const RedisProxyRequest & oRedisProxyReq );
		bool	IsValidAuth( const RedisProxyRequest & oRedisProxyReq );
		void	TransferAppNameAndTableName( 
					const std::string & sAppName,
					const std::string & sTableName,
					RedisProxyRequest & oRedisProxyReq );
		void	ReportLatencyStatic( const int & nCost );

		virtual Status RedisProxyCmd(
				ServerContext * pContext, 
				const RedisProxyRequest * pRedisProxyReq,
				RedisProxyResponse * pRedisProxyRsp ) override;
};
#endif

