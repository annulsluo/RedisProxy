/*************************************************************************
    > File Name: RedisProxyBackEnd.h
    > Author: annulsluo
    > Mail: annulsluo@gmail.com
    > Created Time: 一 11/11 09:47:05 2019
 ************************************************************************/
#ifndef _RedisProxyBackEnd_H_
#define _RedisProxyBackEnd_H_

#include "hiredis/hiredis.h"
#include "redisproxy.pb.h"
#include "RedisProxyCommon.pb.h"
#include "RedisConnectPoolFactory.h"
#include "RedisConnectPoolFactory_Queue.h"

using namespace std;
using namespace RedisProxyCommon;

class RedisProxyBackEnd
{
	public:
		RedisProxyBackEnd( 
				const string & sMsgid,
				const GroupInfo & oGroupInfo,
				const AppTableInfo & oAppTableInfo,
				const PipeLineRequest & oPipeLineReq );

        RedisProxyBackEnd( 
                const string & sMsgid,
                const int & nGroupId,
                const string & sAppTable,
                const PipeLineRequest & oPipeLineReq );

		virtual ~RedisProxyBackEnd();

		int DoPipeLine( PipeLineResponse & oPipeLineRsp );

	private:
		void InitRedisConnect( 
                const GroupInfo & oGroupInfo, 
				const AppTableInfo & oAppTableInfo );

		void InitRedisConnect( 
                const string & sAppTable,
                const int & nGroupId );

		int CreateRedisConnect( 
                const string & sHost, 
                const int nPort, 
                const string & sPasswd );

		void FreeRedisConnect();

		void TransferRedisErrToProxyErr( 
                const int nRedisErr, 
                int & nProxyErr );

        void AddPipeResultElement( 
                redisReply * dele, 
                RedisProxy::ResultElement & oPipeResultElement,
                size_t nIndex );

	private:
		bool		_bNeedAuth;						// 是否需要进行密码验证
		int			_nRedisConnectMaxTryTimes;		// 最大尝试创建链接次数
		string			_sMsgid;					// 请求主ID
		int			    _nGroupId;
		GroupInfo		_oGroupInfo;				// Redis的实例组信息
		AppTableInfo	_oAppTableInfo;				// 应用表信息
        string          _sAppTable;
		PipeLineRequest		_oPipeLineReq;			// pipeline请求
		struct timeval		_oTimeout;				// Redis超时时间
		bool				_bHealthRedisConn;
		redisContext		*_pRedisConn;
		CRedisConnectPoolFactory<string>::CRedisConnectItem * _pItem;
		CRedisConnectPoolFactory_Queue<string>::CRedisConnectItem * _pQueueItem;
};
#endif

