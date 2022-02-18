/*************************************************************************
    > File Name: RedisConnectCommon.h
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 二  3/24 10:41:58 2020
 ************************************************************************/
#ifndef REDIS_CONNECT_COMMON_H
#define REDIS_CONNECT_COMMON_H

#include <map>
#include "util/tc_ex.h"
using namespace std;
using namespace taf;

#define MAX_CONNECT 50				// 最大链接数
#define MIN_CONNECT 5				// 最小链接数
#define MIN_USEDCNT  2				// 最小使用次数
#define MAX_IDLETIME 600			// 最大空闲时间
#define DEFAULTKEEPALIVETIME 5		// 默认心跳检测时间
#define MAX_REPORT_LEN	3			// 检测报告长度
#define MAX_REPORT_DETECT_CONN 3	// 最大心跳检测链接数

/**
* @brief 数据库异常类
*/
struct TC_RedisConnectPool_Exception : public taf::TC_Exception
{
	TC_RedisConnectPool_Exception( const string &sBuffer ) : TC_Exception(sBuffer){};
	TC_RedisConnectPool_Exception( const string &sBuffer, int nErr ) : TC_Exception(sBuffer, nErr){};
	~TC_RedisConnectPool_Exception() throw(){};    
};

/**
* @brief Redis配置接口
*/
struct RedisConf
{
	string	_sHost;						// 主机地址 
	int		_nPort;						// 端口号
	string	_sPasswd;					// 密码
	int		_nDbNum;					// 数据库Number
	int		_nConnectMaxTryTimes;		// 链接重试次数
	struct	timeval _nTimeout;			// 请求超时时间
	/**
	* @brief 读取数据库配置. 
	* 
	* @param mpParam 存放数据库配置的map 
	*   	 host: 主机地址
	*   	 passwd:密码
	*   	 dbnum:数据库编号
	*   	 port:端口
    */
    void loadFromMap(const map<string, string> &mpParam)
    {
        map<string, string> mpTmp = mpParam;

        _sHost			= mpTmp["host"];
        _sPasswd		= mpTmp["passwd"];
        _nDbNum			= atoi(mpTmp["dbnum"].c_str());
        _nPort			= atoi(mpTmp["port"].c_str());
		_nTimeout.tv_sec	= atoi(mpTmp["timeout_sec"].c_str());
		_nTimeout.tv_usec	= atoi(mpTmp["timeout_usec"].c_str());
		_nConnectMaxTryTimes = atoi(mpTmp["connectmaxtrytimes"].c_str());

        if( mpTmp["port"] == "" ) _nPort	= 6379;
		if( mpTmp["dbnum"] == "" ) _nDbNum	= 0;
		if( mpTmp["timeout_sec"] == "" ) _nTimeout.tv_sec	= 0;
		if( mpTmp["timeout_usec"] == "" ) _nTimeout.tv_usec	= 300000;
		if( mpTmp["connectmaxtrytimes"] == "" ) _nConnectMaxTryTimes = 3;
    }
};

/**
* @brief Redis配置接口
*/
struct RedisConnPoolConf
{
	int _nMaxSize;						// 最大链接数
	int _nMinSize;						// 最小链接数
	int _nMinUsedCnt;					// 最少使用次数阀值
	int _nMaxIdleTime;					// 最大空闲时间
	int _nCreateSize;					// 创建数
	bool _bIsNeedKeepAlive;				// 是否需要保活
	int _nKeepAliveTime;				// 保持时间
	bool _bIsNeedReportDetect;			// 是否需要上报检测
};

enum ERedisConnState
{
	REDIS_CONN_OK		=	0,			// 表示正常工作
	REDIS_CONN_PFAIL	=	1,			// 表示疑似不能正常工作
	REDIS_CONN_FAIL		=	2			// 表示不能正常工作
};

struct RedisConnDetectReport
{
	int nRedisConnState;				// 检测状态
	int nTime;							// 检测时间
};

struct RedisConnPoolStatus
{
	int _nCurrSize;								// 当前总数
	int _nUsed;									// 被使用个数
	int _nAutoIncrement;						// 自增主键，用于标识ID
	RedisConnDetectReport _aReport[3];			// 连接检测报告
	int _nReportIndex;							// 连接下标
	int _nLastConnState;						// 上一次检测情况
};

#endif 
