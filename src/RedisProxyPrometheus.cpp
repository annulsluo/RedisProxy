/*************************************************************************
    > File Name: RedisProxyPrometheus.h
    > Author: annulsluo
    > Mail: annulsluo@gmail.com
    > Created Time: 日  3/ 1 20:58:29 2020
 ************************************************************************/

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include "RedisProxyPrometheus.h"
#include <prometheus/registry.h>
#include "AICommon.pb.h"
#include "util/myboost_log.h"

using namespace prometheus;

struct FamilyItem
{
	std::string sFamily;
	std::string sHelp;
	std::string sLabel;
};

static FamilyItem Familys[] =
{
	// add Famliy(mertric help labelpair)
    {"RedisProxyMonitor", "RedisProxyMonitor Status", ""},
    {"RedisProxyAppReq", "AppId Req Feq", ""},					//  业务侧请求频率次数
    {"RedisProxyPipeline", "AppId Pipeline Feq", ""},			//  业务侧pipeline频率次数
    {"RedisConnPoolCounter", "ConnPool Status", ""},			//  统计链接池异常状态
    {"RedisProxyClientCounter", "Client Status", ""},			//  统计客户端请求频次
    {"RedisProxyKafkaProducer", "KafkaProducer Status", ""},	//  统计Kafka生产者生产信息
    {"DmplmdbAgent", "DmplmdbInterfaceAgent Status", ""},		//  统计lmdb接口情况
    {"RedisProxyCanCell", "Client CanCell", ""},			    //  统计客户端超时
    {"HandleSlotMigrateTask", "SlotMigrate Result", ""},        //  统计迁移时，对应结果状态数据
};

struct LabelItem
{
    string sLabelItem;
};

static LabelItem Labels[] = 
{
    // add Counter here
	// {labelname:labelvalue}
    {"Property:Req"},
    {"Property:Rsp"},
    {"Property:Fail"},
	{"Property:IsCancelled"},
	{"Property:PipeLineSucc"},
	{"Property:PipeLineError"},
	{"Property:LmdbError"},
	{"InvalidCase:InvalidParam"},
	{"InvalidCase:InvalidAuth"},
	{"InvalidCase:InFlowControl"},
	{"Timeout:DoCmdTimeout"},
	{"Timeout:MaxDoReqTimeout"},
	{"Timeout:GetRedisTimeout"},
	{"Timeout:KeepAlivePingTimeout"},
	{"Deallatency:totallatency"},
	{"Deallatency:unknow_cost"},
	{"Deallatency:0"},
	{"Deallatency:(0,1]"},
	{"Deallatency:(1,2]"},
	{"Deallatency:(2,5]"},
	{"Deallatency:(5,10]"},
	{"Deallatency:(10,12]"},
	{"Deallatency:(12,15]"},
	{"Deallatency:(15,20]"},
	{"Deallatency:(20,+&)"},
	{"Deallatency:(20,100]"},
	{"Deallatency:(100,200]"},
	{"Deallatency:(200,500]"},
	{"Deallatency:(500,+&)"},
	
};

static LabelItem GaugeLabels[] = 
{
    // add Gauge here
	//{"Latency"},
};

static FamilyItem GaugeFamilys[]=
{
    {"RedisConnPoolGauge", "ConnPool Status", "" },			//  统计链接池异常状态
};

string RedisProxyPrometheus::GetHostName()
{
	char hostname[1024];
	if (::gethostname(hostname, sizeof(hostname))) {
		return {};
	}
	return hostname;
}

void RedisProxyPrometheus::InitCounterFamily()
{
	_pCounterRegistry		= std::make_shared<Registry>();
	// 创建多个counter
	auto & _oCounterFamily	= BuildCounter()
		.Name( "RedisProxyMonitor" )
		.Help( "RedisProxyMonitor Status" )
		.Register( *_pCounterRegistry );

	for( size_t i = 0; i < sizeof( Labels ) / sizeof( LabelItem ); ++ i )
	{
		vector<string> LabelPair = TC_Common::sepstr< std::string >( Labels[i].sLabelItem, ":" );
		_mapCounter.insert( std::pair< string, Counter * >
				( Labels[i].sLabelItem, &_oCounterFamily.Add( {{ LabelPair[0], LabelPair[1] }} )));
	}

	for( size_t i = 0; i < sizeof( Familys ) / sizeof( FamilyItem ); ++ i )
	{
		auto & oFamily = BuildCounter()
			.Name(Familys[i].sFamily).
			Help( Familys[i].sHelp ).
			Register(*_pCounterRegistry);

		_mapFamily.insert( std::pair< std::string, Family<Counter>* >( Familys[i].sFamily, &oFamily ) );
		if( !Familys[i].sLabel.empty() )
		{
			std::string sFamilyLabel = Familys[i].sFamily + ":" + Familys[i].sLabel;
			vector<string> LabelPair = TC_Common::sepstr< std::string >( Familys[i].sLabel, ":" );
			_mapFamilyCounter.insert( std::pair< string, Counter * >
					( sFamilyLabel, &oFamily.Add( {{ LabelPair[0], LabelPair[1] }} )));
		}
	}
}

void RedisProxyPrometheus::InitGaugeFamily()
{
	_pGaugeRegistry			= std::make_shared<Registry>();
	auto & _oGaugeFamily	= prometheus::BuildGauge()
								.Name( "RedisProxyGauge" )
								.Help( "RedisProxyGauge Status." )
								.Register( *_pGaugeRegistry );

	for( size_t i = 0; i < sizeof( GaugeLabels ) / sizeof( LabelItem ); ++ i )
	{
		_mapGauge.insert( std::pair< string, Gauge* >
				( GaugeLabels[i].sLabelItem, &_oGaugeFamily.Add( {{ _sHostName, GaugeLabels[i].sLabelItem }} )));
	}

	for( size_t i = 0; i < sizeof( GaugeFamilys ) / sizeof( FamilyItem ); ++ i )
	{
		MYBOOST_LOG_INFO( "GaugeRegistry " << OS_KV( "family", GaugeFamilys[i].sFamily) 
				<< OS_KV( "help", GaugeFamilys[i].sHelp ));
		auto & oFamily = BuildGauge()
			.Name(GaugeFamilys[i].sFamily).
			Help( GaugeFamilys[i].sHelp ).
			Register(*_pGaugeRegistry);

		_mapGaugeFamily.insert( std::pair< std::string, Family<Gauge>* >( GaugeFamilys[i].sFamily, &oFamily ) );
		if( !GaugeFamilys[i].sLabel.empty() )
		{
			std::string sFamilyLabel = GaugeFamilys[i].sFamily + ":" + GaugeFamilys[i].sLabel;
			vector<string> LabelPair = TC_Common::sepstr< std::string >( GaugeFamilys[i].sLabel, ":" );
			_mapFamilyGauge.insert( std::pair< string, Gauge * >
					( sFamilyLabel, &oFamily.Add( {{ LabelPair[0], LabelPair[1] }} )));
		}
	}
}

int RedisProxyPrometheus::Init( string sIp, string sPort )
{
	if( sIp.empty() && sPort.empty() )
		_sHostName = GetHostName();
	else
	{
		_sHostName = sIp + ":" + sPort;
	}

	_sPrometheusGateWayHost	= ServerConfigMng::getInstance()->GetSObj( "PrometheusGateWayHost" );
	_sPrometheusGateWayPort	= ServerConfigMng::getInstance()->GetSObj( "PrometheusGateWayPort" );
	_sPrometheusGateWayJob	= ServerConfigMng::getInstance()->GetSObj( "PrometheusGateWayJob" );
	_nPrometheusPushTime	= ServerConfigMng::getInstance()->GetIObj( "PrometheusPushTime" );

	InitCounterFamily();
	InitGaugeFamily();

	MYBOOST_LOG_INFO( "RedisProxyPrometheus Init Succ." );

	this->start();

	return  AICommon::RET_SUCC;
}

void RedisProxyPrometheus::run()
{
	const auto labels = Gateway::GetInstanceLabel( _sHostName );
	Gateway oGateWay = Gateway( _sPrometheusGateWayHost, _sPrometheusGateWayPort, _sPrometheusGateWayJob, labels );
	oGateWay.RegisterCollectable( _pCounterRegistry );
	oGateWay.RegisterCollectable( _pGaugeRegistry );
	while(1)
	{
		int nRet = oGateWay.Push();
		if( nRet == 200 )
		{
			//MYBOOST_LOG_DEBUG( "GeteWay Push Succ." << OS_KV( "ret", nRet ) );
		}
		else
		{
			MYBOOST_LOG_ERROR( "GateWay Push ERROR." << OS_KV( "ret", nRet ) );
		}
		sleep( _nPrometheusPushTime );
	}
}

Counter * RedisProxyPrometheus::Add( const string & sFamily, const string & sLabel  )
{
	string sFamilyLabel	= sFamily + ":" + sLabel;	
	if( _mapFamilyCounter.find( sFamilyLabel ) == _mapFamilyCounter.end() )
	{
		vector<string> LabelPair = TC_Common::sepstr< std::string >( sLabel, ":" );
		_mapFamilyCounter.insert( std::pair< std::string, Counter * >
				( sFamilyLabel, &_mapFamily[sFamily]->Add( {{ LabelPair[0], LabelPair[1] }} )));
	}
	return _mapFamilyCounter[sFamilyLabel];
}

Counter * RedisProxyPrometheus::Add( const string & sFamily, const map< std::string, std::string > & mapLabel )
{
    return &_mapFamily[sFamily]->Add( mapLabel );
}

void RedisProxyPrometheus::Report( const string sLabelItem, const double dMertricValue )
{
	_mapCounter[sLabelItem]->Increment(dMertricValue);
}

void RedisProxyPrometheus::ReportPlus( 
		const std::string & sFamily,
		const map< std::string, std::string > & mapLabel, 
		const double dMertricValue )
{
	if ( _mapFamily.find( sFamily ) != _mapFamily.end() )
	{
		Add( sFamily, mapLabel )->Increment( dMertricValue );
	}
}

void RedisProxyPrometheus::ReportPlus( 
		const std::string & sFamily,
		const string & sLabel, 
		const double dMertricValue )
{
	if ( _mapFamily.find( sFamily ) != _mapFamily.end() )
	{
		Add( sFamily, sLabel )->Increment( dMertricValue );
	}
}

void RedisProxyPrometheus::ReportGauge( const string sLabelKeys, const double dMertricValue  )
{
	_mapGauge[sLabelKeys]->Set(dMertricValue);
}

Gauge * RedisProxyPrometheus::AddGauge( const string & sFamily, const string & sLabel  )
{
	string sFamilyLabel	= sFamily + ":" + sLabel;	
	if( _mapFamilyGauge.find( sFamilyLabel ) == _mapFamilyGauge.end() )
	{
		vector<string> LabelPair = TC_Common::sepstr< std::string >( sLabel, ":" );
		_mapFamilyGauge.insert( std::pair< std::string, Gauge* >
				( sFamilyLabel, &_mapGaugeFamily[sFamily]->Add( {{ LabelPair[0], LabelPair[1] }} )));
	}
	return _mapFamilyGauge[sFamilyLabel];
}

void RedisProxyPrometheus::ReportPlusGauge( 
		const std::string & sFamily,
		const string & sLabel, 
		const double dMertricValue )
{
	if ( _mapGaugeFamily.find( sFamily ) != _mapGaugeFamily.end() )
	{
		AddGauge( sFamily, sLabel )->Set( dMertricValue );
	}
}


