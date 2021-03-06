#include "RedisConnectPoolFactory_Queue.h"
#include "ServerConfig.h"
#include "util/myboost_log.h"
#include <assert.h>
#include "RedisProxyPrometheus.h"
#include "RedisProxyFailOver.h"
#include "RedisProxyZkData.h"
using namespace taf;

template< class T >
CRedisConnectPoolFactory_Queue< T >::CRedisConnectPoolFactory_Queue()
{
    _bIsKeepAliveRuning = false;
}

template< class T >
CRedisConnectPoolFactory_Queue< T >::~CRedisConnectPoolFactory_Queue()
{
    this->DestoryConnPool();
}

template< class T >
void CRedisConnectPoolFactory_Queue<T>::InitRedisConnectPoolFactoryByGuard()
{
    bool IsFromGuard = true;
    InitRedisConnectPoolFactoryByZk( IsFromGuard );
}

template< class T >
void CRedisConnectPoolFactory_Queue<T>::InitRedisConnectPoolFactoryByZk( bool bIsFromGuard )
{
	const MapStr2GIdGroupInfo & mapAppTable2GidGroupInfo = RedisProxyZkData::getInstance()->GetGIdGroupInfo();
	for( MapStr2GIdGroupInfo::const_iterator msIt = mapAppTable2GidGroupInfo.begin();
			msIt != mapAppTable2GidGroupInfo.end(); ++ msIt )
	{
		std::string sAppTable	        = msIt->first;
		MYBOOST_LOG_DEBUG( OS_KV( "apptable", sAppTable )  );
		const RedisConf & oTmpRedisConf	= RedisProxyZkData::getInstance()->GetRedisConfByAppTable( sAppTable );
		RedisConf oRedisConf			= oTmpRedisConf;
		const RedisConnPoolConf & oRedisConnPoolConf	= 
				RedisProxyZkData::getInstance()->GetRedisConnPoolConfByAppTable( sAppTable );
		MYBOOST_LOG_INFO( "RedisConf!" << OS_KV( "maxtrytimes",oRedisConf._nConnectMaxTryTimes ) 
				<< OS_KV( "timeoutsec", oRedisConf._nTimeout.tv_sec ) 
				<< OS_KV( "timeoutusec", oRedisConf._nTimeout.tv_usec ) );
		MYBOOST_LOG_INFO( "RedisConnPoolConf!" << OS_KV( "maxisize",oRedisConnPoolConf._nMaxSize ) 
				<< OS_KV( "minsize", oRedisConnPoolConf._nMinSize ) 
                << OS_KV( "usecnt", oRedisConnPoolConf._nMinUsedCnt )
				<< OS_KV( "idletime", oRedisConnPoolConf._nMaxIdleTime ) 
				<< OS_KV( "createsize", oRedisConnPoolConf._nMinUsedCnt )
				<< OS_KV( "isneedkeepalive", oRedisConnPoolConf._bIsNeedKeepAlive )
				<< OS_KV( "keepalivetime", oRedisConnPoolConf._nKeepAliveTime )
				<< OS_KV( "isneedreportdetect", oRedisConnPoolConf._bIsNeedReportDetect ) 
				<< OS_KV( "isfromguard", bIsFromGuard ) );
		for( map< int, RedisProxyCommon::GroupInfo >::const_iterator mIt = msIt->second.begin();
				mIt != msIt->second.end(); mIt ++ )
        {
			oRedisConf._sHost				= mIt->second.omaster().sip();
			oRedisConf._nPort				= mIt->second.omaster().nport();
            std::string sAppTableGroupId	= sAppTable + "." + TC_Common::tostr<int>( mIt->first );
			MYBOOST_LOG_DEBUG( OS_KV( "apptablegroupid", sAppTableGroupId ) );
            MY_TRY
                if( !bIsFromGuard )
                    Init( oRedisConf, oRedisConnPoolConf, sAppTableGroupId );
                else
                    Init( oRedisConf, oRedisConnPoolConf, sAppTableGroupId, bIsFromGuard );
            MYBOOST_CATCH( "Init Error", LOG_ERROR );
        }
	}
	MYBOOST_LOG_INFO( "InitRedisConnectPoolFactoryByZk Succ!" );
}

template< class T >
void CRedisConnectPoolFactory_Queue< T >::Init( 
		const RedisConf & oRedisConf, 
		const RedisConnPoolConf & oRedisConnPoolConf, 
		T Index,
        bool bIsFromGuard )
{
	TC_ThreadRecMutex oLock;
	_mapLock.insert( std::pair< T, TC_ThreadRecMutex >( Index, oLock ) );

	_nKeepAliveTime	= oRedisConnPoolConf._nKeepAliveTime<=0?5:oRedisConnPoolConf._nKeepAliveTime;
	int nCreateSize	= oRedisConnPoolConf._nCreateSize <= 0?MIN_CONNECT:oRedisConnPoolConf._nCreateSize;
    typename map< T, RedisConnPoolConf >::iterator mrcpc = _mapRedisConnPoolConf.find( Index );
    if( mrcpc != _mapRedisConnPoolConf.end() )
        mrcpc->second = oRedisConnPoolConf;
    else{
        _mapRedisConnPoolConf.insert( std::pair<T, RedisConnPoolConf>( Index, oRedisConnPoolConf ));
    }

    typename map< T, RedisConf >::iterator mrc = _mapRedisConf.find( Index );
    if( mrc != _mapRedisConf.end() )
        mrc->second = oRedisConf;
    else{
        _mapRedisConf.insert( std::pair<T, RedisConf>( Index, oRedisConf ) );
    }
	
	RedisConnPoolStatus oRedisConnPoolStatus;
	oRedisConnPoolStatus._nCurrSize			= 0;
	oRedisConnPoolStatus._nUsed				= 0;
	oRedisConnPoolStatus._nAutoIncrement	= 0;
	oRedisConnPoolStatus._nReportIndex		= 0;
	oRedisConnPoolStatus._nLastConnState	= REDIS_CONN_OK;
	assert( nCreateSize <= _mapRedisConnPoolConf[Index]._nMaxSize );

    // ??????????????????????????????????????????????????????
	if( oRedisConnPoolConf._bIsNeedReportDetect && bIsFromGuard )
	{
		CRedisConnectItem * pItem = new CRedisConnectItem( oRedisConf );
		if( pItem != NULL )
		{
			int nRet = pItem->CreateConnectItem();
			if( nRet == 0 )
			{
				_mapReportDetectConnPool[ Index ].push( pItem );
			}
			else
			{
                _mapReportDetectConnPool[ Index ];
                MYBOOST_LOG_ERROR( "CreateReportDetectConn error." << OS_KV( "table",Index ) << OS_KV( "ret", nRet ) );
			}
		}
		else
		{
			_mapReportDetectConnPool[ Index ];
            MYBOOST_LOG_ERROR( "Init ReportDetectConnPool Fail(OOM)." << OS_KV( "table",Index ) );
		}
	}

	// ?????????????????????????????????????????????
	while( nCreateSize -- )
	{
		CRedisConnectItem * pItem = new CRedisConnectItem( oRedisConf );

		if( pItem != NULL )
		{
			int nRet = pItem->CreateConnectItem();
			if( nRet == 0 )
			{
				oRedisConnPoolStatus._nCurrSize++;
				oRedisConnPoolStatus._nAutoIncrement++;
				pItem->_nUsedCnt	= 0;
				pItem->_nUsedTime	= 0;
				pItem->_nItemId		= oRedisConnPoolStatus._nAutoIncrement;
				_mapConnPool[ Index ].push( pItem );
			}
			else
			{
				RedisProxyPrometheus::getInstance()->ReportPlus( "RedisConnPoolCounter", 
						"exception:createitemerror" );
                MYBOOST_LOG_ERROR( "CreateConnectItem error." << OS_KV( "table",Index ) );
				throw TC_RedisConnectPool_Exception( 
						"[CRedisConnectPoolFactory_Queue::CreateConnection] CreateConnectItem error", nRet );
			}
		}
		else
		{
			RedisProxyPrometheus::getInstance()->ReportPlus( "RedisConnPoolCounter", "exception:OOM" );
            MYBOOST_LOG_ERROR( "[CRedisConnectPoolFactory_Queue Excep]: Init Fail(OOM)." << OS_KV( "table",Index ) );
			throw TC_RedisConnectPool_Exception("[CRedisConnectPoolFactory_Queue Excep]: Init Fail(OOM)." );
		}
	}

		_mapRedisConnPoolStatus.insert( std::pair<T,RedisConnPoolStatus>( Index, oRedisConnPoolStatus ));

	// ???????????????????????????????????????
	if( oRedisConnPoolConf._bIsNeedKeepAlive && !_bIsKeepAliveRuning ) 
	{
		this->start();
		_bIsKeepAliveRuning = true;
	}
}

template< class T >
void CRedisConnectPoolFactory_Queue< T >::InitByGuard( 
		const RedisConf & oRedisConf, 
		const RedisConnPoolConf & oRedisConnPoolConf, 
		T Index )
{
	TC_ThreadRecMutex oLock;
	_mapLock.insert( std::pair< T, TC_ThreadRecMutex >( Index, oLock ) );

	_nKeepAliveTime			= oRedisConnPoolConf._nKeepAliveTime<=0?5:oRedisConnPoolConf._nKeepAliveTime;
	_mapRedisConnPoolConf.insert( std::pair<T, RedisConnPoolConf>( Index, oRedisConnPoolConf ));
	_mapRedisConf.insert( std::pair<T, RedisConf>( Index, oRedisConf ) );
	
	RedisConnPoolStatus oRedisConnPoolStatus;
	oRedisConnPoolStatus._nCurrSize			= 0;
	oRedisConnPoolStatus._nUsed				= 0;
	oRedisConnPoolStatus._nAutoIncrement	= 0;
	oRedisConnPoolStatus._nReportIndex		= 0;
	oRedisConnPoolStatus._nLastConnState	= REDIS_CONN_OK;
	//assert( nCreateSize <= _mapRedisConnPoolConf[Index]._nMaxSize );

	// ??????????????????????????????????????????????????????
	CRedisConnectItem * pItem = new CRedisConnectItem( oRedisConf );
	if( pItem != NULL )
	{
		int nRet = pItem->CreateConnectItem();
		if( nRet == 0 )
		{
			_mapReportDetectConnPool[ Index ].push( pItem );
		}
		else
		{
            _mapReportDetectConnPool[ Index ];
			MYBOOST_LOG_ERROR( "CreateReportDetectConn error." << OS_KV( "table",Index ) << OS_KV( "ret", nRet ) );
		}
	}
	else
	{
		_mapReportDetectConnPool[ Index ];
		MYBOOST_LOG_ERROR( "Init ReportDetectConnPool Fail(OOM)." << OS_KV( "table",Index ) );
	}

	_mapRedisConnPoolStatus.insert( std::pair<T,RedisConnPoolStatus>( Index, oRedisConnPoolStatus ));

	// ???????????????????????????????????????
	if( oRedisConnPoolConf._bIsNeedKeepAlive && !_bIsKeepAliveRuning ) 
	{
		this->start();
		_bIsKeepAliveRuning = true;
	}
}

template< class T >
int CRedisConnectPoolFactory_Queue< T >::CreateConnection( const RedisConf & oRedisConf, T Index )
{
	taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( _mapLock[Index] );
	int nRet = 0;
	if( _mapRedisConnPoolStatus[Index]._nCurrSize == _mapRedisConnPoolConf[Index]._nMaxSize )
	{
		RedisProxyPrometheus::getInstance()->ReportPlus( "RedisConnPoolCounter", "exception:cursize=maxsize" );
		throw TC_RedisConnectPool_Exception( "[CRedisConnectPoolFactory_Queue::CreateConnection] currsize == maxsize",  nRet );
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
			_mapConnPool[ Index ].push( pItem );
		}
		else
		{
			RedisProxyPrometheus::getInstance()->ReportPlus( "RedisConnPoolCounter", "exception:createitemerror" );
			throw TC_RedisConnectPool_Exception( "[CRedisConnectPoolFactory_Queue::CreateConnection] CreateConnectItem error", nRet );
		}
	}

	return 0;
}

template< class T >
typename CRedisConnectPoolFactory_Queue<T>::CRedisConnectItem * CRedisConnectPoolFactory_Queue<T>::GetRedisConnect( T Index )
{
	CRedisConnectItem * pRedisItem = NULL;
	{
		taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( _mapLock[Index] );
		if( !_mapConnPool[Index].empty() )
		{
			pRedisItem = _mapConnPool[Index].front();
			_mapConnPool[Index].pop();
			_mapRedisConnPoolStatus[Index]._nUsed++;
		}
	}
	
	if( pRedisItem != NULL )
	{
		pRedisItem->_nUsedCnt ++;
		pRedisItem->_nUsedTime = TNOW; 
	}
	else
	{
		taf::TC_LockT<taf::TC_ThreadRecMutex> lockGuard( _mapLock[Index] );
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
					//_mapConnPool[ Index ].push( pRedisItem );
					pRedisItem->_nUsedCnt++;
					pRedisItem->_nUsedTime	= TNOW;
					_mapRedisConnPoolStatus[Index]._nUsed++;
				}
				else
				{
					RedisProxyPrometheus::getInstance()->ReportPlus( "RedisConnPoolCounter", "exception:createerror" );
					throw TC_RedisConnectPool_Exception( 
							"[CRedisConnectPoolFactory_Queue::GetRedisConnect] CreateConnectItem error", nRet );
				}
			}
		}
		else
		{
			RedisProxyPrometheus::getInstance()->ReportPlus( "RedisConnPoolCounter", "exception:notidle" );
			throw TC_RedisConnectPool_Exception( 
					"[CRedisConnectPoolFactory_Queue::GetRedisConnect] RedisConnectPool not idle." );
		}
	}
	MYBOOST_LOG_INFO( "GetRedisConnect Succ." << OS_KV( "index", Index)
		<< OS_KV( "currsize", _mapRedisConnPoolStatus[Index]._nCurrSize )
		<< OS_KV( "maxsize", _mapRedisConnPoolConf[Index]._nMaxSize ) );

    return pRedisItem;
}

template< class T >
size_t CRedisConnectPoolFactory_Queue<T>::GetCurrSize( T Index )
{
    return _mapRedisConnPoolStatus[Index]._nCurrSize;
}

template< class T >
size_t CRedisConnectPoolFactory_Queue<T>::GetUsedSize( T Index )
{
    return _mapRedisConnPoolStatus[Index]._nUsed;
}

template< class T>
void CRedisConnectPoolFactory_Queue<T>::SetMaxSize( T Index, int nMaxSize )
{
    taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[Index] );
    _mapRedisConnPoolConf[Index]._nMaxSize = nMaxSize; 
}

template< class T >
void CRedisConnectPoolFactory_Queue<T>::DestoryConnPool()
{
    for( typename map< T, queue< CRedisConnectItem* > >::iterator mIt = _mapConnPool.begin();
            mIt != _mapConnPool.end(); ++mIt )
    {
		taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[mIt->first] );
		while( !mIt->second.empty() )
		{
			CRedisConnectItem * pRedisItem = mIt->second.front();
			pRedisItem->DestoryConnectItem();
			mIt->second.pop();
            delete pRedisItem;
			_mapRedisConnPoolStatus[mIt->first]._nCurrSize--;
			_mapRedisConnPoolStatus[mIt->first]._nUsed++;
			if( _mapRedisConnPoolStatus[mIt->first]._nCurrSize < 0 )
			{
				throw TC_RedisConnectPool_Exception( "[CRedisConnectPoolFactory_Queue::DestoryConnPool] nCurrSize < 0." );
			}
		}
	}
}

template< class T >
void CRedisConnectPoolFactory_Queue<T>::RecoverConnect( CRedisConnectItem * pRedisItem, T Index, bool bHealthRedisConn )
{
    taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[Index] );
	MYBOOST_LOG_DEBUG( "RecoverConnect." << OS_KV( " Index", Index ) << OS_KV( "pRedisItemAddr", pRedisItem ) );
    if( pRedisItem != NULL )
    {
		pRedisItem->SetConnectHealth( bHealthRedisConn );
		if( !bHealthRedisConn ){
			_mapUnHealthConnPool[Index].push( pRedisItem );
			_mapRedisConnPoolStatus[Index]._nUsed--;
		}
		else
		{
			_mapConnPool[Index].push( pRedisItem );
		}
    }
}

template< class T >
bool CRedisConnectPoolFactory_Queue<T>::DealIdleConnect( CRedisConnectItem * pRedisItem, T Index )
{
	// ???????????????????????????
	int nCurrTime  = TNOW;
	taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[Index] );
	if( pRedisItem->_nUsedCnt < _mapRedisConnPoolConf[Index]._nMinUsedCnt 
		&& ( nCurrTime - pRedisItem->_nUsedTime ) > _mapRedisConnPoolConf[Index]._nMaxIdleTime
		&& ( _mapRedisConnPoolStatus[Index]._nCurrSize > _mapRedisConnPoolConf[Index]._nMinSize ) ) 
	{

        MYBOOST_LOG_ERROR( "IsIdleConn DestoryConnectItem." 
            << OS_KV("pRedisConnectAddr", pRedisItem->_pRedisConn) );
        delete pRedisItem;
        pRedisItem = NULL;
        _mapRedisConnPoolStatus[Index]._nCurrSize--;
		return true;
	}
	return false;
}

template< class T >
bool CRedisConnectPoolFactory_Queue<T>::ResetConnect( CRedisConnectItem * pRedisItem, T Index )
{
	taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[Index] );

	MYBOOST_LOG_INFO( "IsNotKeepAlive DestoryConnectItem." 
		<< OS_KV("pRedisConnectAddr", pRedisItem->_pRedisConn) );
	string sTableName = TC_Common::replace( Index, ".", "" );
	RedisProxyPrometheus::getInstance()->ReportPlus( "RedisConnPoolCounter", sTableName + ":dealnotkeepalivesize" );

	delete pRedisItem;
	pRedisItem = NULL;
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
				_mapConnPool[ Index ].push( pItem );
			}
			else
			{
				MYBOOST_LOG_ERROR( "KeepAlive CreateConnectItem Error." 
					<< OS_KV( "ret", nRet ) );
				return false;
			}
		}
	}
	else
	{
		MYBOOST_LOG_ERROR( "KeepAlive RedisConnectPool OverHead MaxSize." 
				<< OS_KV( "currsize", _mapRedisConnPoolStatus[Index]._nCurrSize )
				<< OS_KV( "maxsize", _mapRedisConnPoolConf[Index]._nMaxSize ) );
	}
	return true;
}

template< class T >
bool CRedisConnectPoolFactory_Queue<T>::ResetReportDetectConnect( CRedisConnectItem * pRedisItem, T Index )
{
	if( pRedisItem != NULL ){ 
		MYBOOST_LOG_INFO( "IsNotKeepAlive DestoryConnectItem." 
			<< OS_KV("pRedisConnectAddr", pRedisItem->_pRedisConn) );
		delete pRedisItem; 
		pRedisItem = NULL; 
	}

	CRedisConnectItem * pItem = new CRedisConnectItem( _mapRedisConf[Index] );
	if( pItem != NULL )
	{
		int nRet = pItem->CreateConnectItem();
		if( nRet == 0 )
		{
			_mapReportDetectConnPool[ Index ].push( pItem );
		}
		else
		{
			MYBOOST_LOG_ERROR( "KeepAlive CreateConnectItem Error." << OS_KV( "ret", nRet ) );
			return false;
		}
	}
	return true;
}

template< class T >
void CRedisConnectPoolFactory_Queue<T>::ReportDetectState( bool bIsPfail, T Index )
{
	// ????????????????????????, ?????????????????????????????????
	taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[Index] );
	int nReportIndex = _mapRedisConnPoolStatus[Index]._nReportIndex % MAX_REPORT_LEN;
	MYBOOST_LOG_DEBUG( OS_KV("table", Index ) << OS_KV( "bIsPfail", bIsPfail ) << OS_KV( "reportIndex", nReportIndex ) );
	if( _mapRedisConnPoolStatus[Index]._nReportIndex < MAX_REPORT_LEN )
	{
		_mapRedisConnPoolStatus[Index]._aReport[ nReportIndex ].nRedisConnState 
			= (bIsPfail?REDIS_CONN_PFAIL:REDIS_CONN_OK);
		_mapRedisConnPoolStatus[Index]._aReport[ nReportIndex ].nTime = TNOW;
		_mapRedisConnPoolStatus[Index]._nReportIndex++;
	}
	MYBOOST_LOG_DEBUG( OS_KV( "reportIndex", _mapRedisConnPoolStatus[Index]._nReportIndex) );
	if( _mapRedisConnPoolStatus[Index]._nReportIndex == MAX_REPORT_LEN )
	{
		// ?????????????????????????????????????????????
		// OK->FAIL,??????3???????????????
		// FAIL->OK, ????????????3??????????????????????????????????????????
		int nCurrentConnState = REDIS_CONN_OK;
		for( size_t i = 0; i < size_t(MAX_REPORT_LEN); ++ i )
		{
			if( _mapRedisConnPoolStatus[Index]._aReport[i].nRedisConnState == REDIS_CONN_OK )
			{
				break;
			}
			if( i == MAX_REPORT_LEN - 1 ) nCurrentConnState = REDIS_CONN_FAIL;
		}

		{
			vector< string > vecAppInfo = TC_Common::sepstr<string>( Index, "." );
			string sApp			= vecAppInfo[0];
			string sTable		= vecAppInfo[1];
			int nGroupId		= TC_Common::strto<int>(vecAppInfo[2]);
			// 1. ????????????????????????????????? delete
			// 2. ??????????????????????????????????????? delete
			// 3. ???????????????????????????????????????????????????????????????CPU???????????????????????????????????????
			RedisProxyFailOver::getInstance()->UpdateGroupState( sApp + "." + sTable, nGroupId, nCurrentConnState );
		}
		_mapRedisConnPoolStatus[Index]._nReportIndex = 0;
	}
}

template< class T >
void CRedisConnectPoolFactory_Queue<T>::run()
{
    while( 1 )
    {
        sleep( _nKeepAliveTime );		
		// ??????????????????
        for( typename map< T, queue< CRedisConnectItem* > >::iterator mIt = _mapConnPool.begin();
                mIt != _mapConnPool.end(); ++ mIt )
        {
			int nKeepAliveCnt	= mIt->second.size(), nCount  = 0;
			T Index				= mIt->first;
			std::string sTableName = TC_Common::replace( Index, ".", "" );
			MYBOOST_LOG_INFO( "ReportPlusGauge Succ." << OS_KV( "tablename", sTableName )
					<< OS_KV( "cursize", _mapRedisConnPoolStatus[Index]._nCurrSize )
					<< OS_KV( "keepalivecnt", nKeepAliveCnt ) );
			RedisProxyPrometheus::getInstance()->ReportPlusGauge( "RedisConnPoolGauge", sTableName+":currsize", 
					_mapRedisConnPoolStatus[Index]._nCurrSize );
			// ????????????????????????????????????????????????
			int nTableStartTime = TNOWMS;
			while( !mIt->second.empty() && nCount < nKeepAliveCnt )
            {
				nCount ++;
				int nStartTime = TNOWMS;
				CRedisConnectItem * pRedisItem = NULL;
				{
					taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[mIt->first] );
					if( mIt->second.empty() ) break;
					pRedisItem = mIt->second.front();
					mIt->second.pop();
				}
				if( DealIdleConnect( pRedisItem, Index ) )
				{
					continue;
				}
				else 
				{
					if( !pRedisItem->IsKeepAliveV1() )
					{
						ResetConnect( pRedisItem, Index );
					}
					else
					{
                        // ??????????????????????????????????????????
                        taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[mIt->first] );
                        _mapConnPool[Index].push( pRedisItem );
					}
				}
				int nEndTime = TNOWMS;
				MYBOOST_LOG_INFO( "OnceKeepAlive " << OS_KV( "cost", nEndTime - nStartTime ) 
						<< OS_KV( "count", nCount ) << OS_KV( "size", mIt->second.size() ) );
            }
			int nTableEndTime = TNOWMS;
			MYBOOST_LOG_INFO( "TableKeepAlive " << OS_KV( "cost", nTableEndTime - nTableStartTime ) 
						<< OS_KV( "Index", Index )
						<< OS_KV( "count", nCount ) << OS_KV( "keepalivecnt", nKeepAliveCnt ) );
		}
		// ?????????????????????
        for( typename map< T, queue< CRedisConnectItem* > >::iterator mIt = _mapUnHealthConnPool.begin();
                mIt != _mapUnHealthConnPool.end(); ++ mIt )
		{
			T Index					= mIt->first;
			std::string sTableName	= TC_Common::replace( Index, ".", "" );
			RedisProxyPrometheus::getInstance()->ReportPlusGauge( "RedisConnPoolGauge", sTableName+":unhealth", 
					mIt->second.size() );
			while( !mIt->second.empty() )
			{
				CRedisConnectItem * pRedisItem = mIt->second.front();
				mIt->second.pop();
				if( !pRedisItem->IsKeepAliveV1() )
				{
					ResetConnect( pRedisItem, Index ); 
				}
				else
				{
					// ??????????????????????????????????????????
					taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[mIt->first] );
					_mapConnPool[Index].push( pRedisItem );
				}
			}
		}
		// ??????????????????????????????
        for( typename map< T, queue< CRedisConnectItem* > >::iterator mIt = _mapReportDetectConnPool.begin();
                mIt != _mapReportDetectConnPool.end(); ++ mIt )
		{
			// ?????????????????????????????????????????????
			// ??????????????????????????????????????????
			// ????????????redis??????????????????????????????????????????????????????????????????
			// ????????????redis????????????????????????????????????????????????????????????????????????
			int nReportDetectCnt	= 0;
			T Index					= mIt->first;
			while( !mIt->second.empty() 
					&& nReportDetectCnt < MAX_REPORT_DETECT_CONN
					&& nReportDetectCnt < mIt->second.size() )
			{
				bool bIsPfail		= false;
				CRedisConnectItem * pRedisItem = mIt->second.front();
				mIt->second.pop();
				if( !pRedisItem->IsKeepAliveV1() )
				{
					if( !ResetReportDetectConnect( pRedisItem, Index ) )
					{
						bIsPfail = true;
					}
				}
				else
				{
					_mapReportDetectConnPool[Index].push( pRedisItem );
				}
				ReportDetectState( bIsPfail, Index );
				nReportDetectCnt ++;
			}
			if( mIt->second.empty() )
			{
				if( !ResetReportDetectConnect( NULL, Index) )
				{
					ReportDetectState( true, Index );
				}
			}
		}
	}
}

template< class T >
void CRedisConnectPoolFactory_Queue<T>::DestoryConnect( CRedisConnectItem * pRedisItem, T Index )
{
    taf::TC_LockT<taf::TC_ThreadRecMutex> lockGaurd( _mapLock[Index] );
    if( pRedisItem != NULL )
    {
		pRedisItem->DestoryConnectItem();
	}
}

template< class T >
CRedisConnectPoolFactory_Queue<T>::CRedisConnectItem::CRedisConnectItem( const RedisConf & oRedisConf ):
		_bUsed( false ), _nUsedCnt(0), _bHealth( true )
{
	// ??????redis??????????????????????????????_redisConn
	_oRedisConf = oRedisConf;
}

template< class T >
CRedisConnectPoolFactory_Queue<T>::CRedisConnectItem::~CRedisConnectItem()
{
	this->DestoryConnectItem();
}

// ?????????????????????????????????????????????????????? 
template< class T >
int CRedisConnectPoolFactory_Queue<T>::CRedisConnectItem::CreateConnectItem()
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
			nRet = -2;
            MYBOOST_LOG_ERROR( "RedisConnect Error." 
                    << OS_KV( "errstr", _pRedisConn->errstr )
                    << OS_KV( "rediserr", _pRedisConn->err )
                    << OS_KV( "ret", nRet ) );
			redisFree( _pRedisConn );
		}
		else
		{
			redisSetTimeout( _pRedisConn, _oRedisConf._nTimeout );
			if( !_oRedisConf._sPasswd.empty() )
			{
				string sAuthCmd = "AUTH " + _oRedisConf._sPasswd;
				redisAppendCommand( _pRedisConn, sAuthCmd.c_str() );	// ????????????
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
bool CRedisConnectPoolFactory_Queue<T>::CRedisConnectItem::IsKeepAliveV1()
{
	// ????????????????????????Redis????????????????????????????????????????????????????????? PING ????????????????????????INFO?????????Redis???????????? 
	// 1 ??????????????????????????????????????????????????????????????????
	// 2 ?????????????????????????????????????????????
	// 3 ??????????????????????????????????????????????????????
	// 4 ?????????????????????????????????????????????????????????????????????
	// 5 ???????????????????????????????????????????????????????????????????????????
	// 6 ??????????????????????????????????????????
	// 7 ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
	redisReply * pReply;
	int nTry = 0;
	bool bIsKeepAlive = false;
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
void CRedisConnectPoolFactory_Queue<T>::CRedisConnectItem::DestoryConnectItem()
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
void CRedisConnectPoolFactory_Queue<T>::CRedisConnectItem::RecoverConnectItem()
{
    _bUsed = false;
}

template< class T >
void CRedisConnectPoolFactory_Queue<T>::CRedisConnectItem::SetConnectHealth( bool bHealthRedisConn )
{
    _bHealth = bHealthRedisConn;
}

template class CRedisConnectPoolFactory_Queue<string>; // ??????????????????string??????
