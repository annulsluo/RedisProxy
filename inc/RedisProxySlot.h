/*************************************************************************
    > File Name: RedisProxySlot.h
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 一 11/11 07:57:33 2019
 ************************************************************************/
#ifndef _RedisProxySlot_H_
#define _RedisProxySlot_H_
#include "ServerDefine.h"
#include "RedisProxyCommon.pb.h"
#include "redisproxy.pb.h"
#include "util/tc_singleton.h"
#include "util/tc_config.h"

using namespace RedisProxyCommon;
using namespace RedisProxy;
using namespace std;
using namespace taf;

// 实现单例化
// slot的范围是从1开始
// 一期：通过静态配置的方式初始化tablename、slot和group映射关系
// 二期：从zk获取slot和group的映射关系 
class RedisProxySlot:
	public taf::TC_Singleton<RedisProxySlot>
{
	/*
	public:
		RedisProxySlot();
		virtual ~RedisProxySlot();
		*/
	public:
		virtual int Init();
		void InitSlot2GroupInfoByZkData(
				const std::string & sAppTable,
				const std::string & sGroup2Slot );

        void InitRebalanceSlot2GroupInfoByZkData( 
                const std::string & sAppTable,
                const std::string & sGroup2Slot );

	public:
		int GetSlotIdByKey( const string & sKey );
		int GetSlotIdByAppTableKey( 
				const string & sAppName,
				const string & sTableName,
				const string & sKey );

		int GetSlotIdByAppTableKeyReal( 
				const string & sAppTable,
				const string & sKey );

        int GetGroupInfoBySlotGroupInfo( 
                const std::string & sAppTable,
                const std::string & sKey,
                MapStr2SlotGroupInfo & mapAppTable2SlotGroupInfo,
                RedisProxyCommon::GroupInfo & oGroupInfo );

		int GetGroupIdBySlot( int nSlot );

		int GetGroupIdByProxyReq( 
				const RedisProxyInnerRequest & oRedisProxyInnerReq,
				MapInt2VStr & mapGroupId2VSid );

		int GetGroupIdByAppTableSlot( 
				const string & sAppName,
				const string & sTableName, 
				const int nSlotId );

		void GetGroupInfoByAppTableSlot( 
				const string & sAppName,
				const string & sTableName, 
				const int nSlotId, 
				GroupInfo & oGroupInfo );

		int GetGroupInfoByAppTableKey( 
				const string & sAppName,
				const string & sTableName,
				const string & sKey,
				GroupInfo & oGroupInfo );

		int GetGroupIdByKey( const string & sKey );
		int GetGroupInfoByKey( const string & sKey, GroupInfo & oGroupInfo );

		void GetGroupInfo( const int nGroupId, GroupInfo & oGroupInfo );

		void GetGroupInfo( 
				const string & sAppName, 
				const string & sTableName, 
				const string & sKey, 
				GroupInfo & oGroupInfo );

		void GenSid2GroupIdByReq( 
				const RedisProxyInnerRequest & oRedisProxyInnerReq,
				MapStr2Int & mapSid2GroupId,
				vector< int > & vecGroupId );

		void GenSid2GroupInfoByReq( 
				const RedisProxyInnerRequest & oRedisProxyInnerReq,
				MapStr2Int & mapSid2GroupId,
				MapInt2GroupInfo & mapGroupId2GroupInfo );

		void GetAppTableInfo( const string & sAppTable, AppTableInfo & oAppTableInfo );
	private:
		void InitAppTable2Slot( 
				MapStr2MStr::iterator m2mstrit );

		void InitGroupInfo( 
				MapStr2MStr::iterator m2mstrit,
				const int nGroupId,
				RedisProxyCommon::GroupInfo & oGroupInfo );

		void InitSlot2GroupInfo();
		
		void InitAppTable2VGroupInfo();

        void InsertAppTableInfo( const std::string & sAppTable );

		void InsertAppTable2SlotGroupInfo( 
				const std::string & sAppTable,
				const std::string & sGroup2Slot );

		void InsertAppTable2RebalanceSlotGroupInfo( 
				const std::string & sAppTable,
				const std::string & sRebalanceGroup2Slot );
			
	private:
        // 内存中AppTable格式保持一致为 App.Table，即 aiad.dspall
		MapStr2AppTableInfo		_mapAppTableInfo;				// 记录AppTable信息(redispasswd\redislotcnt等)
		MapStr2SlotGroupInfo	_mapAppTable2SlotGroupInfo;		// 记录AppTable到slot和groupinfo关系
        MapStr2SlotGroupInfo    _mapAppTable2RebalanceSGI;      // 记录AppTable到rebalance的slot和groupinfo关系
		TC_ThreadRecMutex		_olock;
};
#endif

