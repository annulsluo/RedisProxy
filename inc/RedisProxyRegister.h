/*************************************************************************
    > File Name: RedisProxyRegister.h
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 一 12/30 10:25:19 2019
 ************************************************************************/
#ifndef _RedisProxyRegister_H_
#define _RedisProxyRegister_H_
#include "util/tc_singleton.h"
#include "ServerDefine.h"
#include "ServerConfig.h"

class RedisProxyRegister:
	public taf::TC_Singleton< RedisProxyRegister >
{
	public:
		int InitRegister( 
                const ConsulInfo & oConsulInfo,
                const string & sIp, 
                const string & sPort );
		int Init( const string & sIp, const string & sPort );
		void SetIp( const string & sIp );
		void SetPort( const string & sPort );
	private:
		int RegisterByHttp();
		
	private:
		string _sConsulProto;
		string _sConsulIpList;
		string _sConsulPort;
		string _sConsulReportInterVal;
		string _sConsulReportTimeout;
		string _sCheckId;
		string _sConsulServiceName;
		string _sConsulToken;
		string _sConsulApi;
		vector< string > _vecUrl;
		string _sPostParams;
		string _sIp;
		string _sPort;
};

#endif 
