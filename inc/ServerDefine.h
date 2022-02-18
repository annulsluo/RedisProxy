#ifndef _ServerDefine_H
#define _ServerDefine_H
#include <map>
#include <vector>
#include <grpc/support/log.h>
#include "RedisProxyCommon.pb.h"
#include "RedisConnectCommon.h"

using namespace RedisProxyCommon;
using namespace std;

#define MAX_TRY_TIMES 3
#define CMD_CMD_INDEX 0
#define CMD_KEY_INDEX 1
#define CMD_VALUE_INDEX 2
#define CMD_OFFSET_INDEX 3
#define ELE_KEY_INDEX 1 
#define GRPC_CLIENTINFO_LEN		3
#define MAX_PARTITION_LEN		10
#define PARTITION_ERR			-1
#define GROUPS_APPTABLE_INDEX   3	
#define REDISCONFINFO_APPTABLE_INDEX 3
#define REDISCONNPOOLCONFINFO_APPTABLE_INDEX 3
#define SLOTCONF_APPTABLE_INDEX 3		
#define SLOTACTION_APPTABLE_INDEX 3 
#define GROUPS_NODE_INDEX		5
#define FAILOVER_TOPIC			"failtopic"
#define SCAN_FINISH				0
#define MIN_GROUPID             1
#define SLEEP_TIME              20
#define SERVER_START_WAIT_TIME  5
#define DEFAULT_GROUP_VERSION   0

struct MigrateConf
{
    int nScanCount;                                 // 每次扫描的个数
    int nBatchMigrateNum;                           // 批量迁移的个数
    int nMigrateTimeout;                            // 迁移超时时间
	int nMigrateSleepTime;							// 每次迁移休眠时间
	int nMaxScanTimes;								// 最大扫描次数
    std::string sScanPattern;                       // 扫描匹配模式
};

// map string->all
typedef map<string, int> MapStr2Int;
typedef map<string, string> MapStr2Str;
typedef map<string, vector<int> > MapStr2VInt;
typedef map<string, vector<string> > MapStr2VStr;
typedef map<string, map<string,string> > MapStr2MStr;
typedef map<string, GroupInfo> MapStr2GroupInfo;
typedef map<string, AppTableInfo> MapStr2AppTableInfo;
typedef map<string, vector<GroupInfo> > MapStr2VGroupInfo;
typedef map<string, RedisProxyCommon::AppInfo > MapStr2AppInfo;
typedef map<string, map<string, RedisProxyCommon::Instance > > MapStr2SSInstance;
typedef map<string, RedisProxyCommon::ClusterInfo > MapStr2ClusterInfo;
typedef map<string, map<int, RedisProxyCommon::GroupInfo > > MapStr2GIdGroupInfo;
typedef map<string, RedisProxyCommon::FailOverInfo > MapStr2FailOverInfo;
typedef map<string, RedisConf > MapStr2RedisConf;
typedef map<string, RedisConnPoolConf > MapStr2RedisConnPoolConf;
typedef map<string, RedisProxyCommon::SlotConf > MapStr2SlotConf;
typedef map<string, map<int, RedisProxyCommon::SlotAction > > MapStr2ISlotAction;
typedef map<string, MigrateConf > MapStr2MigrateConf;

// map int->all
typedef map<int, int > MapInt2Int;
typedef map<int, vector<int> > MapInt2VInt;
typedef map<int, string> MapInt2Str;
typedef map<int, vector< string > > MapInt2VStr;
typedef map<int, vector< RedisProxyCommon::GroupInfo> > MapInt2VGroupInfo;
typedef map<int, GroupInfo> MapInt2GroupInfo;
typedef map<int, RedisProxyCommon::PipeLineRequest > MapInt2PipeLine;
typedef map<int, RedisProxyCommon::FailOverInfo > MapInt2FailOverInfo;
typedef map<int, RedisProxyCommon::SlotAction > MapInt2SlotAction;

// map
typedef map<string, map<int, RedisProxyCommon::GroupInfo > > MapStr2SlotGroupInfo;
typedef map<string, map<int, int > > MapStr2MapInt2Int;

struct ConsulInfo
{
	int nConsulRegisterRatio;
	string sConsulIpList;
	int nConsulPort;
	int nConsulReportInterVal;
	int nConsulReportTimeout;
	string sConsulToken;
	string sConsulApi;
	string sConsulProto;
	string sConsulServiceName;
};

struct KafkaInfo
{
	string	sBrokers;
	string	sTopics;
	string	sPartitions;
	bool	bIsKeepRun;
	int		nLmdbPartition;
};

struct PartitionInfo
{
	int nNum;
	bool bUsed;
	int nOffset;
};

enum ETransferCmdMark
{
	TRANSFERCMDMARD_ONLY_READ  = 0x0001,
	TRANSFERCMDMARD_ONLY_WRITE = 0x0010,
	TRANSFERCMDMARD_READ_AND_WRITE = 0x0100
};



#endif

