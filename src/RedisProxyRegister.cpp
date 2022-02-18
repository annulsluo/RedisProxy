/*************************************************************************
    > File Name: RedisProxyRegister.cpp
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 一 12/30 14:20:44 2019
 ************************************************************************/
#include "RedisProxyRegister.h"
#include "curl/curl.h"
#include "util/myboost_log.h"
#include "ServerConfig.h"
#include "AICommon.pb.h"

int RedisProxyRegister::InitRegister( 
		const ConsulInfo & oConsulInfo, 
		const string & sIp, 
		const string & sPort )
{
	_sIp	= _sIp.empty()?sIp:_sIp;
	_sPort	= _sPort.empty()?sPort:_sPort;
	if( oConsulInfo.nConsulRegisterRatio )	
	{
		vector< string > vecIp	= TC_Common::sepstr<string>( oConsulInfo.sConsulIpList, "," );
		for( size_t i = 0; i < vecIp.size(); ++ i )
		{
			string sUrl = 
				oConsulInfo.sConsulProto + "://" + vecIp[i] + ":" 
                + TC_Common::tostr(oConsulInfo.nConsulPort) + "/" + oConsulInfo.sConsulApi;

			if( !oConsulInfo.sConsulToken.empty() )
				sUrl = sUrl + "?token=" + oConsulInfo.sConsulToken; 
			_vecUrl.push_back( sUrl );
		}
		string sServiceId		= oConsulInfo.sConsulServiceName + "_" + _sIp + "_" + _sPort;
		string sCheckId			= "checkId_" + sServiceId;
		string sCheckStr = "{\"CheckID\":\"" + sCheckId + "\","
		+ "\"name\":\"SSH TCP on " + _sIp +  "\","
		+ "\"interval\":\"" + TC_Common::tostr(oConsulInfo.nConsulReportInterVal) + "s\","
		+ "\"tcp\":\"" + _sIp + ":" + _sPort + "\","
		+ "\"timeout\":\"" + TC_Common::tostr(oConsulInfo.nConsulReportTimeout) + "s\"}";

		_sPostParams = "{\"id\":\"" + sServiceId + "\"," 
			+ "\"Address\":\"" + _sIp + "\","
			+ "\"Port\":" + _sPort + ","
			+ "\"name\":\"" + oConsulInfo.sConsulServiceName + "\","
			+ "\"check\":" + sCheckStr + "}";
		
		MYBOOST_LOG_INFO( OS_KV( "PostParams", _sPostParams ) ); 
	}
	return RegisterByHttp();
}

int RedisProxyRegister::RegisterByHttp()
{
	MY_TRY
		curl_global_init( CURL_GLOBAL_DEFAULT );
		CURL * pCurl	= curl_easy_init();
		CURLcode oRet;
		long lHttpCode = 0;
        size_t i = 0;
		if( pCurl )
		{
			for( i = 0; i < MAX_TRY_TIMES; ++i )
			{
				curl_easy_setopt( pCurl, CURLOPT_CUSTOMREQUEST,	"PUT" );
				curl_easy_setopt( pCurl, CURLOPT_URL, _vecUrl[(i % _vecUrl.size())].c_str() );
				curl_slist * plist = curl_slist_append( NULL, "Content-Type:application/json" );
				curl_easy_setopt( pCurl, CURLOPT_HTTPHEADER, plist );
				curl_easy_setopt( pCurl, CURLOPT_POSTFIELDS, _sPostParams.c_str() );
				curl_easy_setopt( pCurl, CURLOPT_POSTFIELDSIZE, _sPostParams.size() );
				curl_easy_setopt( pCurl, CURLOPT_TIMEOUT, 3 );                     // 设置超时时间
				oRet = curl_easy_perform( pCurl );
				if( oRet != CURLE_OK )
				{
					MYBOOST_LOG_ERROR( "RedisProxyRegister curl perform Error." 
						<< OS_KV( "ret", oRet ) << OS_KV( "Url", _vecUrl[i%_vecUrl.size()]) );
					continue;
				}
				break;
			}
			if( oRet != CURLE_OK )
			{
				MYBOOST_LOG_ERROR( "RedisProxyRegister Init Error." 
						<< OS_KV( "ret", oRet ) << OS_KV( "Postparam", _sPostParams ) );
				return AICommon::RET_FAIL;
			}
			else
			{
				curl_easy_getinfo( pCurl, CURLINFO_RESPONSE_CODE, &lHttpCode );
			}
			curl_easy_cleanup( pCurl );
		}
		curl_global_cleanup();

		if( lHttpCode == 200 )
		{
			MYBOOST_LOG_INFO( "RedisProxyRegister Init Succ." );
		}
		else 
		{
			MYBOOST_LOG_ERROR( "RedisProxyRegister Init Error." 
					<< OS_KV( "ret", oRet ) << OS_KV( "Postparam", _sPostParams ) 
					<< OS_KV( "httpcode", lHttpCode )
                    << OS_KV( "url", _vecUrl[((i-1)%_vecUrl.size())]));
			return AICommon::RET_FAIL;
		}
	MYBOOST_CATCH( "RedisProxyRegsiter Init Excep.", LOG_ERROR );
	return AICommon::RET_SUCC;
}

void RedisProxyRegister::SetIp( const string & sIp )
{
	_sIp = sIp;
}

void RedisProxyRegister::SetPort( const string & sPort )
{
	_sPort = sPort;
}

int RedisProxyRegister::Init( const string & sIp, const string & sPort ) 
{
	/*
	{
    "id": "{serviceName}_{ip}_{port}",
    "Address": "{ip}",
    "Port": {port},
    "name": "{serviceName}",
    "check": {
        "name": "SSH TCP on {ip}:{port}",
        "tcp": "{ip}:{port}",
        "interval": "5s",
        "timeout": "1s"
		}
	}
	*/
	int nRedisProxyRegisterRatio = ServerConfigMng::getInstance()->GetIObj( "redisproxyregister_ratio" );
	if( nRedisProxyRegisterRatio )
	{
		_sConsulProto			= ServerConfigMng::getInstance()->GetSObj( "consul_proto" );
		_sConsulIpList			= ServerConfigMng::getInstance()->GetSObj( "consul_ip" );
		_sConsulPort			= ServerConfigMng::getInstance()->GetSObj( "consul_port" );
		_sConsulReportInterVal	= ServerConfigMng::getInstance()->GetSObj( "consul_report_interval" );
		_sConsulReportTimeout	= ServerConfigMng::getInstance()->GetSObj( "consul_report_timeout" );
		_sConsulToken			= ServerConfigMng::getInstance()->GetSObj( "consul_token" );
		_sConsulServiceName		= ServerConfigMng::getInstance()->GetSObj( "consul_service_name" );
		_sConsulApi				= ServerConfigMng::getInstance()->GetSObj( "consul_api" );
		string sServiceId		= _sConsulServiceName + "_" + sIp + "_" + sPort;
		string sCheckId			= "checkId_" + sServiceId;

		vector< string > vecIp	= TC_Common::sepstr<string>( _sConsulIpList, "," );
		for( size_t i = 0; i < vecIp.size(); ++ i )
		{
			string sUrl = 
				_sConsulProto + "://" + vecIp[i] + ":" + sIp + "/" + sPort;

			if( !_sConsulToken.empty() )
				sUrl = sUrl + "?token=" + _sConsulToken; 
			_vecUrl.push_back( sUrl );
		}

		string sCheckStr = "{\"CheckID\":\"" + sCheckId + "\","
			+ "\"name\":\"SSH TCP on " + sIp +  "\","
			+ "\"interval\":\"" + _sConsulReportInterVal + "s\","
			+ "\"tcp\":\"" + sIp + ":" + sPort + "\","
			+ "\"timeout\":\"" + _sConsulReportTimeout+ "s\"}";

		_sPostParams = "{\"id\":\"" + sServiceId + "\"," 
			+ "\"Address\":\"" + sIp + "\","
			+ "\"Port\":" + sPort + ","
			+ "\"name\":\"" + _sConsulServiceName + "\","
			+ "\"check\":" + sCheckStr + "}";
	}
	return RegisterByHttp();
}
