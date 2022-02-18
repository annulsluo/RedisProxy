/*************************************************************************
    > File Name: redisProxyManager.h
    > Author: annulsluo
    > Mail: annulsluo@qq.com 
    > Created Time: 四 11/ 7 14:58:36 2019
 ************************************************************************/
#ifndef _RedisProxyManager_H_
#define _RedisProxyManager_H_

#include "redisproxy.pb.h"
#include "RedisProxyCommon.pb.h"
#include <grpcpp/grpcpp.h>
#include "DmpService.pb.h"
#include "RedisConnectCommon.h"
#include "ServerDefine.h"

using namespace std;
using namespace RedisProxy;
using namespace RedisProxyCommon;
using grpc::ServerContext;
using namespace dmp;

class RedisProxyManager
{
	public:
		RedisProxyManager();
		virtual ~RedisProxyManager();
		// 在此添加对外的操作函数，对齐impl类接口
		int DoRedisProxyCmd(
			const std::string & sMsgid,
			const RedisProxyRequest & oRedisProxyReq, 
			RedisProxyResponse & oRedisProxyRsp, 
			ServerContext * pContext );

		int DoCmdAgent( 
			const string & sMsgid,
			const RedisProxy::RequestBody & oRequestBody );
	public:
		// 在此添加对内的调用函数
		static void InnerRequestBody2CmdBody( 
			const InnerRequestBody & oInnerRequestBody, 
			CmdBody & oCmdBody );
		
		static void InsertPipeLineRsp2RedisProxyInnerRsp(
			bool bIsEmptyRsp,
			const PipeLineRequest & oPipeLineReq,
			const PipeLineResponse & oPipeLineRsp,
			RedisProxyInnerResponse & oRedisProxyInnerRsp );
		
		static void InnerRsp2RedisProxyRsp(
			const RedisProxyInnerResponse & oRedisProxyInnerRsp,
			RedisProxyResponse & oRedisProxyRsp );

		static void TransferInnerRsp2sRedisProxyRsp( 
			const RedisProxyInnerRequest & oInnerReq,
			const RedisProxyInnerResponse & oInnerRsp,
			RedisProxyResponse & oRedisProxyRsp );

		static bool IsWriteCmd( string & sCmd );
		static bool TransferCmd2KvElement( 
			vector< string >& vecItem, 
			dmp::RequestBody & oKvRequestBody,
			int32_t nTransferCmdMark );

        static bool GenFailTopicPipeLineReq(
            const string & sMsgid,
            const PipeLineRequest & oPipeLineReq,
            PipeLineRequest & oFailTopicPipeLineReq );

		static void TransferRedisProxyAppInfo2LmdbAppInfo(
			const RedisProxyCommon::AppInfo & oReqAppInfo,
			KVOperator & oKvOperReq );

		static bool TransferPipelineReq2KvOperReq( 
			const string & sMsgid, 
			const RedisProxyCommon::AppInfo & oAppInfo,
			const PipeLineRequest & oPipeLineReq,
			KVOperator & oKvOperReq,
			int32_t nTransferCmdMark );

		static void InsertKvOperRsp2RedisProxyInnerRsp(
			bool bIsEmptyRsp,
			const PipeLineRequest & oPipeLineReq,
			const KVResponse & oKvOperRsp,
			RedisProxyInnerResponse & oRedisProxyInnerRsp );

        static void GenMigratePipeLineReq( 
            vector< std::string > & vecMigrateKey, 
            GroupInfo & oGroupInfo,
            MigrateConf & oMigrateConf,
            RedisConf & oRedisConf,
            PipeLineRequest & oPipeLineReq );

	private:
		int TransferCmdStrToInt( const string & sCmd ) ;
		
		void TransferRedisProxyReq2InnerReq( 
			const string & sMsgid,
			const RedisProxyRequest & oRedisProxyReq, 
			RedisProxyInnerRequest & oRedisProxyInnerReq );

};
#endif

