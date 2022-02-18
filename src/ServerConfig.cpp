#include "ServerConfig.h"
#include "ServerDefine.h"
#include "util/TafOsVar.h"
#include "util/myboost_log.h"

ConfItem Conf[] = 
{
    // add code here
    //{"ServerConfig"},
    {"ObjConfig"},
    {"RedisProxyThreadPool"}  
};

SplitItem SplitConf[] =
{
    // foramt {name,split,type} as { "filter_db_souceid", ',', "int" } 
    { "***", ',',"type" },
	{"redisappname",'|',"str"},
	{"redistablename",'|',"str"},
};

void ServerConfigMng::GenRoute( TC_Config & oConfig )
{
    std::string env = oConfig.get( "/root<env>", "beta" );
    if( MapSSplitConf.find( "redisappname" ) != MapSSplitConf.end() )
    {
        for( vector<string>::iterator appit = MapSSplitConf[ "redisappname" ].begin(); 
                appit != MapSSplitConf[ "redisappname" ].end(); ++ appit )
        {
            if( MapSSplitConf.find( "redistablename" ) != MapSSplitConf.end() )
            {
                for( vector<string>::iterator tableit = MapSSplitConf[ "redistablename" ].begin(); 
                        tableit != MapSSplitConf[ "redistablename" ].end(); ++ tableit )
                {
                    string sRouteConfigPath = string( "/root/RedisProxy_" ) + env + "/";
                    string sAppTable        = string( *appit ) + "_" + string( *tableit ); 
                    sRouteConfigPath += sAppTable;
                    MapRoute[ sAppTable ]   = oConfig.getDomainMap( sRouteConfigPath );
					MYBOOST_LOG_DEBUG( OS_KV("apptable",sAppTable) << OS_KV( "routepath", sRouteConfigPath ) 
							<< OS_KV( "redisapp", MapRoute[sAppTable]["redisapp"] )
							<< OS_KV( "redistable", MapRoute[sAppTable]["redistable"] )
							<< OS_KV( "redispasswd", MapRoute[sAppTable]["redispasswd"] ) );
                }
            }
        }
    }
    else
    {
		MYBOOST_LOG_ERROR( "GenRoute Empty.");
    }
}

map< string, string > & ServerConfigMng::GetMapRouteByAppTable( const string & sAppTable )
{
//	if( MapRoute.find( sAppTable ) != MapRoute.end() )
	return MapRoute[ sAppTable ];
}

MapStr2MStr & ServerConfigMng::GetMapAllRoute()
{
	return MapRoute;
}

int ServerConfigMng::Init( TC_Config & config )
{
    ConfigItem &config_item = config_pair_.getBackData();
    config_item.LoadFromMap(config.getDomainMap(ConfigItem::GetConfigDomainPath()));

    std::string env = config.get( "/root<env>", "beta" );
	MYBOOST_LOG_DEBUG( OS_KV("env", env ) );

    for( size_t i = 0; i < sizeof( Conf ) / sizeof( ConfItem ); ++i )
    {
        // fix code here 
        string sConfig = string("/root/RedisProxy_") + env + "/" + Conf[i].sConfigName;
		MYBOOST_LOG_DEBUG( OS_KV("configpath",sConfig ) );
        MapConfig[ Conf[i].sConfigName ] = config.getDomainMap( sConfig );     

        if( Conf[i].sConfigName == "ObjConfig" )
        {
            for( size_t j = 0; j < sizeof( SplitConf ) / sizeof( SplitItem ); ++ j )
            {
				MYBOOST_LOG_DEBUG( OS_KV( "index", int(j) ) << OS_KV( "configname", SplitConf[j].sConfigName )
						<< OS_KV( "split", string(1,SplitConf[j].c)) << OS_KV( "type",SplitConf[j].type ) );

                if( SplitConf[j].type == "int" )
                {
                    vector<int>vecArray;
                    vecArray = TC_Common::sepstr<int>( MapConfig[Conf[i].sConfigName][SplitConf[j].sConfigName], string(1,SplitConf[j].c) );
                    MapISplitConf[ SplitConf[j].sConfigName ] = vecArray;
                }
                else if( SplitConf[j].type == "str" )
                {
                    vector<string>vecArray;
                    vecArray = TC_Common::sepstr<string>( MapConfig[Conf[i].sConfigName][SplitConf[j].sConfigName], string(1,SplitConf[j].c) );
                    MapSSplitConf[ SplitConf[j].sConfigName ] = vecArray;
                }
            }
        }
		if( Conf[i].sConfigName == "RedisProxyThreadPool" )
		{
			_oRedisProxy_ThreadPool_Config.LoadFromMap( MapConfig[Conf[i].sConfigName] );
		}
    }
	GenRoute(config);
	MYBOOST_LOG_DEBUG( "ServerConfig InitImp Succ." );
    config_pair_.switchIndex();
    return 0;
}

string ServerConfigMng::GetSObj( string sObjName )
{
    return MapConfig[ "ObjConfig" ][ sObjName ];
}

int ServerConfigMng::GetIObj( string sObjName )
{
    return TC_Common::strto<int>(MapConfig["ObjConfig"][ sObjName ]);
}

vector< int > ServerConfigMng::GetISplitConf( string sSplitName )
{
    return MapISplitConf[ sSplitName ];
}

vector< string > ServerConfigMng::GetSSplitConf( string sSplitName )
{
    return MapSSplitConf[ sSplitName ];
}

map<string,string> & ServerConfigMng::GetMap( string sIndex )
{
	return MapConfig[ sIndex ];
}

int ServerConfigMng::GetMaxTryTimes()
{
    return GetIObj( "cache_try_time" );
}

void ServerConfigMng::BindLabelEnum()
{
	MapRedisEnum["GET"]				= RedisProxyCommon::REDIS_CMD_GET;
	MapRedisEnum["EXISTS"]			= RedisProxyCommon::REDIS_CMD_EXISTS;
	MapRedisEnum["ZCOUNT"]			= RedisProxyCommon::REDIS_CMD_ZCOUNT;
	MapRedisEnum["ZRANGEBYSCORE"]	= RedisProxyCommon::REDIS_CMD_ZRANGEBYSCORE;
	MapRedisEnum["EXPIRE"]			= RedisProxyCommon::REDIS_CMD_EXPIRE;
	MapRedisEnum["SET"]				= RedisProxyCommon::REDIS_CMD_SET;
	MapRedisEnum["ZADD"]			= RedisProxyCommon::REDIS_CMD_ZADD;
	MapRedisEnum["ZREMRANGEBYSCORE"] = RedisProxyCommon::REDIS_CMD_ZREMRANGEBYSCORE;
	MapRedisEnum["ZREMRANGEBYRANK"]	= RedisProxyCommon::REDIS_CMD_ZREMRANGEBYRANK;
	MapRedisEnum["HGET"]			= RedisProxyCommon::REDIS_CMD_HGET;
	MapRedisEnum["HMGET"]			= RedisProxyCommon::REDIS_CMD_HMGET;
	MapRedisEnum["HGETALL"]			= RedisProxyCommon::REDIS_CMD_HGETALL;
	MapRedisEnum["HDEL"]			= RedisProxyCommon::REDIS_CMD_HDEL;
	MapRedisEnum["HMSET"]			= RedisProxyCommon::REDIS_CMD_HMSET;
	MapRedisEnum["HINCRBY"]			= RedisProxyCommon::REDIS_CMD_HINCRBY;
	MapRedisEnum["HINCRBYFLOAT"]	= RedisProxyCommon::REDIS_CMD_HINCRBYFLOAT;
}
