/*************************************************************************
    > File Name: RedisProxyBackEnd.cpp
    > Author: annulsluo
    > Mail: annulsluo@gmail.com
    > Created Time: 一 11/11 09:46:51 2019
 ************************************************************************/
#include "RedisProxyBackEnd.h"
#include "ServerDefine.h"
#include "ServerConfig.h"
#include "AICommon.pb.h"
#include "redisproxy.pb.h"
#include "hiredis/hiredis.h"
extern "C"{
#include "hiredis/sds.h"
}
#include "util/myboost_log.h"
#include "util/TafOsVar.h"
#include "RedisConnectPoolFactory.h"
#include "RedisProxyPrometheus.h"
#include "RedisConnectPoolFactory_Queue.h"
#include "RedisProxyFailOver.h"

using namespace AICommon;
using namespace std;
using namespace RedisProxy;
using namespace RedisProxyCommon;

RedisProxyBackEnd::RedisProxyBackEnd( 
		const string & sMsgid,
		const GroupInfo & oGroupInfo,
		const AppTableInfo & oAppTableInfo,
		const PipeLineRequest & oPipeLineReq
		):_sMsgid(sMsgid),_oGroupInfo(oGroupInfo),_oAppTableInfo(oAppTableInfo),_oPipeLineReq(oPipeLineReq)
{
	_nGroupId	= oGroupInfo.ngroupid();
	_bNeedAuth	= false;
	_pRedisConn = NULL;
	_bHealthRedisConn	= true;
	_pItem		= NULL;
	_pQueueItem			= NULL;

	// 通过GroupInfo初始化链接
	if( !RedisProxyFailOver::getInstance()->IsGroupFail( oGroupInfo.sapptable(), _nGroupId ) )
	{
		InitRedisConnect( _oGroupInfo, _oAppTableInfo ); 
	}
	else
	{

	}
}

RedisProxyBackEnd::RedisProxyBackEnd( 
		const string & sMsgid,
		const int & nGroupId,
		const string & sAppTable,
		const PipeLineRequest & oPipeLineReq
		):_sMsgid(sMsgid),_nGroupId(nGroupId),_sAppTable(sAppTable),_oPipeLineReq(oPipeLineReq)
{
	_bNeedAuth	= false;
	_pRedisConn = NULL;
	_bHealthRedisConn	= true;
	_pItem		= NULL;
	_pQueueItem			= NULL;

	// 通过GroupInfo初始化链接
	if( !RedisProxyFailOver::getInstance()->IsGroupFail( sAppTable, _nGroupId ) )
	{
		InitRedisConnect( sAppTable, _nGroupId ); 
	}
	else
	{

	}
}

RedisProxyBackEnd::~RedisProxyBackEnd()
{
	// 回收链接
	string sAppTableGroupId	= _oGroupInfo.sapptable() + "." + TC_Common::tostr( _oGroupInfo.ngroupid() );
    if( !_sAppTable.empty() && _nGroupId != 0 )
    {
        sAppTableGroupId    = _sAppTable + "." + TC_Common::tostr( _nGroupId );
    }
	if( ServerConfigMng::getInstance()->GetSObj( "connpoolmode" ) == "vector")
	{
		CRedisConnectPoolFactory<string>::getInstance()->RecoverConnect( _pItem, sAppTableGroupId, _bHealthRedisConn );
	}
	else if( ServerConfigMng::getInstance()->GetSObj( "connpoolmode") == "queue" )
	{
		CRedisConnectPoolFactory_Queue<string>::getInstance()->RecoverConnect( _pQueueItem, sAppTableGroupId, _bHealthRedisConn );
	}
}

void RedisProxyBackEnd::InitRedisConnect( const string & sAppTable, const int & nGroupId )
{
    // 如果链接池没有有效链接,则进行本地创建
    string sAppTableGroupId = sAppTable + "." + TC_Common::tostr( nGroupId );
	MY_TRY
		int64_t nGetRedisStartCost = TNOWMS;
		if( ServerConfigMng::getInstance()->GetSObj( "connpoolmode" ) == "vector")
		{
			 _pItem	= CRedisConnectPoolFactory<string>::getInstance()->GetRedisConnect( sAppTableGroupId );
			if( _pItem != NULL )
			{
				_pRedisConn = _pItem->_pRedisConn;
			}
		}
		else if( ServerConfigMng::getInstance()->GetSObj( "connpoolmode") == "queue" )
		{
			 _pQueueItem	= CRedisConnectPoolFactory_Queue<string>::getInstance()->GetRedisConnect( sAppTableGroupId );
			if( _pQueueItem != NULL )
			{
				_pRedisConn = _pQueueItem->_pRedisConn;
			}
		}
		int64_t nGetRedisEndCost = TNOWMS;
		int64_t nGetRedisTimeout = ServerConfigMng::getInstance()->GetIObj( "getredistimeout" );
		if( nGetRedisEndCost - nGetRedisStartCost > nGetRedisTimeout )
		{
			RedisProxyPrometheus::getInstance()->Report( "Timeout:GetRedisTimeout" );
			MYBOOST_LOG_ERROR( OS_KV( "apptablegroupid", sAppTableGroupId ) 
					<< OS_KV( "msgid", _sMsgid )
					<< " InitRedisConnect Timeout." << OS_KV( "pRedisConnAddr", _pRedisConn ) 
					<< OS_KV( "getrediscost", nGetRedisEndCost - nGetRedisStartCost ) );
		}
	MYBOOST_CATCH( "GetRedisConnect Error", LOG_ERROR );
}

void RedisProxyBackEnd::InitRedisConnect( const GroupInfo & oGroupInfo, const AppTableInfo & oAppTableInfo )
{
    string sAppTable    = oAppTableInfo.sredisapp() + "." + oAppTableInfo.sredistable();
    InitRedisConnect( sAppTable, oGroupInfo.ngroupid() );
}

int RedisProxyBackEnd::CreateRedisConnect( const string & sHost, const int nPort, const string & sPasswd )
{
	int nRet	= RedisProxy::RESULT_SUCC;
	int nTry	= 0;
	while( nTry < _nRedisConnectMaxTryTimes )
	{
		_pRedisConn	= redisConnect( sHost.c_str(), nPort );
		if( _pRedisConn != NULL && _pRedisConn->err )
		{
			MYBOOST_LOG_ERROR( OS_KV("Error", _pRedisConn->errstr ) );
			nRet	= RedisProxy::RESULT_SYSTEM_ERROR;
		}
		else
		{
			redisSetTimeout( _pRedisConn, _oTimeout );
			if( !sPasswd.empty() )
			{
				string sAuthCmd = "AUTH " + sPasswd;
				redisAppendCommand( _pRedisConn, sAuthCmd.c_str() );	// 命令形式
				_bNeedAuth = true;
			}
			break;
		}
		nTry++;
	}
	
	return nRet;
}

void RedisProxyBackEnd::FreeRedisConnect( )
{
	redisFree( _pRedisConn );
	_pRedisConn = NULL;
}
void RedisProxyBackEnd::TransferRedisErrToProxyErr( const int nRedisErr, int & nProxyErr )
{
	switch( nRedisErr )
	{
		case REDIS_ERR_IO:
			nProxyErr = RESULT_REDIS_IO_ERROR;
			break;
		case REDIS_ERR_EOF:
			nProxyErr = RESULT_REDIS_EOF_ERROR;
			break;
		case REDIS_ERR_PROTOCOL:
			nProxyErr = RESULT_REDIS_PROTOCOL_ERROR;
			break;
		case REDIS_ERR_OOM:
			nProxyErr = RESULT_REDIS_OOM_ERROR;
			break;
			/*
		case REDIS_ERR_TIMEOUT:
			nProxyErr = RESULT_REDIS_TIMEOUT_ERROR;
			break;
			*/
		case REDIS_ERR_OTHER:
			nProxyErr = RESULT_REDIS_OTHER_ERROR;
			break;
		default:
			nProxyErr = RESULT_REDIS_UNKNOWN_ERROR;
			break;
	}
}

void RedisProxyBackEnd::AddPipeResultElement( 
        redisReply * dele, 
        ResultElement & oPipeResultElement,
        size_t nIndex )
{
    if( NULL == dele ) return;

    //MYBOOST_LOG_DEBUG( OS_KV( "msgid", _sMsgid ) << OS_KV("index", nIndex ) 
     //       << OS_KV( "deletype", dele->type ) );
    if ( dele->type == REDIS_REPLY_INTEGER )
    {
        oPipeResultElement.set_nret( RedisProxy::RESULT_SUCC );
        oPipeResultElement.set_nresulttype( RESULTTYPE_INTEGER );
        oPipeResultElement.set_sresultelement( TC_Common::tostr( dele->integer ) );
    }
    else if( dele->type == REDIS_REPLY_STRING ) 
    {
        oPipeResultElement.set_nret( RedisProxy::RESULT_SUCC );
        oPipeResultElement.set_nresulttype( RESULTTYPE_STRING );
        oPipeResultElement.set_sresultelement( dele->str, dele->len );
    }
    else if( dele->type == REDIS_REPLY_NIL )
    {
        oPipeResultElement.set_nret( RedisProxy::RESULT_DATA_NOTFOUND );
        oPipeResultElement.set_nresulttype( RESULTTYPE_NONE );
    }
    else
    {
        oPipeResultElement.set_nret( RedisProxy::RESULT_FAIL );
        oPipeResultElement.set_nresulttype( RESULTTYPE_NONE );
        oPipeResultElement.set_sresultelement( "" );
    }
    //MYBOOST_LOG_DEBUG( OS_KV( "msgid", _sMsgid ) << OS_KV("index", nIndex ) 
    //       << OS_KV( "deletype", dele->type )
    //      << OS_KV( "resultelement", oPipeResultElement.DebugString() ) );
}

int RedisProxyBackEnd::DoPipeLine( PipeLineResponse & oPipeLineRsp )
{
    if( ServerConfigMng::getInstance()->GetSObj( "connpoolmode" ) == "vector" && (_pItem == NULL || _pRedisConn == NULL ) ) 
    {
        MYBOOST_LOG_ERROR(  OS_KV("sMsgid",_sMsgid ) << " DopipeLine pRedisConn NULL. Please Check it." );
        return AICommon::RET_FAIL;
    }
    if( ServerConfigMng::getInstance()->GetSObj( "connpoolmode" ) == "queue" && (_pQueueItem == NULL || _pRedisConn == NULL ) ) 
    {
        MYBOOST_LOG_ERROR(  OS_KV("sMsgid",_sMsgid ) << " DopipeLine pRedisConn NULL. Please Check it." );
        return AICommon::RET_FAIL;
	}

	::google::protobuf::RepeatedPtrField<CmdBody> oCmdBodyList = _oPipeLineReq.ocmdbodylist();
	for( size_t i = 0; i < size_t(oCmdBodyList.size()); i++ )
	{
		if( oCmdBodyList[i].nver() != VERSION_ELEMENTVECTOR )
		{
            redisAppendCommand( _pRedisConn, oCmdBodyList[i].scmd().c_str() );	// 命令形式
		}
		else if( oCmdBodyList[i].nver() == VERSION_ELEMENTVECTOR )
		{
			::google::protobuf::RepeatedPtrField<RequestElement> oRequestElementList 
					= oCmdBodyList[i].orequestelementlist();
			vector< const char * > argv;
			vector<	size_t > argvlen;
			for( size_t j = 0; j < size_t(oRequestElementList.size()); ++ j )
			{
				if( oRequestElementList[j].nrequestelementtype() == REQUESTELEMENTTYPE_CMD || 
						j == 0 )
				{
					argv.push_back( oRequestElementList[j].srequestelement().c_str() );
					argvlen.push_back( oRequestElementList[j].srequestelement().size() );
				}
				else
				{
					argv.push_back( oRequestElementList[j].srequestelement().c_str() );
					argvlen.push_back( oRequestElementList[j].srequestelement().size() );
				}
			}
			redisAppendCommandArgv( _pRedisConn, argv.size(), &(argv[0]), &(argvlen[0]) );
		}
	}

	for( size_t i = 0; i < size_t(oCmdBodyList.size()); i++ )
	{
		int nStatus				= RedisProxy::RESULT_FAIL;
		int nResultType			= RESULTTYPE_NONE;
		std::string sRsp		= "";
		redisReply *pReply		= NULL;
		int	nTryCnt				= 0;
		int nRet				= 0;
		// vector< ResultElement > vecResultElement;
		PipeLineResult & oPipeLineResult = *(oPipeLineRsp.add_opipelineresultlist() );
		
		// 假如第一次失败后，需要重试获取数据
		int nBackEndMaxTryTimes	= ServerConfigMng::getInstance()->GetIObj( "backend_maxtrytimes" );	
		while( nTryCnt < nBackEndMaxTryTimes )
		{
			if ( ( nRet = redisGetReply( _pRedisConn, (void **)&pReply ) ) == REDIS_OK )
			{
				if( pReply != NULL && pReply->type == REDIS_REPLY_STRING )
				{
					// 处理单个结果的；
					nStatus		= RedisProxy::RESULT_SUCC;
					sRsp.assign( pReply->str, pReply->len );
					nResultType = RESULTTYPE_STRING;
				}
				if( pReply != NULL && pReply->type == REDIS_REPLY_INTEGER )
				{
					nStatus		= RedisProxy::RESULT_SUCC;
					sRsp		= TC_Common::tostr( pReply->integer );
					nResultType = RESULTTYPE_INTEGER;
				}
				else if( pReply != NULL && pReply->type == REDIS_REPLY_ARRAY )
				{
					// 处理有序集合和Hash结果和scan
					if( pReply->elements == 0 ) nStatus = RedisProxy::RESULT_DATA_NOTFOUND;
					else
					{
						nStatus		= RedisProxy::RESULT_SUCC;
						nResultType = RESULTTYPE_ARRAY;
						for ( size_t j = 0; j < pReply->elements; ++j )
						{
							ResultElement &oPipeResultElement = *(oPipeLineResult.add_oresultelementlist());
							if ( NULL != pReply->element[j] ) 
							{
								redisReply *ele = pReply->element[j];
								if ( ele->type == REDIS_REPLY_INTEGER )
								{
									oPipeResultElement.set_nret( RedisProxy::RESULT_SUCC );
									oPipeResultElement.set_nresulttype( RESULTTYPE_INTEGER );
									oPipeResultElement.set_sresultelement( TC_Common::tostr( ele->integer ) );
								}
								else if( ele->type == REDIS_REPLY_STRING ) 
								{
									oPipeResultElement.set_nret( RedisProxy::RESULT_SUCC );
									oPipeResultElement.set_nresulttype( RESULTTYPE_STRING );
									oPipeResultElement.set_sresultelement( ele->str, ele->len );
								}
								else if( ele->type == REDIS_REPLY_NIL )
								{

									oPipeResultElement.set_nret( RedisProxy::RESULT_DATA_NOTFOUND );
									oPipeResultElement.set_nresulttype( RESULTTYPE_NONE );
								}
                                else if( ele->type == REDIS_REPLY_ARRAY )
                                {
                                    for( size_t k = 0; k < ele->elements; ++ k )
                                    {
                                        if( NULL == ele->element[k] ) continue;
                                        if( k != 0 )
                                        {
                                            ResultElement & oPipeResultElementEx = *(oPipeLineResult.add_oresultelementlist());
                                            AddPipeResultElement( ele->element[k], oPipeResultElementEx, k );
                                        }
                                        else
                                        {
                                            AddPipeResultElement( ele->element[k], oPipeResultElement, k );
                                        }
                                    }
                                }
                                else
                                {
                                    oPipeResultElement.set_nret( RedisProxy::RESULT_FAIL );
                                    oPipeResultElement.set_nresulttype( RESULTTYPE_NONE );
                                    oPipeResultElement.set_sresultelement( "" );
                                }
                                //MYBOOST_LOG_DEBUG( OS_KV( "msgid", _sMsgid ) << OS_KV("index", j) 
                                 //       << OS_KV( "eletype", ele->type ) );
                            }
                        }
                    }
                }
                else if( pReply->type == REDIS_REPLY_ERROR )
                {
                    MYBOOST_LOG_ERROR( OS_KV( "msgid", _sMsgid ) << OS_KV("Error", pReply->str ) ); 
                    nStatus = RedisProxy::RESULT_FAIL;
                    if( strstr( pReply->str, "ERR unknown command" ) != NULL )
                    {
                        nStatus = RedisProxy::RESULT_UNKNOWN_COMMAND;
                    }
                    else if( strstr( pReply->str, "out of range" ) != NULL )
                    {
						nStatus = RedisProxy::RESULT_REDIS_INTOUTOFRANGE_ERROR;
                    }
                    else if( strstr( pReply->str, "BUSYKEY Target key name already exists" ) )
                    {
						nStatus = RedisProxy::RESULT_REDIS_INTOUTOFRANGE_ERROR;
                    }
				}
				else if( pReply->type == REDIS_REPLY_NIL )
				{
					nStatus		= RedisProxy::RESULT_DATA_NOTFOUND;
				}
				else if( pReply->type == REDIS_REPLY_STATUS )
				{
					nStatus		= RedisProxy::RESULT_SUCC;
					sRsp.assign( pReply->str, pReply->len );
					nResultType = RESULTTYPE_STRING;
				}

				freeReplyObject(pReply);
				break;
			}
			else if( _pRedisConn != NULL && _pRedisConn->err )
			{
				nStatus = _pRedisConn->err;
				TransferRedisErrToProxyErr( _pRedisConn->err, nStatus );

				// 如果出现一些失败例如EAGIAN，快速回收链接
				_bHealthRedisConn = false;
				MYBOOST_LOG_ERROR( "GetReply Error." << OS_KV("msgid",_sMsgid ) 
						<< OS_KV("sid",oCmdBodyList[i].ssid() )
						<< OS_KV( "errstr", _pRedisConn->errstr ) << OS_KV( "rediserr", _pRedisConn->err ) 
						<< OS_KV( "status", nStatus ) << OS_KV( "ret", nRet ) << OS_KV( "retry", nTryCnt ) 
						<< OS_KV( "pRedisConnAddr", _pRedisConn ) );
			}
			nTryCnt ++;
		}
		MYBOOST_LOG_DEBUG( "GetReply Finish." << OS_KV("msgid",_sMsgid ) 
				<< OS_KV("sid",oCmdBodyList[i].ssid() )
				<< OS_KV( "rsp", sRsp ) << OS_KV( "status", nStatus ) << OS_KV( "ret", nRet ) );

		oPipeLineResult.set_ssid( oCmdBodyList[i].ssid() );
		oPipeLineResult.set_sresult( sRsp.c_str(), sRsp.size() );
		oPipeLineResult.set_nresulttype( nResultType );
		oPipeLineResult.set_nret( nStatus );
	}
	return AICommon::RET_SUCC;
}
