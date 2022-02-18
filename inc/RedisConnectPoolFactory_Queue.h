#ifndef REDIS_CONNECT_POOL_FACTORY_QUEUE_H
#define REDIS_CONNECT_POOL_FACTORY_QUEUE_H
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <map>
#include <vector>
#include <queue>
#include "util/tc_config.h"
#include "util/tc_singleton.h"
#include "util/tc_ex.h"
#include "hiredis/hiredis.h"
#include "util/tc_timeprovider.h"
#include "RedisConnectCommon.h"

using namespace std;
using namespace taf;

template < class T >
class CRedisConnectPoolFactory_Queue: 
	public taf::TC_Singleton< CRedisConnectPoolFactory_Queue<T>,  CreateStatic, DefaultLifetime >,
	public taf::TC_Thread
{
	public:
		class CRedisConnectItem
		{
			public:
				CRedisConnectItem( const RedisConf & oRedisConf );	
                ~CRedisConnectItem();
                friend class CRedisConnectPoolFactory_Queue;

            private:
				// ��������Redis����
				int CreateConnectItem();

				// ���ֳ�����+���ʱ���㷨
				bool IsKeepAliveV1();
				
                // ���ٵ���Redis����
                void DestoryConnectItem();          

                // ����Redis����
                void RecoverConnectItem();          

				// ���ý���״��
				void SetConnectHealth( bool bHealthRedisConn );

            public:

                bool _bUsed;					// �Ƿ����ڱ�ʹ��
                int _nItemId;					// ����id, ���������ID��
                int _nUsedCnt;					// ʹ�ô��� 
				int _nUsedTime;					// ���ʹ��ʱ��
				bool _bNeedAuth;				// �Ƿ���Ҫ����������֤
				RedisConf _oRedisConf;			// RedisConf������Ϣ
				redisContext *_pRedisConn;		// Redisִ�о��
				int	_nState;					// ״̬, TODO ��������
				bool _bHealth;					// ��ʾ����״̬��
        };

    public:
        CRedisConnectPoolFactory_Queue();
        ~CRedisConnectPoolFactory_Queue();
		
        // ��ʼ�����ӳ�(���ڴ������ӣ����Ҽ��뵽���ӳ���)
        void Init( 
				const RedisConf & oRedisConf, 
				const RedisConnPoolConf & oRedisConnPoolConf, 
				T Index,
                bool bIsFromGuard = false );

        void InitRedisConnectPoolFactoryByZk( bool bIsFromGuard = false );
        void InitRedisConnectPoolFactoryByGuard();
        void InitByGuard( 
				const RedisConf & oRedisConf, 
				const RedisConnPoolConf & oRedisConnPoolConf, 
				T Index );

        // ����db�����ӳش��� 
        int CreateConnection( const RedisConf & oRedisConf, T Index );        

        // ��ȡ����db����
        CRedisConnectItem * GetRedisConnect( T Index );    

        // ��ȡdb��������
        size_t GetCurrSize( T Index );                               

        // �������������
        void SetMaxSize( T Index, int iMaxSize );

        //�������ݿ����ӳ�
        void DestoryConnPool();                                              
        
        // ��������
        void RecoverConnect( CRedisConnectItem * pRedisItem, T Index, bool bHealthRedisConn );

        void DestoryConnect( CRedisConnectItem * pRedisItem, T Index );
        // ��ȡ��ʹ�õ�������
        size_t GetUsedSize( T Index );
        
        // �ж��Ƿ��������
        bool DealIdleConnect( CRedisConnectItem * pRedisItem, T Index );
		
		// ���÷Ǳ��������
		bool ResetConnect( CRedisConnectItem * pRedisItem, T Index );
		
		// ���þٱ�״̬����
		bool ResetReportDetectConnect( CRedisConnectItem * pRedisItem, T Index );

		void ReportDetectState( bool bIsPfail, T Index );

        // �����߳̽��й���
        // ���ڹ���Redis���ӳ� ������Դ�˷�
        // 1. ��λʱ���ڿ��е����ӻ���л���
		// 2. ��ʱ���������
        void run();
        
	private:
		map< T, queue< CRedisConnectItem * > > _mapConnPool;				// ���ӳ� key=AppTableGroupId
		map< T, queue< CRedisConnectItem * > > _mapUnHealthConnPool;		// �����������Ӷ���
		map< T, queue< CRedisConnectItem * > > _mapReportDetectConnPool;	// ��������ϱ�����
		map< T, RedisConf > _mapRedisConf;									// ���ӳ� key=AppTableGroupid
		map< T, RedisConnPoolConf > _mapRedisConnPoolConf;					// ���ӳز�ͬ��ͬ����, key=AppTable
		map< T, RedisConnPoolStatus > _mapRedisConnPoolStatus;				// ���ӳ�״̬��Ϣ key=AppTableGroupId
		bool _bIsKeepAliveRuning;                                           // �����߳��Ƿ�����
		int _nKeepAliveTime;												// �������ʱ��
		int _nKeepAliveRand;												// ������������
		/*
		int _nCurrSize;														// ��ǰ��ʹ�õ�����
		int _nMaxSize;														// ���������
		int _nMinSize;														// ��С������
		int _nUsed;                                                         // ��ʹ�ø���
		int _nMinUsedCnt;													// ����ʹ�ô�����ֵ
		int _nMaxIdleTime;													// ������ʱ��
		int _nCreateSize;													// ÿ�����Ӵ�����
		pthread_mutex_t _lock;                                              // �߳���, �����ʹ��ͬ����������������������
		*/
		map< T, TC_ThreadRecMutex > _mapLock;
		

};

#endif 
