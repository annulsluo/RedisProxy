/*************************************************************************
    > File Name: RedisProxyGuard.cpp.cpp
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 四  4/23 10:48:14 2020
 ************************************************************************/

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
#include "RedisProxyCommon.h"
#include <sys/syscall.h>
#include "RedisProxyZkData.h"
#include "RedisProxyKafka.h"
#include "client/cpp/RedisConnectPoolFactory_Queue.h"
#include "ServerConfig.h"

using namespace RedisProxy;

int main( int argc, char ** argv )
{
	// TODO 单点问题如何解决？
	// 0. 通过zk初始化各数据
	// 1. 定期对redis实例进行心跳检测，把实例状态(正常/下线)写入到Zk中
	// 2. 管理 redis 实例下线时对，计算节点对应Kafka-topic-partition，并且以failoverinfo 结构写入到zk中
	// 3. 对下线的 redis 实例partition 进行kafka 消费，并且以命令形式追加到 redis 实例中，消费记录保存到哪里呢？
	// 4. 追加完成的实例，把状态更新到 zk 中；如果实例恢复，但还没追加完成是什么状态？
	// 5. 如何在维护一份代码的情况下，能兼容到 ProxyGuaid 和 Server 可用性？
	RedisProxyZkData::getInstance()->InitGuard();
	MapStr2GIdGroupInfo & mapAppTable2GidGroupInfo = RedisProxyZkData::getInstance()->GetGIdGroupInfo();
	for( MapStr2GIdGroupInfo::iterator msIt = mapAppTable2GidGroupInfo.begin();
			msIt != mapAppTable2GidGroupInfo.end(); ++ msIt )
	{
		std::string sAppTable	= msIt->first;
		RedisConf & oRedisConf	= RedisProxyZkData::getInstance()->GetRedisConfByAppTable( sAppTable );
		RedisConnPoolConf & oRedisConnPoolConf	= 
				RedisProxyZkData::getInstance()->GetRedisConnPoolConfByAppTable( sAppTable );
		for( map< int, RedisProxyCommon::GroupInfo >::iterator mIt = msIt->second.begin();
				msIt->second.end(); mIt ++ )
		std::string sAppTableGroupId	= sAppTable + "." + TC_Common::tostr<int>( mIt->first );

		CRedisConnectPoolFactory_Queue::getInstance()->Init( 
				oRedisConf, oRedisConnPoolConf, sAppTableGroupId, true, 0, 0 );
	}

	// 如何监听到failoverlist的变化，并且进行消费
    /*
	while( oKafkaInfo.sBrokers.empty() )
	{
		oKafkaInfo	= RedisProxyZkData::getInstance()->GetKafkaInfo();
	}
	RedisProxyKafka::getInstance()->InitConsumer( oKafkaInfo );
    */
}

