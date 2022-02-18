/*************************************************************************
    > File Name: RedisProxyThreadPool.cpp
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 三  7/22 14:52:01 2020
 ************************************************************************/
#include "ServerConfig.h"
#include "RedisProxyThreadPool.h"

void RedisProxyThreadPool::Init()
{
    _oRedisProxyThreadPool.Init( ServerConfigMng::getInstance()->GetThreadPoolConfig() );
    _oRedisProxyThreadPool.Start();
}

ProcessThreadPoolImp & RedisProxyThreadPool::GetThreadPool()
{
    return _oRedisProxyThreadPool;
}

