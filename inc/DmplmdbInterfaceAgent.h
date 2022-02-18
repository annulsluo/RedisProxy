/*************************************************************************
  > File Name: DmplmdbInterfaceAgent.h
  > Author: annulsluo
  > Mail: annulsluo@gmail.com
  > Created Time: 日  3/ 8 16:19:49 2020
 ************************************************************************/

#ifndef _DmplmdbInterfaceAgent_H_
#define _DmplmdbInterfaceAgent_H_
#include "ServerConfig.h"
#include "util/myboost_log.h"
#include "util/tc_singleton.h"
#include "AICommon.pb.h"
#include "DmpService.pb.h"
#include "DmpService.grpc.pb.h"
#include <grpcpp/grpcpp.h>


using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using namespace AICommon;
using namespace dmp;

class DmplmdbInterfaceAgent : public taf::TC_Singleton< DmplmdbInterfaceAgent >
{
	public:
		void Init( )
		{
			try{
				string sDmpLmdbHost = ServerConfigMng::getInstance()->GetSObj( "dmplmdbhostlist" );
				_nMax_try_times		= ServerConfigMng::getInstance()->GetIObj( "dmplmdbmaxtrytimes" );
				_nTimeout_sec		= ServerConfigMng::getInstance()->GetIObj( "dmplmdbtimeoutsec" );
				_nTimeout_nsec		= ServerConfigMng::getInstance()->GetIObj( "dmplmdbtimeoutnsec" );
				vector< string > vecDmpLmdbHostList = TC_Common::sepstr< string >( sDmpLmdbHost, ";" );
				_nHostSize = vecDmpLmdbHostList.size();
				for( size_t i = 0; i < size_t(_nHostSize); ++ i )
				{
					vector< string > vecHostInfo = TC_Common::sepstr< string >( vecDmpLmdbHostList[i], ":" );
					std::shared_ptr<Channel> oChannel(
							grpc::CreateChannel(string(vecHostInfo[0])+":"+string(vecHostInfo[1]),grpc::InsecureChannelCredentials()));
					_vecDmpKvStub.push_back( DmpKVServer::NewStub( oChannel ) ); 
				}

				MYBOOST_LOG_INFO( "Init DmpLMDBInterface Succ." );
			}
			catch( exception & e )
			{
				MYBOOST_LOG_ERROR( "createchannel exception:" << e.what() );
			}
		}

		std::unique_ptr< DmpKVServer::Stub > & GetDmpKvStub( )
		{
			srand((unsigned)time(NULL));
			return _vecDmpKvStub[ ( rand() % _nHostSize ) ];
		}

		int syncPiplineOper( KVOperator & oReq, KVResponse & oRsp )
		{
			for( int i = 0; i < _nMax_try_times; ++ i )
			{
				try{
					ClientContext oContext;
					gpr_timespec timespec;
					timespec.tv_sec		= _nTimeout_sec;
					timespec.tv_nsec	= _nTimeout_nsec;
					timespec.clock_type	= GPR_TIMESPAN;
					oContext.set_deadline( timespec );
					
					Status oStatus	= GetDmpKvStub()->PipleLineOper( &oContext, oReq, &oRsp );

					if ( oStatus.ok()) {
						return AICommon::RET_SUCC;
					} else {
						MYBOOST_LOG_ERROR( "PipelineOper Fail." << OS_KV( "code", oStatus.error_code() )
								<< OS_KV( "msg", oStatus.error_message().c_str() ) );
					}
				}
				catch( exception & e )
				{
					MYBOOST_LOG_ERROR( "PipelineOper Excep." << OS_KV( "msg", e.what() ) );
				}
			}
			return AICommon::RET_FAIL;
		}
	private:
		vector< std::unique_ptr< DmpKVServer::Stub > > _vecDmpKvStub;
		int _nHostSize;
		int _nMax_try_times;
		int _nTimeout_sec;
		int _nTimeout_nsec;
};
#endif
