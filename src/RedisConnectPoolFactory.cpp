#include "RedisConnectPoolFactory.h"
#include "ServerConfig.h"
#include "util/myboost_log.h"
#include <assert.h>
#include "RedisProxyPrometheus.h"
using namespace taf;

template< class T >
CRedisConnectPoolFactory< T >::CRedisConnectPoolFactory()
{
    _bIsKeepAliveRuning = false;
}

template< class T >
CRedisConnectPoolFactory< T >::~CRedisConnectPoolFactory()
{
    this->DestoryConnPool();
}

template< class T >
void CRedisConnectPoolFactory< T >::Init( 
		const RedisConf & oRedisConf, 
		const RedisConnPoolConf & oRedisConnPoolConf, 
		T Index, bool bIsNeedKeepAliveRun, int nKeepAliveTime, int nKeepAliveRand )
{
	TC_ThreadRecMutex oLock;
	_mapLock.insert( std::pair< T, TC_ThreadRecMutex >( Index, oLock ) );

	_nKeepAliveTime			= nKeepAliveTime<=0?5:nKeepAliveTime;
	_nKeepAliveRand			= nKeepAliveRand<=0?5:nKeepAliveRand;
	int nCreateSize	= oRedisConnPoolConf._nCreateSize <= 0?MIN_CONNECT:oRedisConnPoolConf._nCreateSize;
	_mapRedisConnPoolConf.insert( std::pair<T, RedisConnPoolConf>( Index, oRedisConnPoolConf ));
	_mapRedisConf.insert( std::pair<T, RedisConf>( Index, oRedisConf ) );
	
	RedisConnPoolStatus oRedisConnPoolStatus;
	oRedisConnPoolStatus._nCurrSize			= 0;
	oRedisConnPoolStatus._nUsed				= 0;
	oRedisConnPoolStatus._nAutoIncrement	= 0;
	assert( nCreateSize <= _mapRedisConnPoolConf[Index]._nMaxSize );

	// 创建多个链接并且放入到列表中
	while( nCreateSize -- )
	{
		CRedisConnectItem * pItem = new CRedisConnectItem( oRedisConf );

		if( pItem != NULL )
		{
			pItem->CreateConnectItem();

			oRedisConnPoolStatus._nCurrSize++;
			oRedisConnPoolStatus._nAutoIncrement++;
			pItem->_nUsedCnt	= 0;
			pItem->_nUsedTime	= 0;
			pItem->_nItemId		= oRedisConnPoolStatus._nAutoIncrement;
			_mapConnPool[ Index ].push_back( pItem );
		}
		else
		{
			RedisProxyPrometheus::getInstance()->ReportPlus( "RedisConnPoolCounter", "exception:OOM" );
			throw TC_RedisConnectPool_Exception("[CRedisConnectPoolFactory Excep]: Init Fail(OOM)." );
		}
	}

	_mapRedisConnPoolStatus.insert( std::pair<T,RedisConnPoolStatus>( Index, oRedisConnPoolStatus ));

	// 启动一个线程管理所有表链接
	if( bIsNeedKeepAliveRun && !_bIsKeepAliveRuning ) 
	{
		this->start();
		_bIsKeepAliveRuning = true;
	}

}

template< class T >
int CRedisConnectPoolFactory< T >::CreateConnection( const RedisConf & oRedisConf, T Index )
{
	taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( _mapLock[Index] );
	int nRet = 0;
	if( _mapRedisConnPoolStatus[Index]._nCurrSize == _mapRedisConnPoolConf[Index]._nMaxSize )
	{
		RedisProxyPrometheus::getInstance()->ReportPlus( "RedisConnPoolCounter", "exception:cursize_eq_maxsize" );
		throw TC_RedisConnectPool_Exception( "[CRedisConnectPoolFactory::CreateConnection] currsize == maxsize",  nRet );
	}
	CRedisConnectItem * pItem = new CRedisConnectItem( oRedisConf );
	if( pItem != NULL )
	{
		nRet = pItem->CreateConnectItem();
		if( nRet == 0 )
		{
			_mapRedisConnPoolStatus[Index]._nCurrSize++;
			_mapRedisConnPoolStatus[Index]._nAutoIncrement++;
			pItem->_nItemId = _mapRedisConnPoolStatus[Index]._nAutoIncrement;
			_mapConnPool[ Index ].push_back( pItem );
		}
		else
		{
			RedisProxyPrometheus::getInstance()->ReportPlus( "RedisConnPoolCounter", "exception:createitemerror" );
			throw TC_RedisConnectPool_Exception( "[CRedisConnectPoolFactory::CreateConnection] CreateConnectItem error", nRet );
		}
	}

	return 0;
}

template< class T >
typename CRedisConnectPoolFactory<T>::CRedisConnectItem * CRedisConnectPoolFactory<T>::GetRedisConnect( T Index )
{
	taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( _mapLock[Index] );
	CRedisConnectItem * pRedisItem = NULL;

	//MYBOOST_LOG_INFO( "GetRedisConnect start." << OS_KV( "index", Index ) );
	int nIndex = 0;
	for( typename vector< CRedisConnectItem * >::iterator it = _mapConnPool[ Index ].begin();
			it != _mapConnPool[ Index ].end(); ++ it )
	{
		if( (*it) == NULL || (*it)->_bUsed || !(*it)->_bHealth )
		{
			continue;
		}
		else 
        {
			//MYBOOST_LOG_INFO( "GetRedisConnecting.|" << OS_KV( "index", Index ) << OS_KV( "nIndex", nIndex ) );
            (*it)->_bUsed = true;
            (*it)->_nUsedCnt ++;
			(*it)->_nUsedTime = TNOW;
            pRedisItem = ( *it );
            _mapRedisConnPoolStatus[Index]._nUsed++;
			nIndex ++;
            break;
        }
    }

	if( pRedisItem == NULL )
	{
		/*
		MYBOOST_LOG_INFO( "GetRedisConnecting." << OS_KV( "index", Index)
				<< OS_KV( "currsize", _mapRedisConnPoolStatus[Index]._nCurrSize )
				<< OS_KV( "maxsize", _mapRedisConnPoolConf[Index]._nMaxSize ) );
				*/

		if( _mapRedisConnPoolStatus[Index]._nCurrSize < _mapRedisConnPoolConf[Index]._nMaxSize )
		{
			pRedisItem = new CRedisConnectItem( _mapRedisConf[Index] );
			if( pRedisItem != NULL )
			{
				int nRet = pRedisItem->CreateConnectItem();
				if( nRet == 0 )
				{
					_mapRedisConnPoolStatus[Index]._nCurrSize++;
					_mapRedisConnPoolStatus[Index]._nAutoIncrement++;
					pRedisItem->_nItemId	= _mapRedisConnPoolStatus[Index]._nAutoIncrement;
					_mapConnPool[ Index ].push_back( pRedisItem );
					pRedisItem->_bUsed		= true;
					pRedisItem->_nUsedCnt++;
					pRedisItem->_nUsedTime	= TNOW;
					_mapRedisConnPoolStatus[Index]._nUsed++;
				}
				else
				{
					RedisProxyPrometheus::getInstance()->ReportPlus( "RedisConnPoolCounter", 
							"exception:createerror" );
					throw TC_RedisConnectPool_Exception( 
							"[CRedisConnectPoolFactory::GetRedisConnect] CreateConnectItem error", nRet );
				}
			}
		}
		else
		{
			RedisProxyPrometheus::getInstance()->ReportPlus( "RedisConnPoolCounter", "exception:notidle" );
			throw TC_RedisConnectPool_Exception( 
					"[CRedisConnectPoolFactory::GetRedisConnect] RedisConnectPool not idle." );
		}
	}
	/*
	MYBOOST_LOG_INFO( "GetRedisConnect Succ." << OS_KV( "index", Index)
		<< OS_KV( "currsize", _mapRedisConnPoolStatus[Index]._nCurrSize )
		<< OS_KV( "maxsize", _mapRedisConnPoolConf[Index]._nMaxSize ) );
		*/

    return pRedisItem;
}

template< class T >
size_t CRedisConnectPoolFactory<T>::GetCurrSize( T Index )
{
    return _mapRedisConnPoolStatus[Index]._nCurrSize;
}

template< class T >
size_t CRedisConnectPoolFactory<T>::GetUsedSize( T Index )
{
    return _mapRedisConnPoolStatus[Index]._nUsed;
}

template< class T>
void CRedisConnectPoolFactory<T>::SetMaxSize( T Index, int nMaxSize )
{
	taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( _mapLock[Index] );
    _mapRedisConnPoolConf[Index]._nMaxSize = nMaxSize; 
}

template< class T >
void CRedisConnectPoolFactory<T>::DestoryConnPool()
{
    for( typename map< T, vector< CRedisConnectItem* > >::iterator mIt = _mapConnPool.begin();
            mIt != _mapConnPool.end(); ++mIt )
    {
		taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[mIt->first] );
        for( typename vector< CRedisConnectItem* >::iterator it = mIt->second.begin(); 
                it != mIt->second.end(); ++ it )
        {
            if( (*it) != NULL /*&& (*it)->_bUsed == false*/ )
            {
                (*it)->DestoryConnectItem();
                delete (*it);
                (*it) = NULL;
				_mapRedisConnPoolStatus[mIt->first]._nCurrSize--;
				_mapRedisConnPoolStatus[mIt->first]._nUsed++;
				if( _mapRedisConnPoolStatus[mIt->first]._nCurrSize < 0 )
				{
					throw TC_RedisConnectPool_Exception( "[CRedisConnectPoolFactory::DestoryConnPool] nCurrSize < 0." );
				}
            }
        }
        mIt->second.clear();
    }
}

template< class T >
void CRedisConnectPoolFactory<T>::RecoverConnect( CRedisConnectItem * pRedisItem, T Index, bool bHealthRedisConn )
{
	taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[Index] );
	//MYBOOST_LOG_INFO( "RecoverConnect." << OS_KV( " Index", Index ) << OS_KV( "pRedisItemAddr", pRedisItem ) );
    if( pRedisItem != NULL && pRedisItem->_bUsed )
    {
        pRedisItem->RecoverConnectItem();     
		pRedisItem->SetConnectHealth( bHealthRedisConn );
		_mapRedisConnPoolStatus[Index]._nUsed--;
    }

}


template< class T >
void CRedisConnectPoolFactory<T>::run()
{
    while( 1 )
    {
        sleep( _nKeepAliveTime );		
		int nCurrTime  = TNOW;
        for( typename map< T, vector< CRedisConnectItem* > >::iterator mIt = _mapConnPool.begin();
                mIt != _mapConnPool.end(); ++ mIt )
        {
			T Index = mIt->first;
			std::string sTableName = TC_Common::replace( Index, ".", "" );
			RedisProxyPrometheus::getInstance()->ReportPlusGauge( "RedisConnPoolGauge", sTableName+":currsize", 
					_mapRedisConnPoolStatus[Index]._nCurrSize );

			//taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[Index] );
            for( typename vector< CRedisConnectItem * >::iterator it = mIt->second.begin();
                    it != mIt->second.end(); )
            {
				taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[Index] );
                if( (*it) == NULL || (*it)->_bUsed ) 
				{
					++it; 
					continue;
				}
                else
                {
					// 长链接空闲控制策略
                    if( (*it)->_nUsedCnt < _mapRedisConnPoolConf[Index]._nMinUsedCnt 
						&& ( nCurrTime - (*it)->_nUsedTime ) > _mapRedisConnPoolConf[Index]._nMaxIdleTime
						&& ( _mapRedisConnPoolStatus[Index]._nCurrSize > _mapRedisConnPoolConf[Index]._nMinSize ) ) 
                    {
						MYBOOST_LOG_ERROR( "IsIdleConn DestoryConnectItem." 
								<< OS_KV("pRedisConnectAddr", (*it)->_pRedisConn) );
                        delete (*it);
                        (*it) = NULL;
                        it = mIt->second.erase( it );
                        _mapRedisConnPoolStatus[Index]._nCurrSize--;
                        continue;
                    }
					else 
					{
						if( !(*it)->IsKeepAliveV1(_nKeepAliveRand) )
						{
							MYBOOST_LOG_ERROR( "IsNotKeepAlive DestoryConnectItem." 
								<< OS_KV("pRedisConnectAddr", (*it)->_pRedisConn) );
							delete (*it);
							(*it) = NULL;
							it = mIt->second.erase( it ); 
							_mapRedisConnPoolStatus[Index]._nCurrSize--;
							if( _mapRedisConnPoolStatus[Index]._nCurrSize < _mapRedisConnPoolConf[Index]._nMaxSize )
							{
								CRedisConnectItem * pItem = new CRedisConnectItem( _mapRedisConf[Index] );
								if( pItem != NULL )
								{
									int nRet = pItem->CreateConnectItem();
									if( nRet == 0 )
									{
										_mapRedisConnPoolStatus[Index]._nCurrSize++;
										_mapRedisConnPoolStatus[Index]._nAutoIncrement++;
										pItem->_nItemId = _mapRedisConnPoolStatus[Index]._nAutoIncrement;
										_mapConnPool[ Index ].push_back( pItem );
									}
									else
									{
										MYBOOST_LOG_ERROR( "KeepAlive CreateConnectItem Error." 
												<< OS_KV( "ret", nRet ) );
									}
								}
							}
							else
							{
								MYBOOST_LOG_ERROR( "KeepAlive RedisConnectPool OverHead MaxSize." 
										<< OS_KV( "currsize", _mapRedisConnPoolStatus[Index]._nCurrSize )
										<< OS_KV( "maxsize", _mapRedisConnPoolConf[Index]._nMaxSize ) );
							}
						}
						else
						{
							++it;		
						}
					}
                }
            }
        }
    }
}

template< class T >
void CRedisConnectPoolFactory<T>::DestoryConnect( CRedisConnectItem * pRedisItem )
{
    if( pRedisItem != NULL )
    {
        for( typename map< T, vector< CRedisConnectItem * > >::iterator mIt = _mapConnPool.begin();
                mIt != _mapConnPool.end(); ++mIt )
        {
			taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[mIt->first] );
            for( typename vector< CRedisConnectItem * >::iterator it = mIt->second.begin();
                    it != mIt->second.end(); ++ it )
            {
                if( (*it) != NULL && (*it)->_nItemId == pRedisItem->_nItemId )
                {
                    pRedisItem->DestoryConnectItem();
                    //_nCurrSize--;
                    mIt->second.erase( it );
                    break;
                }
            }
        }
    }
}

template< class T >
CRedisConnectPoolFactory<T>::CRedisConnectItem::CRedisConnectItem( const RedisConf & oRedisConf ):
		_bUsed( false ), _nUsedCnt(0), _bHealth( true )
{
	// 通过redis配置或者信息去初始化_redisConn
	_oRedisConf = oRedisConf;
}

template< class T >
CRedisConnectPoolFactory<T>::CRedisConnectItem::~CRedisConnectItem()
{
	this->DestoryConnectItem();
}

// 如果是从服务初始化情况可以先进行验证 
template< class T >
int CRedisConnectPoolFactory<T>::CRedisConnectItem::CreateConnectItem()
{
	int nTry	= 0;
	int nRet	= 0;
	/*
	MYBOOST_LOG_INFO( "CreateConnectItem!" << OS_KV( "maxtrytimes",_oRedisConf._nConnectMaxTryTimes) 
				<< OS_KV( "host", _oRedisConf._sHost ) << OS_KV( "port", _oRedisConf._nPort ) );
				*/
	while( nTry < _oRedisConf._nConnectMaxTryTimes )
	{
		_pRedisConn	= redisConnectWithTimeout( _oRedisConf._sHost.c_str(), _oRedisConf._nPort, _oRedisConf._nTimeout );
		if( _pRedisConn == NULL )
		{
			nRet = -1;
		}
		else if( _pRedisConn->err )
		{
			redisFree( _pRedisConn );
			nRet = -2;
		}
		else
		{
			redisSetTimeout( _pRedisConn, _oRedisConf._nTimeout );
			if( !_oRedisConf._sPasswd.empty() )
			{
				string sAuthCmd = "AUTH " + _oRedisConf._sPasswd;
				redisAppendCommand( _pRedisConn, sAuthCmd.c_str() );	// 命令形式
				redisReply *pReply		= NULL;
				if ( ( nRet = redisGetReply( _pRedisConn, (void **)&pReply ) ) == REDIS_OK )
				{
					if( pReply->type == REDIS_REPLY_STATUS )
					{
						MYBOOST_LOG_INFO( "Auth Succ." );
					}
					else 
					{
						MYBOOST_LOG_ERROR( "Auth Fail." );
					}
				}
				if( pReply != NULL )
					freeReplyObject(pReply);
			}
			if( nRet == REDIS_OK )
				break;
		}
		nTry++;
	}
	if( nRet == REDIS_OK )
	{
		MYBOOST_LOG_INFO( "CreateConnectItem Succ!"  << OS_KV( "pRedisConnectAddr", _pRedisConn )
				<< OS_KV( "maxtrytimes",_oRedisConf._nConnectMaxTryTimes) 
			<< OS_KV( "host", _oRedisConf._sHost ) << OS_KV( "port", _oRedisConf._nPort ) );
	}
	else
	{
		MYBOOST_LOG_ERROR( "CreateConnectItem Error!" << OS_KV( "ret", nRet ) 
			<< OS_KV( "host", _oRedisConf._sHost ) << OS_KV( "port", _oRedisConf._nPort ) );
	}
	return nRet;
}

template< class T >
bool CRedisConnectPoolFactory<T>::CRedisConnectItem::IsKeepAlive()
{
	// 对于链接正常，但Redis负载超高，无法响应请求的情况如何处理？ PING 包确认链接正常，INFO包确认Redis负载情况 
	// 1 客户端每隔一个时间间隔发生一个探测包给服务器
	// 2 客户端发包时启动一个超时定时器
	// 3 服务器端接收到检测包，应该回应一个包
	// 4 如果客户机收到服务器的应答包，则说明服务器正常
	// 5 如果客户端重试依然没有收到应答包，则说明服务器挂了
	// 6 在心跳检测中的链接不能被使用
	// 7 对不健康或者空闲的链接进行心跳检测，在进行心跳检测的链接暂时不能被外部获取，避免所有的链接都在进行心跳检测
	//if( _bHealth ) return true;
	redisReply * pReply = NULL;
	int nTry = 0;
	bool bIsKeepAlive = false;
	int nRet = REDIS_OK;
	_bUsed = true;
	int64_t nStartTime	= TNOWMS;
	while( nTry < MAX_TRY_TIMES )
	{
		redisAppendCommand( _pRedisConn, "PING" );
		if( ( nRet = redisGetReply( _pRedisConn, (void**) &pReply ) ) == REDIS_OK )
		{
			if( pReply->type == REDIS_REPLY_STATUS && strcmp( pReply->str, "PONG" ) == 0 )
			{
				bIsKeepAlive = true;
				break;
			}
			else
			{
				MYBOOST_LOG_ERROR( "Is not KeepAlive!" << OS_KV( "pRedisConnectAddr", _pRedisConn ) );
				bIsKeepAlive = false;
			}
		}
		else
		{
			MYBOOST_LOG_ERROR( "KeepAlive Reply Error!" << OS_KV( "pRedisConnectAddr", _pRedisConn ) 
					<< OS_KV( "ret", nRet ) );
		}
		nTry ++;
	}
	
	if( pReply != NULL )
	{
		freeReplyObject(pReply);
		pReply = NULL;
	}
        if( !bIsKeepAlive )
		{
			_bHealth = false;
			MYBOOST_LOG_ERROR( "IsKeepAlive End!" << OS_KV( "pRedisConnectAddr", _pRedisConn ) 
				<< OS_KV("try", nTry ) << OS_KV( "usectimeout", _oRedisConf._nTimeout.tv_usec )
				<< OS_KV( "sectimeout", _oRedisConf._nTimeout.tv_sec ) );
		}
		else
		{
			_bUsed = false;
		}
	int64_t nEndTime	= TNOWMS;
	int64_t nCost		= nEndTime - nStartTime;
	if( nCost > 5 )
	{
		RedisProxyPrometheus::getInstance()->Report( "Timeout:KeepAlivePingTimeout" );
		MYBOOST_LOG_ERROR( "Ping Timeout." << OS_KV("cost",nCost) 
				<< OS_KV( "pRedisConnectAddr", _pRedisConn ) 
			<< OS_KV("try", nTry ) << OS_KV( "usectimeout", _oRedisConf._nTimeout.tv_usec )
			<< OS_KV( "sectimeout", _oRedisConf._nTimeout.tv_sec ) );

	}


	return bIsKeepAlive;
}

template< class T >
bool CRedisConnectPoolFactory<T>::CRedisConnectItem::IsKeepAliveV1( int nKeepAliveRand )
{
	// 对于链接正常，但Redis负载超高，无法响应请求的情况如何处理？ PING 包确认链接正常，INFO包确认Redis负载情况 
	// 1 客户端每隔一个时间间隔发生一个探测包给服务器
	// 2 客户端发包时启动一个超时定时器
	// 3 服务器端接收到检测包，应该回应一个包
	// 4 如果客户机收到服务器的应答包，则说明服务器正常
	// 5 如果客户端重试依然没有收到应答包，则说明服务器挂了
	// 6 在心跳检测中的链接不能被使用
	// 7 对不健康或者空闲的链接进行心跳检测，在进行心跳检测的链接暂时不能被外部获取，避免所有的链接都在进行心跳检测
	//if( _bHealth ) return true;
	srand((unsigned)time(NULL));
	if((rand()%(nKeepAliveRand)+1)!=1) return true;
	redisReply * pReply;
	int nTry = 0;
	bool bIsKeepAlive = false;
	_bUsed = true;
	int64_t nStartTime	= TNOWMS;
	while( nTry < MAX_TRY_TIMES )
	{
		pReply = static_cast<redisReply*>(redisCommand( _pRedisConn, "PING" ));
		if( pReply != NULL && pReply->type == REDIS_REPLY_STATUS && strcasecmp( pReply->str, "pong" ) == 0 )
		{
			bIsKeepAlive = true;
			break;
		}
		else
		{
			MYBOOST_LOG_ERROR( "Is not KeepAlive!" << OS_KV( "pRedisConnectAddr", _pRedisConn ) );
			bIsKeepAlive = false;
		}
		nTry ++;
	}
	
	freeReplyObject(pReply);

	if( !bIsKeepAlive )
	{
		_bHealth = false;
		MYBOOST_LOG_ERROR( "IsKeepAlive End!" << OS_KV( "pRedisConnectAddr", _pRedisConn ) 
			<< OS_KV("try", nTry ) << OS_KV( "usectimeout", _oRedisConf._nTimeout.tv_usec )
			<< OS_KV( "sectimeout", _oRedisConf._nTimeout.tv_sec ) );
	}
	else
	{
		_bUsed = false;
	}
	int64_t nEndTime	= TNOWMS;
	int64_t nCost		= nEndTime - nStartTime;
	if( nCost > 5 )
	{
		RedisProxyPrometheus::getInstance()->Report( "Timeout:KeepAlivePingTimeout" );
		MYBOOST_LOG_ERROR( "Ping Timeout." << OS_KV("cost",nCost) 
				<< OS_KV( "pRedisConnectAddr", _pRedisConn ) 
			<< OS_KV("try", nTry ) << OS_KV( "usectimeout", _oRedisConf._nTimeout.tv_usec )
			<< OS_KV( "sectimeout", _oRedisConf._nTimeout.tv_sec ) );
	}

	return bIsKeepAlive;
}

template< class T >
void CRedisConnectPoolFactory<T>::CRedisConnectItem::DestoryConnectItem()
{
	MYBOOST_LOG_INFO( "DestoryConnectItem." << OS_KV( "pRedisConn", _pRedisConn )
		<< OS_KV( "used", _bUsed ) << OS_KV( "itemid", _nItemId ) << OS_KV( "usedTime", _nUsedTime ) );
	if( _pRedisConn != NULL )
	{
		redisFree( _pRedisConn );
		_pRedisConn = NULL;
	}
}

template< class T >
void CRedisConnectPoolFactory<T>::CRedisConnectItem::RecoverConnectItem()
{
    _bUsed = false;
}

template< class T >
void CRedisConnectPoolFactory<T>::CRedisConnectItem::SetConnectHealth( bool bHealthRedisConn )
{
    _bHealth = bHealthRedisConn;
}

template class CRedisConnectPoolFactory<string>; // 动态声明作为string处理
