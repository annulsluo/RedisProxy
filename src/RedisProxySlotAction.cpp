/*************************************************************************
    > File Name: RedisProxySlotAction.cpp
    > Author: annulsluo
    > Mail: annulsluo@gmail.com
    > Created Time: 二  6/30 16:49:54 2020
 ************************************************************************/
#include "RedisProxySlotAction.h"
#include "RedisProxyCommon.pb.h"
#include "util/tc_common.h"
#include "ServerDefine.h"
#include "AICommon.pb.h"
#include "RedisProxyPrometheus.h"
#include "HandleSlotMigrateTask.h"
#include "RedisProxyThreadPool.h"
#include "ServerConfig.h"
#include "ServerDefine.h"
#include "RedisProxyZkData.h"

RedisProxySlotAction::RedisProxySlotAction()
{
    _bIsMigrateRunning = false;
}

RedisProxySlotAction::~RedisProxySlotAction()
{

}

bool RedisProxySlotAction::IsSlotMigrate( int nSlotState )
{
    if( nSlotState == RedisProxyCommon::SLOT_STATE_MIGRATING
        || nSlotState == RedisProxyCommon::SLOT_STATE_IMPORTING ) 
    {
        return true;
    }
    return false;
}

bool RedisProxySlotAction::InitSlotActions( 
        const std::string & sAppTable,
        MapInt2SlotAction & mapSlotId2SA,
        bool bIsMigrateRun )
{
    if( UpdateSlotActions( sAppTable, mapSlotId2SA ) == AICommon::RET_SUCC )
    {
        MYBOOST_LOG_INFO( "InitSlotActions Succ."
                << OS_KV( "apptable", sAppTable ) << OS_KV( "size", mapSlotId2SA.size() ) );
        if( bIsMigrateRun && !_bIsMigrateRunning )
        {
            this->start();
            _bIsMigrateRunning = true;
        }
        return true;
    }
    else
    {
        MYBOOST_LOG_ERROR( "InitSlotActions Fail." 
               <<  OS_KV( "apptable", sAppTable ) << OS_KV( "size", mapSlotId2SA.size() ) );
    }
    return false;
}

void RedisProxySlotAction::UpdateSlotAction( 
        MapInt2SlotAction & mapSlotAction,
        MapInt2SlotAction::iterator mit )
{
    MapInt2SlotAction::iterator mi2sa = mapSlotAction.find( mit->first );
    if( mi2sa == mapSlotAction.end() )
        mapSlotAction.insert( std::pair< int, RedisProxyCommon::SlotAction >( mit->first, mit->second ) );
    else
        mi2sa->second = mit->second;
}

void RedisProxySlotAction::UpdateSlotAction( 
        const std::string & sAppTable,
        const SlotAction & oSlotAction,
        MapStr2ISlotAction & mapAppTable2ISA )
{
    auto mat2isa = mapAppTable2ISA.find( sAppTable );
    if( mat2isa != mapAppTable2ISA.end() )
    {
        auto mi2sa = mat2isa->second.find( oSlotAction.nslotid() );
        if( mi2sa == mat2isa->second.end() )
        {
            mat2isa->second.insert( std::pair< int, RedisProxyCommon::SlotAction >( oSlotAction.nslotid(), oSlotAction ) );
            /*
            MYBOOST_LOG_DEBUG( "UpdateSlotAction." 
                    << OS_KV( "apptable", sAppTable ) );
                    */
            //        << OS_KV( "slotaction", oSlotAction.DebugString() ) );
        }
        else
        {
            mi2sa->second = oSlotAction;
        }
    }
    else
    {
        MYBOOST_LOG_ERROR( "Not Find AppTable."
            << OS_KV( "apptable", sAppTable ) );
            //<< OS_KV( "slotaction", oSlotAction.DebugString()) );
    }
}

void RedisProxySlotAction::UpdateSlotActions( 
        const std::string & sAppTable,
        const SlotAction & oSlotAction )
{
    {
        taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( _olock );
        if( oSlotAction.nstate() == RedisProxyCommon::SLOT_STATE_PENDING )
        {
            UpdateSlotAction( sAppTable, oSlotAction, _mapAppTable2PlanSlotAction );
        }
        else if( oSlotAction.nstate() == RedisProxyCommon::SLOT_STATE_PREPARING
                || oSlotAction.nstate() == RedisProxyCommon::SLOT_STATE_MIGRATING 
                || oSlotAction.nstate() == RedisProxyCommon::SLOT_STATE_IMPORTING )
        {
            EraseSlotAction( sAppTable, oSlotAction.nslotid(), _mapAppTable2PlanSlotAction );
            UpdateSlotAction( sAppTable, oSlotAction, _mapAppTable2MigrateingSlotAction );
        }
        else if( oSlotAction.nstate() == RedisProxyCommon::SLOT_STATE_FINISH )
        {
            EraseSlotAction( sAppTable, oSlotAction.nslotid(), _mapAppTable2PlanSlotAction );
            EraseSlotAction( sAppTable, oSlotAction.nslotid(), _mapAppTable2MigrateingSlotAction );
            UpdateSlotAction( sAppTable, oSlotAction, _mapAppTable2FinishSlotAction );
        }
        MYBOOST_LOG_DEBUG( OS_KV( "plan.size", _mapAppTable2PlanSlotAction[sAppTable].size() )
               << OS_KV( "migrating.size", _mapAppTable2MigrateingSlotAction[sAppTable].size() )
               << OS_KV( "finish.size", _mapAppTable2FinishSlotAction[sAppTable].size() ) );
    }
    
    MYBOOST_LOG_DEBUG( "UpdateSlotActionZk Begin." );
    bool bUpdateSlot  = RedisProxyZkData::getInstance()->UpdateSlotAction( sAppTable, oSlotAction );
    MYBOOST_LOG_DEBUG( OS_KV( "UpdateSlotActionZk ret", bUpdateSlot ) );
    if( _mapAppTable2PlanSlotAction[ sAppTable ].size() == 0 
        && _mapAppTable2MigrateingSlotAction[ sAppTable ].size() == 0 )
    {
        RedisProxyZkData::getInstance()->DelAppTableGroupData( sAppTable );
        RedisProxyZkData::getInstance()->SetRebalanceSC2SlotConf( sAppTable );
        // TODO 清理rebalance的配置
    }
}

int RedisProxySlotAction::UpdateSlotActions(
        const std::string & sAppTable,
        MapInt2SlotAction & mapSlotId2SA )
{
    // 只有一个slot在迁移中都认为不能使用该节点
    // group的迁移状态通过slotaction配置感知到其他RedisProxyServer中
    // 1. 如果希望迁移过程中进行停止呢？
    // 2. 当完成迁移的时候，Guard程序去写内存和Zk配置, Proxy通过watch来改变
    // 把plansa\finishsa\migratesa分离出去，由Guard主动去写内存，而非zk感知方式
    MapInt2SlotAction mapPlanSa, mapFinishSa, mapMigrateSa;
    map< std::string, int >mapAppGroupId2FirstState;
    MYBOOST_LOG_INFO( OS_KV( "apptable", sAppTable ) << OS_KV( "size", mapSlotId2SA.size() ) );
    for( MapInt2SlotAction::iterator mit = mapSlotId2SA.begin(); mit != mapSlotId2SA.end(); ++ mit )
    {
        int nGroupMigrateState;
        if( IsSlotMigrate( mit->second.nstate() ) )
        {
            nGroupMigrateState = RedisProxyCommon::GROUP_MIGRATE_STATE_MIGRATING;
        }
        else
        {
            if( mit->second.nstate() == RedisProxyCommon::SLOT_STATE_FINISH )
                nGroupMigrateState = RedisProxyCommon::GROUP_MIGRATE_STATE_FINISH;
            else
                nGroupMigrateState = RedisProxyCommon::GROUP_MIGRATE_STATE_NORMAL;
        }
        // 先收集存在迁移的slot，然后再设置实例group的状态
        // 设置的策略是什么呢？
        // 1. 一个实例下的只要有一个slot(槽)处于迁移状态，则认为该实例为迁移状态
        // 2. 只有该实例下所有的slot都处于finish/pending/normal状态，才认为该实例为非迁移状态
        if( mit->second.nfromgid() >= MIN_GROUPID )
        {
            std::string sAppTableGid = sAppTable + "." + TC_Common::tostr( mit->second.nfromgid() );
            if( mapAppGroupId2FirstState.find( sAppTableGid ) == mapAppGroupId2FirstState.end() )
            {
                mapAppGroupId2FirstState[sAppTableGid] = RedisProxyCommon::GROUP_MIGRATE_STATE_NORMAL;
                mapAppGroupId2FirstState[sAppTableGid] |= nGroupMigrateState;
            }
            else 
                mapAppGroupId2FirstState[sAppTableGid] |= nGroupMigrateState;
        }
        if( mit->second.ntogid() >= MIN_GROUPID )
        {
            std::string sAppTableGid = sAppTable + "." + TC_Common::tostr( mit->second.ntogid() );
            if( mapAppGroupId2FirstState.find( sAppTableGid ) == mapAppGroupId2FirstState.end() )
            {
                mapAppGroupId2FirstState[sAppTableGid] = RedisProxyCommon::GROUP_MIGRATE_STATE_NORMAL;
                mapAppGroupId2FirstState[sAppTableGid] |= nGroupMigrateState;
            }
            else 
                mapAppGroupId2FirstState[sAppTableGid] |= nGroupMigrateState;
        }
        MYBOOST_LOG_INFO( OS_KV( "apptable", sAppTable ) 
                << OS_KV( "groupmigratestate", nGroupMigrateState )
                << OS_KV( "slotaction", mit->second.DebugString() ) );
        if( mit->second.nstate() == RedisProxyCommon::SLOT_STATE_PENDING )
        {
            UpdateSlotAction( mapPlanSa, mit );
        }
        else if( mit->second.nstate() == RedisProxyCommon::SLOT_STATE_PREPARING
                    || mit->second.nstate() == RedisProxyCommon::SLOT_STATE_MIGRATING 
                    || mit->second.nstate() == RedisProxyCommon::SLOT_STATE_IMPORTING )
        {
            EraseSlotAction( sAppTable, mit->first, _mapAppTable2PlanSlotAction );
            UpdateSlotAction( mapMigrateSa, mit );
        }
        else if( mit->second.nstate() == RedisProxyCommon::SLOT_STATE_FINISH )
        {
            EraseSlotAction( sAppTable, mit->first, _mapAppTable2PlanSlotAction );
            EraseSlotAction( sAppTable, mit->first, _mapAppTable2MigrateingSlotAction );
            UpdateSlotAction( mapFinishSa, mit );
        }
    }
    _mapAppTable2PlanSlotAction[sAppTable]          = mapPlanSa;
    _mapAppTable2MigrateingSlotAction[sAppTable]    = mapMigrateSa;
    _mapAppTable2FinishSlotAction[sAppTable]        = mapFinishSa;
    // 设置集群状态
    int nAppTableRebalanceState     = RedisProxyCommon::APPTABLE_REBALANCE_STATE_NORMAL;
    for( auto mst = mapAppGroupId2FirstState.begin(); mst != mapAppGroupId2FirstState.end(); mst ++ )
    {
        if( mst->second & RedisProxyCommon::GROUP_MIGRATE_STATE_MIGRATING )
        {
            nAppTableRebalanceState     |= RedisProxyCommon::APPTABLE_REBALANCE_STATE_REBALANCING;
            SetGroupMigrateState( mst->first, RedisProxyCommon::GROUP_MIGRATE_STATE_MIGRATING );
        }
        else
        {
            SetGroupMigrateState( mst->first, mst->second );
        }
    }
    SetAppTableRebalanceState( sAppTable, nAppTableRebalanceState );
    
    return AICommon::RET_SUCC;
}

void RedisProxySlotAction::EraseSlotAction(
        const std::string & sAppTable,
        const int & nSlotId,
        MapStr2ISlotAction & mapAppTable2SlotAction )
{
    //taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( _olock );
    auto ms2isa = mapAppTable2SlotAction.find( sAppTable );
    if( ms2isa != mapAppTable2SlotAction.end() )
    {
        MapInt2SlotAction::iterator mi2sa = ms2isa->second.find( nSlotId );
        if( mi2sa != ms2isa->second.end() )
        {
            ms2isa->second.erase( mi2sa );
        }
    }
}

void RedisProxySlotAction::SetGroupMigrateState( 
        const std::string & sAppTableGid,
        const int & nGroupMigrateState )
{
    MYBOOST_LOG_INFO( OS_KV( "apptablegid", sAppTableGid ) 
                << OS_KV( "groupmigratestate", nGroupMigrateState ) );

    MapStr2Int::iterator ms2ms = _mapAppTableGid2FinalState.find( sAppTableGid );
    if( ms2ms != _mapAppTableGid2FinalState.end() )
    {
        ms2ms->second = nGroupMigrateState;
    }
    else
    {
        _mapAppTableGid2FinalState.insert( std::pair< std::string, int >( sAppTableGid, nGroupMigrateState ) );
    }
}

bool RedisProxySlotAction::IsGroupMigrate(
        const std::string & sAppTable,
        const int & nGroupId )
{
    std::string sAppTableGid    = sAppTable + "." + TC_Common::tostr( nGroupId );
    MYBOOST_LOG_DEBUG( OS_KV( "apptablegid", sAppTableGid ) ); 

    MapStr2Int::iterator ms2ms = _mapAppTableGid2FinalState.find( sAppTableGid );
    if( ms2ms != _mapAppTableGid2FinalState.end() )
    {
        MYBOOST_LOG_DEBUG( OS_KV( "apptablegid", sAppTableGid ) << OS_KV( "state", ms2ms->second ) ); 
        if( ms2ms->second == RedisProxyCommon::GROUP_MIGRATE_STATE_MIGRATING )
        {
            MYBOOST_LOG_ERROR( "Group Migrating." 
                    << OS_KV( "apptable",sAppTable ) << OS_KV( "state",ms2ms->second) );
            return true;
        }
    }
    return false;
}

int RedisProxySlotAction::GetGroupMigrateState(
        const std::string & sAppTable,
        const int & nGroupId )
{
    std::string sAppTableGid    = sAppTable + "." + TC_Common::tostr( nGroupId );
    MYBOOST_LOG_DEBUG( OS_KV( "apptablegid", sAppTableGid ) ); 

    MapStr2Int::iterator ms2ms = _mapAppTableGid2FinalState.find( sAppTableGid );
    if( ms2ms != _mapAppTableGid2FinalState.end() )
    {
        MYBOOST_LOG_DEBUG( OS_KV( "apptablegid", sAppTableGid ) << OS_KV( "state", ms2ms->second ) ); 
        return ms2ms->second;
    }
    return RedisProxyCommon::GROUP_MIGRATE_STATE_NORMAL;
}

void RedisProxySlotAction::SetAppTableRebalanceState( 
        const std::string & sAppTable,
        const int & nAppTableRebalanceState )
{
    MYBOOST_LOG_INFO( OS_KV( "apptable", sAppTable ) 
                << OS_KV( "apptablerebalancestate", nAppTableRebalanceState ) );

    auto mit = _mapAppTable2RebalanceState.find( sAppTable );
    if( mit != _mapAppTable2RebalanceState.end() )
    {
        mit->second = nAppTableRebalanceState;
    }
    else
    {
        _mapAppTable2RebalanceState.insert( std::pair< std::string, int >( sAppTable, nAppTableRebalanceState ) );
    }
}

bool RedisProxySlotAction::IsAppTableRebalance( const std::string & sAppTable )
{
    auto mit = _mapAppTable2RebalanceState.find( sAppTable );
    if( mit != _mapAppTable2RebalanceState.end() )
    {
        if( mit->second == RedisProxyCommon::APPTABLE_REBALANCE_STATE_REBALANCING )
        {
            MYBOOST_LOG_ERROR( "AppTable Rebalanceing." 
                    << OS_KV( "apptable",sAppTable ) << OS_KV( "state",mit->second) );
            return true;
        }
    }
    return false;
}

void RedisProxySlotAction::run()
{
    // TODO 如果有在迁移的任务，然后临时加了一个新的迁移任务，这时候如何处理？不可能重启，只能使用感知的方式
    // 如果程序停止了，然后状态处于PREPARING/Migrating时，则会进行断点续迁；
    sleep( SLEEP_TIME );
	MYBOOST_LOG_INFO( "RedisProxySlotAction MigrateBegin." );
    for( auto msIsa = _mapAppTable2MigrateingSlotAction.begin(); 
            msIsa != _mapAppTable2MigrateingSlotAction.end(); ++ msIsa )
    {
        for( auto mi2sa = msIsa->second.begin(); mi2sa != msIsa->second.end(); ++ mi2sa )
        {
            MYBOOST_LOG_INFO( "Migrate Task Begin."
                    << OS_KV( "apptable", msIsa->first )
                    << OS_KV( "slotid", mi2sa->first )
                    << OS_KV( "Slotid2SlotAction.size",msIsa->second.size() ) );
            HandleSlotMigrateTask * task = new HandleSlotMigrateTask( msIsa->first, mi2sa->second );
            if( task == NULL ||
                    RedisProxyThreadPool::getInstance()->GetThreadPool().AddTask( task ) != ProcessThreadPool::ADD_TASK_RET_SUCC )
            {
                MYBOOST_LOG_ERROR( " HandleSlotMigrateTask Error! " );
            }
        }
    }
    while( 1 )
    {
        sleep( SLEEP_TIME );
        // 开始执行任务后，加上一个状态或者从该队列中删除，避免再次循环处理
        // 任务进入到线程池队列以后，也需要进行删除，避免再次进入队列
		MYBOOST_LOG_INFO( OS_KV( "AppTable2PlanSlotAction.size",_mapAppTable2PlanSlotAction.size() ) );
        for( auto msIsa = _mapAppTable2PlanSlotAction.begin(); 
                msIsa != _mapAppTable2PlanSlotAction.end(); ++ msIsa )
        {
            MigrateConf oMigrateConf =  RedisProxyZkData::getInstance()->GetMigrateConf( msIsa->first );
            for( auto mi2sa = msIsa->second.begin(); mi2sa != msIsa->second.end(); ++ mi2sa )
            {
                if( mi2sa->second.nstate() == RedisProxyCommon::SLOT_STATE_PENDING )
                {
                    MYBOOST_LOG_INFO( "Plan Task Begin."
                            << OS_KV( "apptable", msIsa->first )
                            << OS_KV( "slotid", mi2sa->first )
                            << OS_KV( "Slotid2SlotAction.size",msIsa->second.size() ) );
                    mi2sa->second.set_nstate( RedisProxyCommon::SLOT_STATE_PREPARING );
                    mi2sa->second.set_sctime( TC_Common::now2str( "%Y-%m-%d %H:%M:%S" ) );
                    RedisProxyZkData::getInstance()->UpdateSlotAction( msIsa->first, mi2sa->second );
                    HandleSlotMigrateTask * task = new HandleSlotMigrateTask( msIsa->first, mi2sa->second );
                    if( task == NULL ||
                            RedisProxyThreadPool::getInstance()->GetThreadPool().AddTask( task ) != ProcessThreadPool::ADD_TASK_RET_SUCC )
                    {
                        MYBOOST_LOG_ERROR( " HandleSlotMigrateTask Error! " );
                    }
                }
            }
        }
    }
}

