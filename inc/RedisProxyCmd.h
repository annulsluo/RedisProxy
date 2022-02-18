/*************************************************************************
    > File Name: RedisProxyCmd.h
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 一 11/11 09:58:54 2019
 ************************************************************************/
#ifndef _RedisProxyCmd_H_
#define _RedisProxyCmd_H_

#include "AICommon.pb.h"
#include "redisproxy.pb.h"
#include "ServerDefine.h"
#include "RedisProxyCommon.pb.h"

using namespace RedisProxy;
using namespace RedisProxyCommon;
using namespace std;

class RedisProxyCmd
{
	public:
		RedisProxyCmd( const string & sMsgid,
						const RedisProxyInnerRequest & oRedisProxyInnerReq );

		virtual ~RedisProxyCmd();

	public:
		int HandleProcess( RedisProxyInnerResponse & oRedisProxyInnerRsp ) ;

	private:
		void ProduceKafka( string sTopicName, const KVOperator & oKvOperReq, string sPartitionKey = "" );

		void ProduceKafka( string sTopicName, const PipeLineRequest & oPipeLineReq, string sHost = "" );

    private:
        string				_sMsgid;
		RedisProxyInnerRequest	_oRedisProxyInnerReq;
 

};
#endif

