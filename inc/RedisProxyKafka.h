/*************************************************************************
    > File Name: RedisProxyKafka.h
    > Author: annulsluo
    > Mail: annulsluo@gmail.com
    > Created Time: 六  4/ 4 18:38:52 2020
 ************************************************************************/
#ifndef _RedisProxyKafka_H_
#define _RedisProxyKafka_H_
#include "util/tc_singleton.h"
#include "ServerDefine.h"
#include "ServerConfig.h"
#include "RedisProxyCommon.pb.h"
#include "rdkafka.h"
#include "rdkafkacpp.h"
#include "util/myboost_log.h"
#include "RedisProxyPrometheus.h"

class RedisProxyDeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    void dr_cb (RdKafka::Message &message) {
		string sProperty	= message.topic_name() + "_" + TC_Common::tostr( message.partition() );
		if( message.err())
		{
			RedisProxyPrometheus::getInstance()->ReportPlus( "RedisProxyKafkaProducer", sProperty + ":RspFail" );
			MYBOOST_LOG_ERROR( "Message delivery failed." 
                     << OS_KV( "msg err", message.errstr() ) );
		}
		else
		{
			RedisProxyPrometheus::getInstance()->ReportPlus( "RedisProxyKafkaProducer", sProperty + ":RspSucc" );
			MYBOOST_LOG_INFO( "Message delivery Succ." 
						<< OS_KV( "len",message.len() ) 
						<< OS_KV( "partition",message.partition() ) 
						<< OS_KV( "offset",message.offset() ) 
						<< OS_KV( "topic", message.topic_name() ) );
		}
    }
};

class RedisProxyEventCb : public RdKafka::EventCb {
public:
    void event_cb (RdKafka::Event &event) {
        switch (event.type())
        {
			case RdKafka::Event::EVENT_ERROR:
				MYBOOST_LOG_ERROR( OS_KV( "err", RdKafka::err2str(event.err()) ) << event.str() );
				if (event.err() == RdKafka::ERR__ALL_BROKERS_DOWN)
				{
					// donothing
				}
				break;
	 
			case RdKafka::Event::EVENT_STATS:
				MYBOOST_LOG_DEBUG( OS_KV( "stats", event.str() ) );
				break;
	 
			case RdKafka::Event::EVENT_LOG:
				MYBOOST_LOG_DEBUG( OS_KV( "log_serverity", event.severity() ) 
						<< OS_KV( "fac", event.fac() ) << OS_KV( "event", event.str() ) );
				break;
	 
			default:
				MYBOOST_LOG_DEBUG( OS_KV( "event_type", event.type() ) << OS_KV( "event", event.str() ) );
				break;
			}
    }
};
class RedisProxyConsumeCb : public RdKafka::ConsumeCb {
public:
    void consume_cb ( RdKafka::Message & oMsg, void *pOpaque ) {
		//RedisProxyKafka::Consumer( &oMsg, pOpaque );
    }
};

class RedisProxyKafka:
	public taf::TC_Singleton< RedisProxyKafka >,
	public taf::TC_Thread
{
	public:
		RedisProxyKafka();
		~RedisProxyKafka();

		void DestoryResource();
		void InitProduce( KafkaInfo & oKafkaInfo );
		void InitConsumer( KafkaInfo & oKafkaInfo );
		
		void InsertFailOver( RedisProxyCommon::FailOverInfo & oFailOverInfo );
		// 初始化两个topic(正常topic和异常topic，前者提供给LMDB消费，后者提供给守护进程消费)
		// 对于异常topic创建一定数量的分区，用于提供给异常的实例生产写数据
		void UpdateFailOverTopic( MapStr2FailOverInfo & mapHost2FailOverInfo );
		int Produce( string sTopicName, char * pczMsg, 
                size_t nMsgLen, string sHost = "", string sPartitionKey = "" );
		int GetIdlePartition();
		int Consumer( RdKafka::Message * pMessage, std::string & sData );

	private:
		void run();

	private:
		int _nLmdbPartition;
		bool _bIsKeepRunning;
		RedisProxyEventCb _oExEventCb;
		RedisProxyDeliveryReportCb _oExDrCb;
		RedisProxyConsumeCb _oExConsumCb;
		string _sBrokers, _sTopics, _sPartitions;
		PartitionInfo _aPartition[ MAX_PARTITION_LEN ];
		map< string, int >_mapHost2Partition;
		RdKafka::Producer * _pProducer;
		RdKafka::Consumer * _pConsumer;
		RdKafka::Conf	  * _pGConf, * _pTConf;
		
		map< string, RdKafka::Topic * >_mapTopic;
		//map< int, RedisProxyCommon::FailOverInfo >_mapPartition2FailOverInfo;
		MapInt2FailOverInfo _mapPartition2FailOverInfo;
};
#endif 


