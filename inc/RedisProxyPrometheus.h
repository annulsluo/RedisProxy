/*************************************************************************
    > File Name: RedisProxyPrometheus.h
    > Author: annulsluo
    > Mail: annulsluo@gmail.com
    > Created Time: 日  3/ 1 20:55:04 2020
 ************************************************************************/
#ifndef _RedisProxyPrometheus_H_
#define _RedisProxyPrometheus_H_

#include "util/tc_singleton.h"
#include "ServerDefine.h"
#include "ServerConfig.h"
#include <pthread.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/gateway.h>
#include <prometheus/family.h>

using namespace prometheus;

class RedisProxyPrometheus:
	public taf::TC_Singleton< RedisProxyPrometheus >,
	public taf::TC_Thread
{
	public:
		int Init( std::string sIp = "", std::string sPort = "" );
		void InitCounterFamily();
		void InitGaugeFamily();
		void Report( const std::string sLabelItem, const double dMertricValue = 1 );
		void ReportGauge( const std::string sLabelKeys, const double dMertricValue );

		void ReportPlus( 
				const std::string & sFamily,
				const std::string & sLabel, 
				const double dMertricValue = 1 );

        void ReportPlus( 
				const std::string & sFamily,
				const map< std::string, std::string > & sLabel, 
				const double dMertricValue = 1 );

		void ReportPlusGauge( 
				const std::string & sFamily,
				const std::string & sLabel, 
				const double dMertricValue = 1 );

	private:
		string GetHostName();
		void run();
		Counter * Add( const std::string & sFamily, const std::string & sLabel );
        Counter * Add( const std::string & sFamily, const map< std::string, std::string > & mapLabel );

		Gauge * AddGauge( const std::string & sFamily, const std::string & sLabel );
		
	private:
        std::string _sPrometheusGateWayHost;
        std::string _sPrometheusGateWayPort;
        std::string _sPrometheusGateWayJob;
		int _nPrometheusPushTime;
		string _sHostName;
		map< string, Counter * > _mapCounter;
		shared_ptr< Registry > _pCounterRegistry;
		map< std::string, Family<Counter> * > _mapFamily;
		map< std::string, Counter * > _mapFamilyCounter;

		map< std::string, Gauge * > _mapGauge;
		map< std::string, Family<Gauge> * > _mapGaugeFamily;
		map< std::string, Gauge * > _mapFamilyGauge;
		shared_ptr< Registry > _pGaugeRegistry;
		
};

#endif 
