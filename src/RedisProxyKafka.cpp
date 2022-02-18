/*************************************************************************
    > File Name: RedisProxyKafka.cpp
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 六  4/ 4 18:39:00 2020
 ************************************************************************/
#include "RedisProxyKafka.h"
#include "RedisProxyFailOver.h"
#include "RedisProxyBackEnd.h"
#include "RedisProxyZkData.h"
#include "RedisProxyPrometheus.h"
#include "AICommon.pb.h"

RedisProxyKafka::RedisProxyKafka()
{
	_bIsKeepRunning = false;
	_pProducer		= NULL;
	_pConsumer		= NULL;
	_pGConf			= NULL;
	_pTConf			= NULL;
}

RedisProxyKafka::~RedisProxyKafka()
{
	DestoryResource();
}

void RedisProxyKafka::DestoryResource()
{
	 for( map<string, RdKafka::Topic *>::iterator mit = _mapTopic.begin();
            mit != _mapTopic.end(); ++ mit )
    {   
        if( mit->second ){
            delete mit->second;
            _mapTopic.erase( mit );
        }   
    }   
    if( _pProducer ) delete _pProducer;
    if( _pConsumer ) delete _pConsumer;
    if( _pGConf ) delete _pGConf;
    if( _pTConf ) delete _pTConf;
}

void RedisProxyKafka::InitProduce( KafkaInfo & oKafkaInfo )
{
	_sBrokers		= oKafkaInfo.sBrokers;
	_sTopics		= oKafkaInfo.sTopics;
	_nLmdbPartition	= oKafkaInfo.nLmdbPartition;
	// 注意在初始化的时候，要把相关的信息进行删除，否则在发送消息的时候没有回调结果
	// 有空查下源码的原因是什么
	DestoryResource();

	string sErrStr  = "";
    _pGConf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    _pTConf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);

    _pGConf->set( "bootstrap.servers", _sBrokers, sErrStr );

    if( _pGConf->set( "event_cb", &_oExEventCb, sErrStr ) != RdKafka::Conf::CONF_OK )
	{
		MYBOOST_LOG_ERROR( "Failed to set event_cb." << OS_KV( "sErrStr", sErrStr ) );
	}

    if( _pGConf->set("dr_cb", &_oExDrCb, sErrStr) != RdKafka::Conf::CONF_OK )
	{
		MYBOOST_LOG_ERROR( "Failed to set dr_cb." << OS_KV( "sErrStr", sErrStr ) );
	}
	
    _pProducer = RdKafka::Producer::create(_pGConf, sErrStr );
    if (!_pProducer) {
		MYBOOST_LOG_ERROR( "Failed to create producer." << OS_KV( "sErrStr", sErrStr ) );
    }

	vector< string >vecTopic = TC_Common::sepstr< string >( _sTopics, "," );
	for( size_t i = 0; i < vecTopic.size(); ++ i )
	{
		RdKafka::Topic * pTmpTopic	= RdKafka::Topic::create( _pProducer, vecTopic[i], _pTConf, sErrStr );
		_mapTopic.insert( std::pair< std::string, RdKafka::Topic * >( vecTopic[i], pTmpTopic ) );
		if ( !pTmpTopic ) 
		{
			MYBOOST_LOG_ERROR( "Failed to create." << OS_KV( "topic", vecTopic[i] ) << OS_KV( "err", sErrStr ) );
			continue;
		}
		MYBOOST_LOG_INFO( "Init Kafka Succ." << OS_KV( "topic", vecTopic[i] ) );
	}

	if( oKafkaInfo.bIsKeepRun && !_bIsKeepRunning )
	{
		this->start();
		_bIsKeepRunning = true;
	}
}

void RedisProxyKafka::InitConsumer( KafkaInfo & oKafkaInfo )
{
	_sBrokers		= oKafkaInfo.sBrokers;
	_sTopics		= oKafkaInfo.sTopics;
	_sPartitions	= oKafkaInfo.sPartitions;

	DestoryResource();

	vector< int > vecPartition = TC_Common::sepstr< int >( _sPartitions, "," );
	for( size_t i = 0; i < vecPartition.size(); ++ i )
	{
		_aPartition[i].nNum		= vecPartition[i];
		_aPartition[i].bUsed	= false;
	}

    string sErrStr  = "";

    _pGConf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    _pTConf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);

    _pGConf->set( "bootstrap.servers", _sBrokers, sErrStr );

    if( _pGConf->set( "consume_cb", &_oExConsumCb, sErrStr ) != RdKafka::Conf::CONF_OK )
	{
		MYBOOST_LOG_ERROR( "Failed to set consume_cb." << OS_KV( "sErrStr", sErrStr ) );
	}

    if( _pGConf->set( "event_cb", &_oExEventCb, sErrStr ) != RdKafka::Conf::CONF_OK )
	{
		MYBOOST_LOG_ERROR( "Failed to set event_cb." << OS_KV( "sErrStr", sErrStr ) );
	}

	_pGConf->set( "default_topic_conf", _pTConf, sErrStr );

    _pConsumer = RdKafka::Consumer::create( _pGConf, sErrStr );
    if (!_pConsumer) {
		MYBOOST_LOG_ERROR( "Failed to create Consumer." << OS_KV( "sErrStr", sErrStr ) );
    }

    vector< string >vecTopic = TC_Common::sepstr< string >( _sTopics, "," );
	for( size_t i = 0; i < vecTopic.size(); ++ i )
	{
		RdKafka::Topic * pTmpTopic	= RdKafka::Topic::create( _pConsumer, vecTopic[i], _pTConf, sErrStr );
		_mapTopic.insert( std::pair< std::string, RdKafka::Topic * >( vecTopic[i], pTmpTopic ) );
		if ( !pTmpTopic ) 
		{
			MYBOOST_LOG_ERROR( "Failed to create." << OS_KV( "topic", vecTopic[i] ) << OS_KV( "err", sErrStr ) );
			continue;
		}
		MYBOOST_LOG_INFO( "Init Kafka Topic Succ." << OS_KV( "topic", vecTopic[i] ) );
	}

    for( size_t j = 0; j < MAX_PARTITION_LEN ; ++ j )
    {
        //if( _aPartition[j].bUsed )
        {
            RdKafka::ErrorCode	nErrorCode	
                = _pConsumer->start( _mapTopic["failtopic"], _aPartition[j].nNum, RdKafka::Topic::OFFSET_END );
            if( nErrorCode != RdKafka::ERR_NO_ERROR )
            {
                MYBOOST_LOG_ERROR( "Failed to assign Consumer." << OS_KV( "sErrStr", RdKafka::err2str( nErrorCode ) ) );
            }
            else
            {
                MYBOOST_LOG_INFO( "Init KafkaConsumer failtopic Succ." << OS_KV( "partition", _aPartition[j].nNum )
                        << OS_KV( "offset", _aPartition[j].nOffset ) );
            }
        }
    }

	if( oKafkaInfo.bIsKeepRun && !_bIsKeepRunning )
	{
		this->start();
		_bIsKeepRunning = true;
	}
}

void RedisProxyKafka::InsertFailOver( RedisProxyCommon::FailOverInfo & oFailOverInfo )
{
	int nPartition						= oFailOverInfo.npartition();
	MapInt2FailOverInfo::iterator mIt	= _mapPartition2FailOverInfo.find( nPartition );
	if( mIt == _mapPartition2FailOverInfo.end() )
	{
		_mapPartition2FailOverInfo.insert( 
				std::pair< int, RedisProxyCommon::FailOverInfo >( nPartition, oFailOverInfo ) ) ;
	}
	else
	{
		mIt->second = oFailOverInfo;
	}
}

// Proxy从Zk感知节点下线/上线要进行恢复的操作
// Guard 自身设置节点上下线操作
void RedisProxyKafka::UpdateFailOverTopic( MapStr2FailOverInfo & mapHost2FailOverInfo )
{
	// 需要把下线的机器跟partition绑定对应起来
	for( MapStr2FailOverInfo::iterator msfi = mapHost2FailOverInfo.begin();
			msfi != mapHost2FailOverInfo.end(); ++ msfi )
	{
		MapStr2Int::iterator ms2i		= _mapHost2Partition.find( msfi->first );	
		MYBOOST_LOG_DEBUG( OS_KV( "host", msfi->first ) << OS_KV( "size", _mapHost2Partition.size() ) );
		if( ms2i != _mapHost2Partition.end() )
		{
			continue;
		}
		else
		{
			if( !_aPartition[msfi->second.npartition()].bUsed )
			{
				_mapHost2Partition.insert( std::pair< string, int >( msfi->first, msfi->second.npartition() ) );
				_aPartition[msfi->second.npartition()].bUsed = true;
				_mapPartition2FailOverInfo.insert( 
						std::pair< int, RedisProxyCommon::FailOverInfo >( msfi->second.npartition(), msfi->second ) );
			}
		}
	}

    // 把已经上线的节点从故障集合中剔除
    for( MapInt2FailOverInfo::iterator mIt = _mapPartition2FailOverInfo.begin(); 
            mIt != _mapPartition2FailOverInfo.end(); ++ mIt )
    {
        string sHost = mIt->second.shost();
        if( mapHost2FailOverInfo.find( sHost ) == mapHost2FailOverInfo.end() )
        {
            _mapPartition2FailOverInfo.erase( mIt );
            _aPartition[mIt->first].bUsed   = false;
            MapStr2Int::iterator ms2it      = _mapHost2Partition.find( sHost ); 
            if( ms2it != _mapHost2Partition.end() )
            {
                _mapHost2Partition.erase( ms2it );
            }
        }
    }
}

int RedisProxyKafka::GetIdlePartition()
{
	for( size_t i = 0; i < MAX_PARTITION_LEN; ++ i )
	{
		if( !_aPartition[i].bUsed ) return _aPartition[i].nNum;
	}
	return PARTITION_ERR;
}

int RedisProxyKafka::Produce( string sTopicName, char * pczMsg, size_t nMsgLen, string sHost, string sPartitionKey )
{
	// 跟Host计算对应的Partition，同时不存在的实例>10个，可能会造成数据丢失
	int32_t nPartition = RdKafka::Topic::PARTITION_UA;
	if( !sHost.empty() ) 
	{
		map< string, int >::iterator msi = _mapHost2Partition.find( sHost );
		if( msi != _mapHost2Partition.end() )
		{
			nPartition = _mapHost2Partition[sHost];
		}
		else
		{
			MYBOOST_LOG_ERROR( "Produce fail.There isn't failtopic Partition." );
			return AICommon::RET_FAIL;
		}
	}

	// 需要保证同一个Key的消息落到同一个 partition 中，考虑以下两种方式哪种更均衡
	// 1. 根据 crc 传值对应的 slot % _nLmdbPartition，crc值是由Key计算得到的，因为会存在批量的情况，所以可能会不一样；
	// 2. 把 AppTable 作为 messages.key 由kafka进行自动分配
	// 答：考虑2的方式，由 Kafka 生产通过murmur2 算法生成Hash值取mod
	// 3. 消费者并发消费时，保证单个进程/线程消费同一个分区
	int nCnt = 0, nRet = AICommon::RET_SUCC;
	while( nCnt < MAX_TRY_TIMES )
	{
		RdKafka::ErrorCode resp;
		if( !sPartitionKey.empty() )
		{
            std::string * _pKey = new string( sPartitionKey );
			resp = _pProducer->produce( _mapTopic[sTopicName], nPartition, 
					RdKafka::Producer::RK_MSG_COPY, pczMsg, nMsgLen, _pKey, NULL );
            if( _pKey ) delete( _pKey );
		}
		else
		{
			resp = _pProducer->produce( _mapTopic[sTopicName], nPartition, 
					RdKafka::Producer::RK_MSG_COPY, pczMsg, nMsgLen, NULL, NULL );
		}
		if ( resp == RdKafka::ERR_NO_ERROR )
		{
			MYBOOST_LOG_INFO( "Produce succ." 
                    << OS_KV( "topic", sTopicName ) << OS_KV( "partitionkey", sPartitionKey )
                    << OS_KV( "host", sHost ) << OS_KV( "partition", nPartition ) );
			RedisProxyPrometheus::getInstance()->ReportPlus( "RedisProxyKafkaProducer", sTopicName + ":" + 
		    			TC_Common::tostr( nPartition ) );
			break;
		}
		else if ( resp == RdKafka::ERR__QUEUE_FULL )
		{
			_pProducer->poll(0);
		}
		else 
		{
			MYBOOST_LOG_ERROR( "Produce fail." << OS_KV( "err", RdKafka::err2str( resp ) ) );
			nRet = AICommon::RET_FAIL;
		}
		nCnt ++;
	}
	return nRet;
}

int RedisProxyKafka::Consumer( RdKafka::Message* pMessage, std::string & sData )
{
    sData.clear();
    sData = "";
	int nRet = pMessage->err();
	switch (nRet) {
    case RdKafka::ERR__TIMED_OUT:
		MYBOOST_LOG_ERROR( "Consumer Timeout." );
        break;
 
    case RdKafka::ERR_NO_ERROR:
        /* Real message */
        //int64_t nMsgLen = pMessage->len();
		MYBOOST_LOG_DEBUG( "Consumer Read Msg At " << OS_KV( "offset", pMessage->offset() ) );
        sData.assign( static_cast<const char*>( pMessage->payload() ), static_cast<int>(pMessage->len()) );
        RdKafka::MessageTimestamp ts;
        ts = pMessage->timestamp();
        if ( ts.type != RdKafka::MessageTimestamp::MSG_TIMESTAMP_NOT_AVAILABLE) {
            std::string tsname = "?";
            if (ts.type == RdKafka::MessageTimestamp::MSG_TIMESTAMP_CREATE_TIME)
                tsname = "create time";
            else if (ts.type == RdKafka::MessageTimestamp::MSG_TIMESTAMP_LOG_APPEND_TIME)
                tsname = "log append time";
			MYBOOST_LOG_DEBUG( OS_KV( "timestamp name", tsname ) << OS_KV( "timestamp", ts.timestamp ) );
        }
        if ( pMessage->key() ) {
			MYBOOST_LOG_DEBUG( OS_KV( "key", *pMessage->key() ) );
        }
		MYBOOST_LOG_DEBUG( "Message Info. " 
				<< OS_KV( "len", static_cast<int>(pMessage->len() ) )
				<< OS_KV( "payload", static_cast<const char *>(pMessage->payload() ) )
				<< OS_KV( "topic", pMessage->topic()->name() )
				<< OS_KV( "partition", pMessage->partition() )
				<< OS_KV( "offset", pMessage->offset() ));
        break;
 
    case RdKafka::ERR__PARTITION_EOF:
        /* Last message */
		MYBOOST_LOG_DEBUG( "Consumer EOF reached for all." );
        break;
 
    case RdKafka::ERR__UNKNOWN_TOPIC:
    case RdKafka::ERR__UNKNOWN_PARTITION:
		MYBOOST_LOG_ERROR( "Consumer " << OS_KV( "failed", pMessage->errstr() ) );
        break;
 
    default:
        /* Errors */
		MYBOOST_LOG_ERROR( "Consumer " << OS_KV( "failed", pMessage->errstr() ) );
    }
    return nRet;
}

void RedisProxyKafka::run()
{
	while( 1 )
	{
		if( _pProducer != NULL && _pProducer->outq_len() > 0 )
		{
			_pProducer->poll( 1 );
			MYBOOST_LOG_DEBUG( "Producer Begin." );
		}

		if( _pConsumer != NULL )
		{
			// 1. 启动时正常，运行过程中发现节点下线；2. 启动时发现已经有节点下线
            // 对于下线的节点按照先下线先消费方式进行处理
            // 优点：早下线的节点能快速恢复
            // 缺点：有可能导致迟下线的节点因等待早下线节点恢复而饿死
			MYBOOST_LOG_DEBUG( "Consumer Begin." );
            for( size_t j = 0; j < MAX_PARTITION_LEN; ++ j )
            {
                if( _aPartition[j].bUsed )
                {
                    while(1)
                    {
                        // 0. 判断该Host 是否已经处于恢复阶段
                        // 1. 根据Partition 得到 Host
                        // 2. 根据Host 得到 GroupId
                        // 3. 调用 RedisProxyBack.DoPipeline接口追加数据到redis
                        // 4. 如果消费完成(进度追加完成)，则更新该节点为正常上线节点，
						//		并且设置删除Kafka中该节点的FailOverInfo
						MYBOOST_LOG_DEBUG( "Consumer Prepared." << OS_KV( "partition", _aPartition[j].nNum ) );
                        MapInt2FailOverInfo::iterator mIt = 
                                _mapPartition2FailOverInfo.find( _aPartition[j].nNum );
                        if( mIt != _mapPartition2FailOverInfo.end() )
                        {
                            int nGroupId            = mIt->second.ngroupid();
                            std::string sAppTable   = mIt->second.sapptable();
							MYBOOST_LOG_DEBUG( "Consumer Prepareing." 
									<< OS_KV( "apptable", sAppTable )
									<< OS_KV( "groupid", nGroupId ) );
                            if( !RedisProxyFailOver::getInstance()->IsGroupFail( sAppTable, nGroupId ) )
                            {
                                RdKafka::Message * pMsg	= _pConsumer->consume( 
                                        _mapTopic["failtopic"], _aPartition[j].nNum, 1000 );
                                std::string sData;
                                int nRet = Consumer( pMsg, sData );
                                delete pMsg;
                                if( nRet == RdKafka::ERR__PARTITION_EOF || nRet == RdKafka::ERR__TIMED_OUT )
                                {
                                    MYBOOST_LOG_DEBUG( "Consumer Finish." 
											<< OS_KV( "host", mIt->second.shost() )
											<< OS_KV( "partition", _aPartition[j].nNum ) );
                                    // 消费完成
                                    _aPartition[j].bUsed    = false;
                                    string sHost            = mIt->second.shost();
                                    _mapPartition2FailOverInfo.erase( mIt );
									MapStr2Int::iterator mSit = _mapHost2Partition.find( sHost );
									if( mSit != _mapHost2Partition.end() ) _mapHost2Partition.erase( mSit );
                                    RedisProxyZkData::getInstance()->DelFailOver( sAppTable, sHost );
									break;
                                }
                                else if( nRet == RdKafka::ERR_NO_ERROR )
                                {
                                    PipeLineRequest oPipeLineReq;
                                    oPipeLineReq.ParseFromArray( sData.c_str(), sData.size() );
                                    MYBOOST_LOG_DEBUG( "Consumer Msg Parse." 
                                            << OS_KV( "pipelinereq", oPipeLineReq.DebugString() ));
                                    
                                    string sMsgid       = oPipeLineReq.smsgid();
                                    PipeLineResponse	oPipeLineRsp;
                                    RedisProxyBackEnd	oRedisProxyBackEnd( 
                                            sMsgid, nGroupId, sAppTable, oPipeLineReq );
                                    int nRet = oRedisProxyBackEnd.DoPipeLine( oPipeLineRsp );
                                    if( nRet == AICommon::RET_SUCC )
                                    {
                                        MYBOOST_LOG_DEBUG( OS_KV( "msgid", sMsgid ) 
                                                << OS_KV( "Dopipeline Succ. ret", nRet ) 
                                                << OS_KV( "PipeLineRsp", oPipeLineRsp.DebugString() ) ); 
                                    }
                                    else
                                    {
                                        MYBOOST_LOG_ERROR(  OS_KV("msgid", sMsgid ) 
                                                << OS_KV( "Dopipeline Error. ret", nRet ) ); 
                                    }
                                }
                            }
							else
							{
								break;
							}
                        }
                    }
                }
            }
		}
		sleep(10);
	}
}
