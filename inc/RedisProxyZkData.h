/*************************************************************************
    > File Name: RedisProxyZkData.h
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 一  2/17 15:28:08 2020
 ************************************************************************/
#ifndef _REDISPROXYZKDATA_H_
#define _REDISPROXYZKDATA_H_

#include "ServerConfig.h"
#include "ZKProcess.h"
#include "util/tc_singleton.h"
#include "util/tc_common.h"
#include "ServerDefine.h"
#include "RedisProxyCommon.pb.h"
#include "RedisConnectPoolFactory.h"
#include "RedisProxyAppFlowControl.h"
#include "AICommon.pb.h"
using namespace taf;
using namespace aiad;

class RedisProxyZkData:
	public taf::TC_Singleton< RedisProxyZkData >
{
	public:
		void Init();
        void InitGuard();
		const RedisProxyCommon::AppInfo & GetAppInfoByAppId( 
				const std::string & sAppId );

		const ConsulInfo & GetConsulInfo( );

		const vector<std::string> & GetRedisTableList();

		bool SetGroupData( RedisProxyCommon::GroupInfo & oGroupInfo );
		bool DelGroupData( RedisProxyCommon::GroupInfo & oGroupInfo );
        void SetRebalanceSC2SlotConf( const std::string & sAppTable );
		void DelAppTableGroupData( const std::string & sAppTable );

		bool SetFailOvers( 
				RedisProxyCommon::FailOverInfo & oFailOverInfo );

        bool DelFailOver( const string & sAppTable, const string & sHost );

		bool IsFirstInitFinish();

		const RedisConf & GetRedisConfByAppTable( const std::string & sAppTable );

		const RedisConnPoolConf & GetRedisConnPoolConfByAppTable( const std::string & sAppTable );
		
		const GroupInfo & GetGroupInfoByAppTableGid( 
                const std::string & sAppTable, 
                const int nGroupId );

		const MapStr2GIdGroupInfo & GetGIdGroupInfo();
        
        const MapStr2FailOverInfo & GetHost2FailOverInfos();

		KafkaInfo GetKafkaInfo();
		
		const SlotConf & GetSlotConfByAppTable( const std::string & sAppTable );

        const MigrateConf & GetMigrateConf( const std::string & sAppTable );

		bool UpdateSlotAction( 
				const std::string & sAppTable,
				const RedisProxyCommon::SlotAction & oSlotAction );

        std::string GetAppTableStateByAppTable( const std::string & sAppTable );
    private:

        static void ConsulInfoCallback( 
				const std::string & sPath, const std::string & sValue,
                void * pCtx, const struct Stat * pStat );
        static int DecodeConsuleInfo( 
				const std::string & sConsulInfo, ConsulInfo & oConsulInfo );

		static void KafkaInfoCallback( 
				const std::string & sPath, const std::string & sValue,
				void * pCtx, const struct Stat * pStat );
			
		static int DecodeKafkaInfo( 
				const std::string & sKafkaInfo, KafkaInfo& oKafkaInfo );

        static void AppInfoCallback( 
				const std::string & sPath, const std::string & sValue,
                void * pCtx, const struct Stat * pStat );
        static int DecodeAppInfo( 
				const std::string & sAppInfo, 
				map< std::string, RedisProxyCommon::AppInfo > & mapAppid2AppInfo );

		static void RedisConfCallback( 
				const std::string & sPath, const std::string & sValue,
				void * pCtx, const struct Stat * pStat );
        static int DecodeRedisConf( 
				const std::string & sRedisConf, 
				RedisConf & oRedisConf );

        static void GenRedisConnPoolInfo( 
                const std::string & sPath, const std::string & sValue, void * pCtx );
        static void RedisConnPoolInfoCallback( 
				const std::string & sPath, const std::string & sValue,
                void * pCtx, const struct Stat * pStat );
        static int DecodeRedisConnPoolInfo( 
				const std::string & sRedisConnPoolInfo, 
				RedisConnPoolConf & oRedisConnPoolConf );

        static void RedisSlotConfCallback( 
				const std::string & sPath, const std::string & sValue,
                void * pCtx, const struct Stat * pStat );
        static int DecodeSlotConf( 
				const std::string & sSlotConf, RedisProxyCommon::SlotConf & oSlotConf );

        static void RedisMigrateConfCallback( 
				const std::string & sPath, const std::string & sValue,
                void * pCtx, const struct Stat * pStat );
        static int DecodeMigrateConf( 
				const std::string & sMigrateConf, MigrateConf & oMigrateConf );

        static int DecodeGroupsRebalance( 
                const std::string & sGroupsRebalance, 
                const std::string & sAppTable,
                vector< RedisProxyCommon::GroupInfo > & vecSetGroup,
                vector< RedisProxyCommon::GroupInfo > & vecDelGroup );

        /*static void RedisSlotActionCallback( 
				const std::string & sPath, const std::string & sValue,
                void * pCtx, const struct Stat * pStat );
                */
        static int DecodeSlotAction( 
				const std::string & sSlotAction, MapInt2SlotAction & mapSlotId2SA );
        /*
		static bool UpdateSlotAction( 
				const std::string & sAppTable,
				RedisProxyCommon::SlotAction & oSlotAction );
        static void GenRedisSlotAction(
                const std::string & sPath, const std::string & sValue, void * pCtx );
                */
		// 要为每个节点在zk创建路径
		
		static void RedisGroupDataCallback(
				const std::string & sPath, const std::string & sValue,
				void * pCtx, const struct Stat * pStat );
		static void RedisGroupsChildCallback(
				const std::string & sPath, const vector< std::string > & vecChild,
				void * pCtx, const struct Stat * pStat );
		static int DecodeRedisGroupsInfo(
				const std::string & sGroupsInfo, RedisProxyCommon::GroupInfo & oGroupInfo );
		static void UpdateFailOvers( 
				RedisProxyCommon::FailOverInfo & oFailOverInfo,
				MapStr2FailOverInfo & mapHost2FailOverInfo );
		static string EncodeFailovers( 
				MapStr2FailOverInfo & mapHost2FailOverInfo );
		static int DecodeRedisFailOverInfos( 
				const std::string & sFailOverInfo, 
                const std::string & sAppTable,
				MapStr2FailOverInfo & mapHost2FailOverInfo );

		static int DecodeAppTableList( 
				const std::string & sAppTableList,
				MapStr2Int & mapAppTable2State );
		static void RedisAppTableListCallback(
				const std::string & sPath, const std::string & sValue,
				void * pCtx, const struct Stat * pStat );

		static string TransferGroupState2Str( int nState );
		static int TransferGroupState2Int( string sState );
		static int TransferSlotState2Int( const string & sState );
        static string TransferSlotState2Str( int nState );
		static string EncodeSlotAction( MapInt2SlotAction & mapSlotId2SlotAction );
		static string EnCodeGroups( RedisProxyCommon::GroupInfo & oGroupInfo );
        static string EncodeSlotConf( RedisProxyCommon::SlotConf & oSlotConf );
		static int TransferAppTableStateStr2Int( string sState );
		static string TransferAppTableStateInt2Str( int nState );

	private: 
		void InitAppInfo();
		void InitConsulInfo();
		void InitKafkaInfo();
		static void InitRedisConf( const std::string & sAppTable, void * pCtx );
		static void InitRedisConnPoolInfo( const std::string & sAppTable, void * pCtx );
		static void InitRedisSlotConf( const std::string & sAppTable, void * pCtx );
		void InitRedisAppTableList();
		static void InitRedisGroupsInfo( const std::string & sAppTable, void * pCtx );
		static void InitRedisSlotAction( const std::string & sAppTable, void * pCtx );
        /*
        static void CalDiffGroupId( 
                const std::string & sNewGroup2Slot, 
                const std::string & sOldGroup2Slot,
                vector< int > & vecSetGroupId,
                vector< int > & vecDeleteGroupId );
                */
        void InitKafkaInfoByGuard();
        void InitRedisAppTableListByGuard();
        void InitRedisSlotsConfByGuard();
        void InitRedisGroupsRebalanceByGuard( const std::string & sAppTable );
        void InitRedisGroupsInfoByGuard( const std::string & sSourceAppTable );
        void InitRedisConfByGuard( const std::string & sAppTable );
        void InitRedisConnPoolInfoByGuard( const std::string & sAppTable );
        void InitRedisSlotConfByGuard( const std::string & sAppTable );
		void InitRedisSlotActionByGuard( const std::string & sAppTable );
        void InitRedisMigrateConfByGuard( const std::string & sAppTable );

	private:
		std::string _sRootPath;
		int _nDiffIndex;
		ZKProcess _oZkProcess;
		MapStr2AppInfo _mapAppid2AppInfo;
		AppInfo _oEmptyAppInfo;
		MapStr2RedisConf _mapAppTable2RedisConf;							// 表名映射到redis配置
		RedisConf _oEmptyRedisConf;
		MapStr2RedisConnPoolConf _mapAppTable2RedisConnPoolConf;			// 表名映射到链接池配置
		RedisConnPoolConf _oEmptyRedisConnPoolConf;
		MapStr2SlotConf	_mapAppTable2SlotConf;								// 表名映射到slot配置
		SlotConf _oEmptySlotConf;
        MapStr2SlotConf _mapAppTable2RebalanceSC;                           // 表名映射到rebalance配置
		MapStr2ISlotAction _mapAppTable2ISlotAction;						// 表名映射到slotid->slotaction
		SlotAction _oEmptySlotAction;

		ConsulInfo _oConsulInfo;
		map< std::string, RedisProxyCommon::Instance > _mapIndex2InstanceInfo;
		MapStr2GIdGroupInfo _mapAppTable2GidGroupInfo;
		GroupInfo _oEmptyGroupInfo;
		MapStr2FailOverInfo _mapHost2FailOverInfo;							// 实例到下线实例信息
		KafkaInfo _oKafkaInfo;
		MapStr2Int _mapAppTable2State;
		MapStr2SlotGroupInfo _mapAppTable2SlotGroupInfo;					// 记录AppTable到slot和groupinfo关系
		string _sAppTable;
        MapStr2MigrateConf _mapAppTable2MigrateConf;                        // 记录AppTable到迁移配置
		MapStr2VGroupInfo _mapAppTable2DelGroupInfo;						// 记录要删除的实例
        MigrateConf _oEmptyMigrateConf;                     
		TC_ThreadRecMutex _olock;
		bool bIsFirstInitFinish;
};


# endif 
