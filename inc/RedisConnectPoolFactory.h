#ifndef REDIS_CONNECT_POOL_FACTORY_H
#define REDIS_CONNECT_POOL_FACTORY_H
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <map>
#include <vector>
#include "util/tc_config.h"
#include "util/tc_singleton.h"
#include "util/tc_ex.h"
#include "hiredis/hiredis.h"
#include "util/tc_timeprovider.h"
#include "RedisConnectCommon.h"

/*
#define MAX_CONNECT 50				// 最大链接数
#define MIN_CONNECT 5				// 最小链接数
#define MIN_USEDCNT  2				// 最小使用次数
#define MAX_IDLETIME 600			// 最大空闲时间
#define DEFAULTKEEPALIVETIME 5		// 默认心跳检测时间
*/

using namespace std;
using namespace taf;

template < class T >
class CRedisConnectPoolFactory: 
	public taf::TC_Singleton< CRedisConnectPoolFactory<T>,  CreateStatic, DefaultLifetime >,
	public taf::TC_Thread
{
	public:
		class CRedisConnectItem
		{
			public:
				CRedisConnectItem( const RedisConf & oRedisConf );	
                ~CRedisConnectItem();
                friend class CRedisConnectPoolFactory;

            private:
				// 创建单个Redis链接
				int CreateConnectItem();

				// 保持长链接
				bool IsKeepAlive();

				// 保持长链接+随机时间算法
				bool IsKeepAliveV1( int nKeepAliveRand );
				
                // 销毁单个Redis链接
                void DestoryConnectItem();          

                // 回收Redis链接
                void RecoverConnectItem();          

				// 设置健康状况
				void SetConnectHealth( bool bHealthRedisConn );

            public:

                bool _bUsed;					// 是否正在被使用
                int _nItemId;					// 链接id, 主动分配的ID号
                int _nUsedCnt;					// 使用次数 
				int _nUsedTime;					// 最近使用时间
				bool _bNeedAuth;				// 是否需要进行密码验证
				RedisConf _oRedisConf;			// RedisConf配置信息
				redisContext *_pRedisConn;		// Redis执行句柄
				int	_nState;					// 状态, TODO 二期增加
				bool _bHealth;					// 表示健康状态：
        };

    public:
        CRedisConnectPoolFactory();
        ~CRedisConnectPoolFactory();
		
        // 初始化链接池(用于创建链接，并且加入到链接池中)
        void Init( 
				const RedisConf & oRedisConf, 
				const RedisConnPoolConf & oRedisConnPoolConf, 
				T Index, bool bIsKeepAliveRun, int nKeepAliveTime, int nKeepAliveRand );

        // 单个db的链接池创建 
        int CreateConnection( const RedisConf & oRedisConf, T Index );        

        // 获取单个db链接
        CRedisConnectItem * GetRedisConnect( T Index );    

        // 获取db的链接数
        size_t GetCurrSize( T Index );                               

        // 设置最大链接数
        void SetMaxSize( T Index, int iMaxSize );

        //销毁数据库连接池
        void DestoryConnPool();                                              
        
        // 回收链接
        void RecoverConnect( CRedisConnectItem * pRedisItem, T Index, bool bHealthRedisConn );

        void DestoryConnect( CRedisConnectItem * pRedisItem );
        // 获取被使用的链接数
        size_t GetUsedSize( T Index );

        // 开启线程进行管理
        // 定期管理Redis链接池 避免资源浪费
        // 1. 单位时间内空闲的链接会进行回收
		// 2. 定时做心跳检测
        void run();
        
	private:
		map< T, vector< CRedisConnectItem * > > _mapConnPool;				// 链接池 key=AppTableGroupId
		map< T, RedisConf > _mapRedisConf;									// 链接池 key=AppTableGroupid
		map< T, RedisConnPoolConf > _mapRedisConnPoolConf;					// 链接池不同表不同配置, key=AppTable
		map< T, RedisConnPoolStatus > _mapRedisConnPoolStatus;				// 链接池状态信息 key=AppTableGroupId
		bool _bIsKeepAliveRuning;                                           // 管理线程是否启动
		int _nKeepAliveTime;												// 心跳检测时间
		int _nKeepAliveRand;												// 心跳检测随机数
		int _nRedisState;													// redis集群状态 ok表示正常，fail 表示不能正常工作
		/*
		int _nCurrSize;														// 当前可使用的数量
		int _nMaxSize;														// 最大链接数
		int _nMinSize;														// 最小链接数
		int _nUsed;                                                         // 被使用个数
		int _nMinUsedCnt;													// 最少使用次数阀值
		int _nMaxIdleTime;													// 最大空闲时间
		int _nCreateSize;													// 每个链接创建数
		pthread_mutex_t _lock;                                              // 线程锁, 多个表使用同个锁，增加争抢的严重性
		*/
		//TC_ThreadRecMutex _olock;
		map< T, TC_ThreadRecMutex > _mapLock;
		

};

#endif 
