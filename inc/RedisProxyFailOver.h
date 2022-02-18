/*************************************************************************
    > File Name: RedisProxyFailOver.h
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 四  3/19 16:58:05 2020
 ************************************************************************/

#ifndef _RedisProxyFailOver_H_
#define _RedisProxyFailOver_H_
#include "util/tc_singleton.h"
#include "ServerDefine.h"
#include "ServerConfig.h"
#include "ZKProcess.h"
#include "RedisProxyCommon.pb.h"
#include "RedisConnectPoolFactory_Queue.h"

using namespace std;
using namespace taf;

class RedisProxyFailOver:
	public taf::TC_Singleton< RedisProxyFailOver >,
	public taf::TC_Thread
{
	public:
		// 参考 /Users/luoshaohua/Documents/GitWork/redis/src/cluster.c->clusterUpdateState
		// 难点一：集群出现故障，如何保证高可用?
		// 1. 记录集群下每个节点的状态
		// OK->FAIL
		// 2. 如果某个节点在一定策略下出现连续3次失败，则认为该节点处于不正常状态
		// 3. 标记该节点状态为FAIL，同时对请求到该节点的所有读请求切换到容灾服务，写请求仅写lmdb不写redis;
		// 那么这时候会出现不一致，如何保证一致性呢？如果仅做读请求保证最终一致性，保证可用性
		// 解决方案：
		// 3.1 写kafka(异常topic)的数据包含标志位和机器(待考虑是否需要)，用以表示该条数据记录是否为节点下线时数据，若是则在节点恢复后追加到redis中；
		// 3.2 节点下线时，Proxy向zk写入该下线节点的ip/port，还有此时写入到kafka的offset；
		// 3.3 守护进程监听zk下线节点事件，若有，则读取zk里面的offset并开始从kafka的offset位置开始读取数据追加到redis 中
		// 注意故障期间写数据到redis的时候避免数据再次落入到kafka中避免循环写入
		// 3.4 等待数据追加完成了，追加过程中读请求还是走lmdb，因为这时候的lmdb的数据完整性更高；
		// 3.5 Proxy在更新状态到ZK的时候需要加锁避免重复创建,避免多个Proxy更新时信任最旧的offset
		//
		// FAIL->OK
		// 4. 如果某个节点在一定策略下出现连续3次正常，则认为该节点处于正常状态
		// 5. 标记该节点状态为OK，同时对到lmdb的流量开关切换会到redis 上；
		// 
		// 如果短时间内整个集群都是FAIL状态，这样故障恢复的策略会不会慢？
		// 2. 难点二：集群节点出现故障时，读的请求是怎么处理？
		// 2.1 解决方案：节点下线时，Proxy向zk写入该下线节点的ip/port，还有此时写入到kafka的offset(同上)
		// 2.2 其他Proxy监听到zk上redis节点下线事件，对于原来的请求由超时处理
		// 2.3 对于新来的请求如果发现该redis节点下线，则路由请求到LMDB
		// 2.4 合并请求:正常节点请求和LMDB请求
		// 
		// 难点三：如果某个节点出现故障后，其他proxy是如何感知，如何保证其他proxy对于redis节点状态一致性？
		// 解决方案：因为节点的状态是存储在zk中，一旦节点状态发生更新后，通过Proxy对每个节点watch则会进行感知；
		// 一致性由zk进行保证
		//
		// 方案二：
		// 3.0.1 多个proxy发现同个实例故障时，不同proxy的往异常topic生产数据，并且写到zk(topic+parttion)
		// 3.0.2 守护进程监听到zk下事件，开始对kafka进行消费，并且记录offset的位置，把数据追加到redis中；
		// 3.0.3 当kafka中的数据全部消费完成，才认为节点正常，并且切换到读取redis
		void Init();

		int InitGroupsInfo( 
				MapStr2GIdGroupInfo & mapAppTable2GidGroupInfo );

		void UpdateGroupsInfo( 
				const std::string & sAppTable,
				MapInt2GroupInfo & mapGid2GroupInfo );

		int GetGroupState(
				const std::string & sAppTable, 
				const int & nGroupId );

		bool IsGroupFail( 
				const std::string & sAppTable, 
				const int & nGroupId );

		bool IsGroupOK( 
				const std::string & sAppTable, 
				const int & nGroupId );
		
		bool IsInstanceFail( 
				const std::string & sAppTable, 
				const int & nGroupId,
				const std::string & Ip, 
				const int & nPort );

		RedisProxyCommon::FailOverInfo & GenFailOverInfo( 
				RedisProxyCommon::GroupInfo & oGroupInfo, 
				RedisProxyCommon::FailOverInfo & oFailOverInfo,
				int nPartition );

		int UpdateGroupState(
				const std::string & sAppTable,
				const int nGroupId,
				int nGroupState );

		int UpdateGroupState( 
				const std::string & sAppTable,
				RedisProxyCommon::GroupInfo & oGroupInfo );

		int UpdateInstanceState( 
				const std::string & sAppTable, 
				const int & nGroupId,
				RedisProxyCommon::Instance & oInstance );

		int UpdateSlotState( 
				const std::string & sAppTable,
				const int & nGroupId,
				RedisProxyCommon::SlotInfo & oSlot );

		int UpdateClusterState(
				const std::string & sAppTable,
				RedisProxyCommon::ClusterInfo & oCluster );
	private:
		void run();

	private:
		MapStr2GIdGroupInfo _mapAppTable2GidGroupInfo;			// 记录AppTable到节点信息关系
		MapStr2ClusterInfo _mapAppTable2ClusterInfo;			// 记录AppTable到集群信息关系
                                                                // TODO 记录数据不一致的节点
};
#endif 
