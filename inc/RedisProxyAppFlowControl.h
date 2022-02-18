/*************************************************************************
    > File Name: RedisProxyAppFlowControl.h
    > Author: annulsluo
    > Mail: annulsluo@gmail.com
    > Created Time: 一  3/23 14:47:55 2020
 ************************************************************************/

#ifndef _RedisProxyAppFlowControl_H_
#define _RedisProxyAppFlowControl_H_

#include <stdlib.h>
#include <stdint.h>
#include <map>
#include "util/tc_thread_mutex.h"
#include "util/FlowControl.h"
#include "RedisProxyCommon.pb.h"

struct AppFlowControlConfigItem {
    uint64_t nMaxRequestPerIntervalMs;
    int nIntervalMs;
    FlowControl oFlowCtrl;
};

class RedisProxyAppFlowControl :
    public taf::TC_Singleton<RedisProxyAppFlowControl>
{
public:
    void Init( map< std::string, RedisProxyCommon::AppInfo > & mapAppid2AppInfo ) {
		std::map< std::string, AppFlowControlConfigItem > tmpApp2FlowControlConfig;	
		for( map< std::string, RedisProxyCommon::AppInfo >::const_iterator mIt = mapAppid2AppInfo.begin(); 
				mIt != mapAppid2AppInfo.end(); ++ mIt )
		{
			if( mIt->first.empty() || mIt->second.nflowcontrolmaxreq() <= 0 ) continue;
			AppFlowControlConfigItem oAppFlowControlConfig;
			oAppFlowControlConfig.nMaxRequestPerIntervalMs		= mIt->second.nflowcontrolmaxreq();
			oAppFlowControlConfig.nIntervalMs					= mIt->second.nflowcontrolintervalms();
			tmpApp2FlowControlConfig[ mIt->second.sappid() ]	= oAppFlowControlConfig;
			//MYBOOST_LOG_INFO( OS_KV( "appid", mIt->second.sappid() ) 
			//			<< OS_KV( "maxreq", oAppFlowControlConfig.nMaxRequestPerIntervalMs )
			//		<< OS_KV( "intervalms", oAppFlowControlConfig.nIntervalMs ));
		}
		_mapApp2FlowControlConfig.swap( tmpApp2FlowControlConfig );
		MYBOOST_LOG_INFO( "RedisProxyAppFlowControl Init Succ. " << OS_KV( "size", _mapApp2FlowControlConfig.size() ) );
    }

public:
    bool IsFlowControl( std::string & sAppId, uint64_t nReqCnt ) {

		std::map< std::string, AppFlowControlConfigItem >::iterator mIt 
					= _mapApp2FlowControlConfig.find( sAppId );
		std::map< std::string, AppFlowControlConfigItem >::iterator allMIt 
					= _mapApp2FlowControlConfig.find( "all" );

		if( mIt != _mapApp2FlowControlConfig.end() && allMIt != _mapApp2FlowControlConfig.end() )
		{
			//MYBOOST_LOG_ERROR( "IsFlowControl process." << OS_KV( "appid", sAppId ) );
			return (mIt->second.oFlowCtrl.ProcessRequest( 
						nReqCnt, mIt->second.nMaxRequestPerIntervalMs, mIt->second.nIntervalMs ) > 0
					|| allMIt->second.oFlowCtrl.ProcessRequest( 
						nReqCnt, allMIt->second.nMaxRequestPerIntervalMs, allMIt->second.nIntervalMs ) > 0 );
		}
		return false;
    }

private:
	std::map< std::string, AppFlowControlConfigItem > _mapApp2FlowControlConfig;

};


#endif 
