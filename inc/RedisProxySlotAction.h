/*************************************************************************
    > File Name: RedisProxySlotAction.h
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 日  6/28 19:09:44 2020
 ************************************************************************/

#ifndef _RedisProxySlotAction_H_
#define _RedisProxySlotAction_H_
#include "ServerDefine.h"
#include "RedisProxyCommon.pb.h"
#include "redisproxy.pb.h"
#include "util/tc_singleton.h"
#include "util/tc_config.h"
#include "util/myboost_log.h"
#include "util/tc_thread.h"

// 管理迁移计划表, 执行迁移计划
// 以队列和线程任务形式对迁移计划进行执行
// 需要控制线程同时处理多少个任务

class RedisProxySlotAction:
	public taf::TC_Singleton< RedisProxySlotAction >,
    public taf::TC_Thread
{
    public:
        RedisProxySlotAction();
        ~RedisProxySlotAction();

        bool InitSlotActions( 
                const std::string & sAppTable,
                MapInt2SlotAction & mapSlotId2SA,
                bool IsMigrateRun = false );

        void UpdateSlotAction( 
                MapInt2SlotAction & mapSlotAction,
                MapInt2SlotAction::iterator mit );

        void UpdateSlotAction( 
                const std::string & sAppTable,
                const SlotAction & oSlotAction,
                MapStr2ISlotAction & mapAppTable2ISA );

        void UpdateSlotActions( 
                const std::string & sAppTable,
                const SlotAction & oSlotAction );

        int UpdateSlotActions(
                const std::string & sAppTable,
                MapInt2SlotAction & mapSlotId2SA );

        bool IsGroupMigrate( 
                const std::string & sAppTable,
                const int & nGroupId );

        bool IsAppTableRebalance( 
                const std::string & sAppTable );

        void SetGroupMigrateState( 
                const std::string & sAppTableGid, 
                const int & nGroupMigrateState );

        void SetAppTableRebalanceState( 
                const std::string & sAppTable, 
                const int & nAppTableRebalanceState );

        int GetGroupMigrateState( 
                const std::string & sAppTable,
                const int & nGroupId );
                
        void EraseSlotAction(
                const std::string & sAppTable,
                const int & nSlotId, 
                MapStr2ISlotAction & mapAppTable2SlotAction );

    private:
        bool IsSlotMigrate( int nSlotState );
        void run();

    private:
        MapStr2ISlotAction  _mapAppTable2PlanSlotAction;			    // 迁移计划表
        MapStr2ISlotAction  _mapAppTable2MigrateingSlotAction;          // 迁移中表
        MapStr2ISlotAction  _mapAppTable2FinishSlotAction;				// 迁移完成表
        MapStr2Int          _mapAppTableGid2FinalState;                 // 实例对应的迁移状态
        MapStr2Int          _mapAppTable2RebalanceState;                // 表名的均衡状态判断
        bool                _bIsMigrateRunning;                         // 迁移线程是否启动
		TC_ThreadRecMutex   _olock;
};

#endif
