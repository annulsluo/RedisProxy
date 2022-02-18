/*************************************************************************
    > File Name: HandleSlotMigrateTask.h
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 二  6/30 16:58:50 2020
 ************************************************************************/
#ifndef _HANDLESLOTMIGRATETASK_H_
#define _HANDLESLOTMIGRATETASK_H_

#include "util/tc_common.h"
#include "util/ProcessThreadPool.h"
#include "boost/crc.hpp"
#include "ServerDefine.h"
#include "RedisProxySlot.h"
#include "util/common.h"
#include "RedisProxyBackEnd.h"
#include "RedisProxyZkData.h"
#include "RedisProxySlot.h"
#include "RedisProxyPrometheus.h"
#include "RedisProxyManager.h"
#include "RedisProxySlotAction.h"

using namespace std;
using namespace AICommon;
using namespace RedisProxy;
using namespace RedisProxyCommon;

struct HandleSlotMigrateTask : public ProcessThreadPoolTask{
			HandleSlotMigrateTask( const std::string & sAppTable, SlotAction & oSlotAction ):
			_oSlotAction(oSlotAction),_sAppTable(sAppTable)
		{
			_oMigrateConf   = RedisProxyZkData::getInstance()->GetMigrateConf( _sAppTable );
            _nCursor        = oSlotAction.ncursor();
            _nGroupId       = oSlotAction.nfromgid();
            _sPattern       = _oMigrateConf.sScanPattern.empty()?"*":_oMigrateConf.sScanPattern;
            _nFailMigrateKey    = 0;
            _nFinishMigrateKey  = 0;
		}

		int BatchMigrateKey( vector< string > & vecPlanMigrateKey, string & sAppTable, SlotAction & oSlotAction )		
		{
            // 需要使用版本3的方式
            PipeLineRequest		oPipeLineReq;
            PipeLineResponse	oPipeLineRsp;
            oPipeLineReq.set_smsgid( _sMsgid );
			GroupInfo oGroupInfo 
					= RedisProxyZkData::getInstance()->GetGroupInfoByAppTableGid( sAppTable, oSlotAction.ntogid() );
            RedisConf oRedisConf
                    = RedisProxyZkData::getInstance()->GetRedisConfByAppTable( sAppTable );
            int nTry = 0, nRet = AICommon::RET_SUCC;
			while( nTry < MAX_TRY_TIMES )
			{
                RedisProxyManager::GenMigratePipeLineReq( 
                        vecPlanMigrateKey, oGroupInfo, _oMigrateConf, oRedisConf, oPipeLineReq );
                MYBOOST_LOG_DEBUG( OS_KV( "pipelinereq", oPipeLineReq.DebugString() ) );
				RedisProxyBackEnd oRedisProxyBackEnd( _sMsgid, _nGroupId, _sAppTable, oPipeLineReq );
				nRet = oRedisProxyBackEnd.DoPipeLine( oPipeLineRsp );
				if( nRet == AICommon::RET_SUCC )
				{
					MYBOOST_LOG_INFO( OS_KV( "msgid", _sMsgid ) 
							<< OS_KV( "Dopipeline Succ. ret", nRet ) 
							<< OS_KV( "PipeLineRsp", oPipeLineRsp.DebugString() ) ); 
					for( size_t i = 0; i < size_t( oPipeLineRsp.opipelineresultlist_size() ); ++ i )
					{
						const PipeLineResult & oPipeLineResult	= oPipeLineRsp.opipelineresultlist(i);
                        if( oPipeLineResult.oresultelementlist_size() == 0 )
                        {
                            if( oPipeLineResult.nret() == 0 )
                            {
                                RedisProxyPrometheus::getInstance()->ReportPlus( "HandleSlotMigrateTask", 
                                        "MigrateResultType:Succ", 1 );
                            }
                            else
                            {
                                MYBOOST_LOG_INFO( "migrate Fail." << OS_KV( "msgid", _sMsgid ) 
                                        << OS_KV( "ret", oPipeLineResult.nret() ) );
                                RedisProxyPrometheus::getInstance()->ReportPlus( "HandleSlotMigrateTask", 
                                        "MigrateResultType:fail", 1 );
                            }
                        }
                        else
                        {
                            for( size_t j = 0; j < size_t( oPipeLineResult.oresultelementlist_size() ); ++ j )
                            {
                                const RedisProxy::ResultElement & oPipeResultElement 
                                    = oPipeLineResult.oresultelementlist(j);
                                MYBOOST_LOG_DEBUG( OS_KV( "migrate resultelemnt", oPipeResultElement.DebugString() ) );
                                // 获取OK的字段需要再处理下
                                if( oPipeResultElement.sresultelement() == "OK" )
                                {
                                    RedisProxyPrometheus::getInstance()->ReportPlus( "HandleSlotMigrateTask", 
                                            "MigrateResultType:Succ", 1 );
                                }
                                else if( oPipeResultElement.sresultelement() == "NOKEY" )
                                {
                                    RedisProxyPrometheus::getInstance()->ReportPlus( "HandleSlotMigrateTask", 
                                            "MigrateResultType:NOKEY", 1 );
                                }
                                else
                                {
                                    MYBOOST_LOG_ERROR( OS_KV( "msgid", _sMsgid )  << OS_KV( "try", nTry )
                                            << OS_KV( "Migrate Fail. result", oPipeResultElement.sresultelement() ) 
                                            << OS_KV( "PipeLineResult", oPipeResultElement.DebugString() ) ); 
                                }
                            }
                        }
                    }
                    return AICommon::RET_SUCC;
                }
                else
                {
                    MYBOOST_LOG_ERROR( OS_KV( "msgid", _sMsgid )  << OS_KV( "try", nTry )
							<< OS_KV( "Migrate Fail. ret", nRet ) 
							<< OS_KV( "PipeLineRsp", oPipeLineRsp.DebugString() ) ); 
				}
				nTry++;
			}
            RedisProxyPrometheus::getInstance()->ReportPlus( "HandleSlotMigrateTask", 
                    "MigrateResultType:Fail", vecPlanMigrateKey.size() );
			return nRet;
		}

		virtual int HandleProcess()
		{
			// 1. 分页扫描需要执行迁移的源实例中的key，扫描的时候往哪个节点进行扫描？是通过Proxy访问还是直接去访问？
			// 答：不通过proxy，在部署的Guard节点进行扫描
			// 2. 实例中存在多种类型的key 能否都能扫描到？
			// 2. 根据CRC算法计算Key是否命中要迁移的slot，若命中，则需要进行迁移；否则，保留在源实例中
			// 3. 利用原生Migrate命令分批对key进行迁移，迁移成功则把游标记录到zk中
            
            _sMsgid	= ::GenerateUUID(); 
            MYBOOST_LOG_DEBUG( "HandleProcess Begin." 
                    << OS_KV( "apptable", _sAppTable )
                    << OS_KV( "maxscantimes", _oMigrateConf.nMaxScanTimes )
                    << OS_KV( "msgid", _sMsgid )
                    << OS_KV( "slotaction", _oSlotAction.DebugString() ) );
			_oSlotAction.set_nstate( RedisProxyCommon::SLOT_STATE_MIGRATING );
			_oSlotAction.set_sctime( TC_Common::now2str( "%Y-%m-%d %H:%M:%S" ) );
			int nScanTimes		= 0;
            do
			{
				PipeLineRequest		oPipeLineReq;
				PipeLineResponse	oPipeLineRsp;
				oPipeLineReq.set_smsgid( _sMsgid );

				RedisProxyCommon::CmdBody & oCmdBody	= *(oPipeLineReq.add_ocmdbodylist());
				string sCmd	= "scan " + TC_Common::tostr( _nCursor ) + " match " + _sPattern 
								+ " count " + TC_Common::tostr( _oMigrateConf.nScanCount );
				oCmdBody.set_scmd( sCmd );
				oCmdBody.set_nver( RedisProxy::VERSION_CMD );
				MYBOOST_LOG_DEBUG( OS_KV( "apptable", _sAppTable )
						<< OS_KV( "groupid", _nGroupId )
                        << OS_KV( "cursor", _nCursor )
                        << OS_KV( "scantimes", nScanTimes )
						<< OS_KV( "pipelinereq", oPipeLineReq.DebugString() ) );

				RedisProxyBackEnd oRedisProxyBackEnd( _sMsgid, _nGroupId, _sAppTable, oPipeLineReq );
				int nRet = oRedisProxyBackEnd.DoPipeLine( oPipeLineRsp );
				if( nRet == AICommon::RET_SUCC )
				{
					MYBOOST_LOG_DEBUG( OS_KV( "msgid", _sMsgid ) 
							<< OS_KV( "Dopipeline Succ. ret", nRet ) 
							<< OS_KV( "PipeLineRsp", oPipeLineRsp.DebugString() ) ); 
					for( size_t i = 0; i < size_t( oPipeLineRsp.opipelineresultlist_size() ); ++ i )
					{
						const PipeLineResult & oPipeLineResult	= oPipeLineRsp.opipelineresultlist(i);
						for( size_t j = 0; j < size_t( oPipeLineResult.oresultelementlist_size() ); 
								j += _oMigrateConf.nBatchMigrateNum )
						{
							int nEnd;
							if( j + _oMigrateConf.nBatchMigrateNum >= size_t(oPipeLineResult.oresultelementlist_size() ) )
							{
								nEnd = oPipeLineResult.oresultelementlist_size();
							}
							else
							{
								nEnd = j + _oMigrateConf.nBatchMigrateNum;
							}
							for( size_t k = j; k < size_t(nEnd); ++ k )
							{
								const RedisProxy::ResultElement & oPipeResultElement = oPipeLineResult.oresultelementlist(k);
								if( k == 0 ) 
								{
									_nCursor = TC_Common::strto<long long>( oPipeResultElement.sresultelement() );
									continue;
								}
								std::string sKey	= oPipeResultElement.sresultelement();
								int nSlotId			
                                    = RedisProxySlot::getInstance()->GetSlotIdByAppTableKeyReal( _sAppTable, sKey );
								if( nSlotId == _oSlotAction.nslotid() )
									_vecPlanMigrateKey.push_back( oPipeResultElement.sresultelement() );
                                /*
                                MYBOOST_LOG_DEBUG( OS_KV( "msgid", _sMsgid ) 
                                            << OS_KV( "index", k )
                                            << OS_KV( "size", _vecPlanMigrateKey.size() ) );
                                            */
							}
                            
                            RedisProxyPrometheus::getInstance()->ReportPlus( "HandleSlotMigrateTask", 
                                    {{"type","PlanMigrate"},{"apptable",_sAppTable},
                                    {"slotid",TC_Common::tostr(_oSlotAction.nslotid())},
                                    {"fromgid",TC_Common::tostr(_oSlotAction.nfromgid())},
                                    {"togid",TC_Common::tostr(_oSlotAction.ntogid())}},
                                    _vecPlanMigrateKey.size() );
                            if( _vecPlanMigrateKey.size() > 0 )
                            {
                                int nRet = BatchMigrateKey( _vecPlanMigrateKey, _sAppTable, _oSlotAction );
                                if( nRet == AICommon::RET_SUCC )
                                {
                                    _oSlotAction.set_sctime( TC_Common::now2str( "%Y-%m-%d %H:%M:%S" ) );
                                    _oSlotAction.set_ncursor( _nCursor );
                                    RedisProxySlotAction::getInstance()->UpdateSlotActions( _sAppTable, _oSlotAction );
                                    _nFinishMigrateKey += _vecPlanMigrateKey.size();
                                    RedisProxyPrometheus::getInstance()->ReportPlus( "HandleSlotMigrateTask", 
                                            {{"type","FinishMigrate"},{"apptable",_sAppTable},
                                            {"slotid",TC_Common::tostr(_oSlotAction.nslotid())},
                                            {"fromgid",TC_Common::tostr(_oSlotAction.nfromgid())},
                                            {"togid",TC_Common::tostr(_oSlotAction.ntogid())}},
                                            _vecPlanMigrateKey.size() );
                                    MYBOOST_LOG_DEBUG( OS_KV( "msgid", _sMsgid ) 
                                            << OS_KV( "BatchMigrate Result Succ. ret", nRet ) 
                                            << OS_KV( "size", _vecPlanMigrateKey.size() )
                                            << OS_KV( "cursor", _nCursor ) 
                                            << OS_KV( "finishmigrate", _nFinishMigrateKey )
                                            << OS_KV( "slotaction", _oSlotAction.DebugString() ) ); 
                                }
                                else
                                {
                                    RedisProxyPrometheus::getInstance()->ReportPlus( "HandleSlotMigrateTask", 
                                            {{"type","FailMigrate"},{"apptable",_sAppTable},
                                            {"slotid",TC_Common::tostr(_oSlotAction.nslotid())},
                                            {"fromgid",TC_Common::tostr(_oSlotAction.nfromgid())},
                                            {"togid",TC_Common::tostr(_oSlotAction.ntogid())}},
                                            _vecPlanMigrateKey.size() );
                                    _nFailMigrateKey += _vecPlanMigrateKey.size();
                                    MYBOOST_LOG_ERROR( OS_KV( "msgid", _sMsgid ) 
                                            << OS_KV( "BatchMigrate Result Error. ret", nRet ) 
                                            << OS_KV( "size", _vecPlanMigrateKey.size() )
                                            << OS_KV( "cursor", _nCursor ) 
                                            << OS_KV( "failmigrate", _nFailMigrateKey ) );
                                }
                                _vecPlanMigrateKey.clear();
                            }
                        }
                    }
                }
                else
                {
                    MYBOOST_LOG_ERROR(  OS_KV("msgid", _sMsgid ) 
                            << OS_KV( "Dopipeline Error. ret", nRet ) ); 
                }
                nScanTimes ++;
                if( nScanTimes > _oMigrateConf.nMaxScanTimes ) break;
            }while( _nCursor != SCAN_FINISH );

            if( _nCursor == SCAN_FINISH && nScanTimes > 0 )
            {
                _oSlotAction.set_nstate( RedisProxyCommon::SLOT_STATE_FINISH );
                _oSlotAction.set_sctime( TC_Common::now2str( "%Y-%m-%d %H:%M:%S" ) );
                _oSlotAction.set_ncursor( _nCursor );
                RedisProxySlotAction::getInstance()->UpdateSlotActions( _sAppTable, _oSlotAction );
                MYBOOST_LOG_DEBUG( "DoHandleSlotMigrate Finish." 
                        << OS_KV( "msgid", _sMsgid ) 
                        << OS_KV( "apptable", _sAppTable )
                        << OS_KV( "groupid", _nGroupId )
                        << OS_KV( "cursor", _nCursor )
                        << OS_KV( "scantimes", nScanTimes )
                        << OS_KV( "FailMigrate", _nFailMigrateKey )
                        << OS_KV( "FinsisMigrate", _nFinishMigrateKey )
                        << OS_KV( "slotaction", _oSlotAction.DebugString() ) );
            }

            return AICommon::RET_SUCC;
        }

        virtual void HandleQueueFull() { MYBOOST_LOG_ERROR("HandleQueueFull"); }
        virtual void HandleTimeout() { MYBOOST_LOG_ERROR("HandleTimeout"); }
        virtual void HandleErrorMuch() { MYBOOST_LOG_ERROR("HandleErrorMuch"); }
        virtual void HandleExp() { MYBOOST_LOG_ERROR("HandleExp"); }
        virtual void DestroySelf() { MYBOOST_LOG_DEBUG( "Task Delete Succ" ); delete this; }

	private:
		SlotAction _oSlotAction;
		long long _nCursor;				        // 扫描时候的游标
		int _nGroupId;					 
		std::string _sAppTable;			 
		vector< string > _vecPlanMigrateKey;	// 要迁移的Key
		int _nFailMigrateKey;			        // 失败迁移Key数
		int _nFinishMigrateKey;			        // 完成迁移Key数
		string _sPattern;
		MigrateConf _oMigrateConf;
        std::string _sMsgid;            
};

#endif // _HANDLESLOTMIGRATETASK_H_

