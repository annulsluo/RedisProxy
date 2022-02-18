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
#define MAX_CONNECT 50				// ���������
#define MIN_CONNECT 5				// ��С������
#define MIN_USEDCNT  2				// ��Сʹ�ô���
#define MAX_IDLETIME 600			// ������ʱ��
#define DEFAULTKEEPALIVETIME 5		// Ĭ���������ʱ��
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
				// ��������Redis����
				int CreateConnectItem();

				// ���ֳ�����
				bool IsKeepAlive();

				// ���ֳ�����+���ʱ���㷨
				bool IsKeepAliveV1( int nKeepAliveRand );
				
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
        CRedisConnectPoolFactory();
        ~CRedisConnectPoolFactory();
		
        // ��ʼ�����ӳ�(���ڴ������ӣ����Ҽ��뵽���ӳ���)
        void Init( 
				const RedisConf & oRedisConf, 
				const RedisConnPoolConf & oRedisConnPoolConf, 
				T Index, bool bIsKeepAliveRun, int nKeepAliveTime, int nKeepAliveRand );

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

        void DestoryConnect( CRedisConnectItem * pRedisItem );
        // ��ȡ��ʹ�õ�������
        size_t GetUsedSize( T Index );

        // �����߳̽��й���
        // ���ڹ���Redis���ӳ� ������Դ�˷�
        // 1. ��λʱ���ڿ��е����ӻ���л���
		// 2. ��ʱ���������
        void run();
        
	private:
		map< T, vector< CRedisConnectItem * > > _mapConnPool;				// ���ӳ� key=AppTableGroupId
		map< T, RedisConf > _mapRedisConf;									// ���ӳ� key=AppTableGroupid
		map< T, RedisConnPoolConf > _mapRedisConnPoolConf;					// ���ӳز�ͬ��ͬ����, key=AppTable
		map< T, RedisConnPoolStatus > _mapRedisConnPoolStatus;				// ���ӳ�״̬��Ϣ key=AppTableGroupId
		bool _bIsKeepAliveRuning;                                           // �����߳��Ƿ�����
		int _nKeepAliveTime;												// �������ʱ��
		int _nKeepAliveRand;												// ������������
		int _nRedisState;													// redis��Ⱥ״̬ ok��ʾ������fail ��ʾ������������
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
		//TC_ThreadRecMutex _olock;
		map< T, TC_ThreadRecMutex > _mapLock;
		

};

#endif 
