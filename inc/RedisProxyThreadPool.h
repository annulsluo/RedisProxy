/*************************************************************************
    > File Name: RedisProxyThreadPool.h
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 三  7/22 14:53:04 2020
 ************************************************************************/
#ifndef _RedisProxyThreadPool_H_
#define _RedisProxyThreadPool_H_
#include "util/ProcessThreadPool.h"
#include "util/tc_thread_pool.h"
class RedisProxyThreadPool : public taf::TC_Singleton<RedisProxyThreadPool>
{
    public:
        void Init( );
        ProcessThreadPoolImp & GetThreadPool();

    protected:
        ProcessThreadPoolImp _oRedisProxyThreadPool;
};
#endif
