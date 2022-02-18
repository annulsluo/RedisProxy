#ifndef _SERVERCONFIG_H_
#define _SERVERCONFIG_H_

#include <stdlib.h>
#include <string>
#include <ostream>
#include "util/tc_singleton.h"
#include "util/data_pair.h"
#include "util/tc_common.h"
#include "ServerDefine.h"
#include "util/tc_config.h"
#include "util/ProcessThreadPool.h"
using namespace taf;

// 配置组件
struct ConfItem
{
    string sConfigName;
};

extern ConfItem Conf[];

struct SplitItem
{
    string sConfigName;
    char c;
    string type;
};

extern SplitItem SplitConf[];

class ServerConfigMng:
    public taf::TC_Singleton< ServerConfigMng >
{
public:
    struct ConfigItem;

    virtual const char* Name() const { return "ServerConfig"; }
    virtual int Init( TC_Config & config );
    virtual std::ostream& Stat(std::ostream &os) const {
        const ConfigItem &config_item = config_pair_.getCurrData();
        return config_item.Stat(os);
    }
    const ConfigItem& GetCurrConfigItem() {
        const ConfigItem &config_item = config_pair_.getCurrData();
        return config_item;
    }

public:
    // 配置项
    struct ConfigItem {
        static std::string GetConfigDomainPath() { return "/root/ServerConfig"; }
        void LoadFromMap(const std::map<std::string, std::string> &param_kvs) {
            enable_local_log = (GetValueFromMap(param_kvs, "enable_local_log", "true") == "true");
            enable_remote_log = (GetValueFromMap(param_kvs, "enable_remote_log", "true") == "true");
        }
        std::ostream& Stat(std::ostream &os) const {
            os << OS_VAR_BOOL(enable_local_log) << std::endl
               << OS_VAR_BOOL(enable_remote_log) << std::endl
               ;
            return os;
        }

        bool enable_local_log;      // 启动FDLOG本地日志
        bool enable_remote_log;     // 启动FDLOG远程日志
    };

    ProcessThreadPoolConfigItem GetThreadPoolConfig() { return _oRedisProxy_ThreadPool_Config; };
    string GetSObj( string sObjName );
    int GetIObj( string sObjName );
    vector< int > GetISplitConf( string sSplitName );
    vector< string > GetSSplitConf( string sSplitName );
    map<string,string> & GetMap( string sIndex );
    int GetMaxTryTimes();
    void BindLabelEnum();
	void GenRoute( TC_Config & oConfig );
	map< string, string > & GetMapRouteByAppTable( const string & sAppTable );
	MapStr2MStr & GetMapAllRoute();


private:
    DataPair<ConfigItem> config_pair_;
    MapStr2MStr MapConfig;
    ProcessThreadPoolConfigItem _oRedisProxy_ThreadPool_Config;
    MapStr2VInt MapISplitConf;
    MapStr2VStr MapSSplitConf;
    MapStr2Int  MapRedisEnum;
	MapStr2MStr  MapRoute;
};

#endif

