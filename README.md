# RedisProxy 使用文档

RedisProxy 是一个分布式高可用、高性能、低成本的 Redis 代理解决方案。

## RedisProxy 开发背景

> * 混合部署，CPU/IO/网络资源争抢，导致互相影响不可用
> *	链接难管理，导致redis链接过多，负载增大
> *	安全能力弱，实例和服务模块权限不明确
> *	扩展能力弱，扩容时需要停机使用，影响业务正常使用
> * 数据挤兑，同个实例不同业务场景写入数据导致内存资源争抢(超内存)

## RedisProxy 版本简介
RedisProxy 目前主要release版本如下:

### RedisProxy 一期(1.0)特性
> * 完全支持原生 Redis 版本 2.8.x, 3.x.x和5.x.x等所有版本
> * 支持集群 TPS 15w+/s，低延时 <10 ms，Proxy 层无状态可平衡扩展
> * 支持按 Key 路由，引入逻辑 slot(槽) 和 group(组) 的概念，均衡数据存储和容灾
> * 支持对 Redis 长链接池，减少网络链接损耗
> * 支持 Pipeline 的模式请求，不支持 MGET、MSET
> * 支持SDK函数式接口，更方便接入和通用
> * RedisProxy 对 consul 强依赖，实现负载均衡、心跳检测、和客户端的服务发现
> * 使用 Protobuf 作为客户端和 Proxy 服务端通信协议
> * 鉴权、流控持续开发中
> * 数据结构通用化和二进制读写开发中

### RedisProxy 未来二期规划
> * 支持 slot 同步迁移、异步迁移和并发迁移，对 key 大小无任何限制，迁移性能大幅度提升
> * 元数据存储，可自行扩展支持新的存储，集群正常运行期间，即便元存储故障也不再影响 RedisProxy 集群，提升稳定性
> * 实现 select 命令，支持多 DB
> * 支持读写分离、优先读同 IP/同 DC 下副本功能
> * slot auto rebalance 算法从 2.0 的基于 max memory policy 变更成基于 group 下 slot 数量
> * 提供更加友好的 dashboard 界面和管理界面

### RedisProxy 整体架构

![image](https://github.com/annulsluo/RedisProxy/blob/master/photos/RedisProxyArchitecture1.png)

RedisProxy 1.0 由以下组件组成：

* **RedisProxyServer** 
	+ 服务程序启动类，参数获取、初始化本地日志、配置文件和 Slot到 group 路由映射关系、到 Redis 实例的链接池，绑定IP和端口号接口，启动监听。

* **ServerConfig** 
	+ 解析配置文件类，把配置内容封装成函数的形式提供外部其他组件进行访问，配置详细内容参考 2.1.2。

* **RedisProxySlot**
	+ 分槽分组映射类，按照配置预生成分组映射关系；提供对外函数，按 Key 获取 slot\group 信息(IP/PORT等)。

* **RedisConnPoolFactory**
	+ Redis 链接池类，为每个 group 创建一个链接池，并且启动心跳检测机制；提供对外函数根据 group 信息获取链接，释放链接等。

* **RedisProxyInterfaceImp** 
	+ 接口实现类，对客户端传入的参数进行有效性校验，生成业务唯一ID。

* **RedisProxyManager** 
	+ 具体接口逻辑管理类，把客户端请求转为内部请求，增加Session和业务信息、流控信息，触发命令处理函数，并且按顺序构造返回响应。

* **RedisProxyCmd**
	+ 命令处理类，一阶段：按照 Key 计算 Slot，拆分成以组单位的 pipeline 请求；
	+ 二阶段：发起 pipeline 请求，根据 pipeline 响应结果按顺序合并为内部响应。

* **RedisProxyBackEnd**
	+ Redis 协议后台处理类，真正跟 Redis 服务通信，首选 Pipeline 的命令方式进行协议请求和响应。

* **redisproxy.proto**
	+ 提供对外的接口函数规范，请求包和返回包协议格式，返回值说明等，支持 KV/Command/元素向量 方式请求。

* **RedisProxyCommon.proto**
	+ 提供内部使用的结构定义：应用信息、内部请求响应协议、Pipeline收发包协议、slot\group 信息结构。

## 0. 编译

### 部署[release binary]文件安装
仅支持 C++ 编译环境

### 编译源码安装

#### 1. 部署 grpc 环境 [参考这里](https://github.com/grpc/grpc)
#### 2. 部署 boost 环境 [参考这里](https://github.com/boostorg/boost)
#### 3. 部署 hiredis 环境 [参考这里](https://github.com/redis/hiredis)
#### 4. 部署 export-dev 环境 [参考这里](http://git.待补充.com/aiad-platform/cpp-server-proj/tree/master/export-dev)

#### 5. 部署编译环境

* 可创建 `/data/dmp/` 目录或把上述依赖的环境部署在 `/usr/local/` 下或者根据自己的环境修改 `makefile` 中的目录环境

#### 6. 编译 RedisProxy 源代码

* 直接通过 make 进行编译，可生成二进制文件

```
$ ls -lrht RedisProxyServer
-rwxr-xr-x 1 app apps 2.1M Dec  9 19:48 RedisProxyServer
```

## 1. RedisProxyServer 快速部署和启动
2分钟快速构建一个单机版测试 RedisProxy 集群，无任何外部组件依赖.

源码中 `RedisProxyServer.sh` 文件提供了一系列脚本以便快速启动、停止服务，提高运维效率。

### 启动RedisProxyServer
使用 `RedisProxyServer.sh` 脚本启动 Proxy，并查看 Proxy 日志确认启动是否有异常。

```
/data/dmp/app/RedisProxyServer/RedisProxyServer.sh start
tail -100 ../log/RedisProxyServer_2019-12-10_0.log
```
```
[2019-12-10 21:10:13.567628]: [DEBUG]:ServerConfig.cpp:117[Init]ServerConfig InitImp Succ
...
[2019-12-10 21:10:13.570599]: [DEBUG]:RedisProxySlot.cpp:45[Init]RedisProxySlot Init Succ.
[2019-12-10 21:10:13.571351]: [DEBUG]:RedisProxyServer.cpp:88[RunServer]Init Server Succ
```
同时生成 `RedisProxyServer.pid` 文件，记录当前服务线程号;若启动失败，请检查目录文件权限或者端口号是否占用
cat RedisProxyServer.pid
```
115343
```

### 启动redis
使用 `./src/redis-server ./redis.conf` 脚本启动 redis，并查看 redis 日志确认启动是否有异常。

```
tail -100 /tmp/log/redis_6379.log 
```
```
5706:M 08 Apr 16:04:11.748 * DB loaded from disk: 0.000 seconds
5706:M 08 Apr 16:04:11.748 * The server is now ready to accept connections on port 6379
```
redis.conf 配置中 pidfile、logfile 默认保存在 `/tmp` 目录，若启动失败，请检查当前用户是否有该目录的读写权限。

## 通过 Deploy 打包文件夹快速部署

使用 Deploy 文件夹可快速在单机、多机部署多套 RedisProxy 集群。

Deploy 文件夹包含了部署的 RedisProxyServer，根据自己部署环境修改 `RedisProxyServer.conf` 文件里参数，修改配置中的 svr_ip 和 svr_port 即可。

Deploy 部署集群需安装 `consul agent`[参考这里](http://git.待补充.com/aiad-platform/service-discovery-and-rate-limit)，其余没有任何依赖的内容。

```
git clone http://git.待补充.com/aiad-platform/cpp-server-proj/tree/master/RedisProxyServer/Deploy
$ cd ./Deploy
```

## 2. 启动及参数、配置

**注意：请按照顺序逐步完成操作。生产环境建议修改 RedisProxyServer.conf 配置，使用 `zookeeper` 或者 `consul` 作为外部存储。**

**注意：RedisProxy 1.x 支持 AUTH，但是要求同个 Redis 场景使用的 Passwd 必须完全相同。**

#### 2.1 RedisProxy 

##### 2.1.1 启动命令：

```
$ nohup ./bin/RedisProxyServer.sh --config=../conf/RedisProxyServer.conf &
```
+ 启动参数说明：
Usage:  
	RedisProxyServer [--config=CONF]
Options:
	--config=CONF		指定启动配置文件

默认配置文件 `./conf/RedisProxyServer.conf` 

##### 2.1.2 配置详细说明：

+ 默认配置文件：

```bash
$ cat RedisProxyServer.conf 
# 配置送结构如下
<root>										\\ 配置根节点
	env=online								\\ 设置当前环境为测试环境或者生产环境( beta/online )
	<RedisProxy_$env>
		<ObjConfig></ObjConfig>						\\ 公共配置属性
		<redis_$app_$table></redis_$app_$table>				\\ 表属性配置	
	</RedisProxy_$env>	
</root>

// ObjConfig 包含配置属性
enable_local_log=true								\\ 是否开启本地日志，会有少量性能影响
svr_ip=127.0.0.1								\\ 服务
svr_port=50051									\\ 端口号
authapp_ratio=0									\\ 是否进行应用App权限检验
rediscreateconnectbylocal=1							\\ 是否在本地创建redis链接，慎用，建议使用链接池
redisappname=aiad								\\ 应用名称列表，不同应用名使用 | 分割符号
redistablename=userprofile|userclick						\\ 表名称，不同表名称使用 | 分割符号

// redis_app_table
redisconnectmaxtrytimes=3							\\ 链接池最大重试次数
redistimeoutsec=0								\\ redis 请求超时时间（秒）
redistimeoutusec=300000								\\ redis 请求超时时间（毫秒）
redisapp=aiad									\\ 逻辑--业务应用名称
redistable=userprofile								\\ 逻辑--应用表名称
redisconnpoolmaxconnsize=50							\\ 链接池最大链接数
redisconnpoolminconnsize=5							\\ 链接池最小链接数
redisconnpoolminusedcnt=2							\\ 链接最小使用次数
redisconnpoolmaxidletime=1800							\\ 链接最大空闲时间
redisconnpoolkeepalivetime=5							\\ 链接心跳检测时间
redisconnpoolcreatesize=10							\\ 分组创建的链接个数
redispasswd=dsp12345								\\ redis 密码
redisslot=1024									\\ 逻辑--分槽
group_slot=1:1~20,2:21~40,3:41~61,4:62~81,5:82~102,6:103~122,7:123~143,8:144~163,9:164~184,10:185~204,11:205~225,12:226~245,13:246~266,14:267~286,15:287~307,16:308~327,17:328~348,18:349~368,19:369~389,20:390~409,21:410~430,22:431~450,23:451~471,24:472~491,25:492~512,26:513~532,27:533~552,28:553~573,29:574~593,30:594~614,31:615~634,32:635~655,33:656~675,34:676~696,35:697~716,36:717~737,37:738~757,38:758~778,39:779~798,40:799~819,41:820~839,42:840~860,43:861~880,44:881~901,45:902~921,46:922~942,47:943~962,48:963~983,49:984~1003,50:1004~1024    \\ 逻辑--格式 组ID:槽ID，组ID必须为数值，槽ID可以为范围值(1~20，~ 作为分割符号)，也可以是数值(1)
group_1=172.16.154.51:10010							\\ 逻辑--组格式 key格式：group_$ID；value格式：masterip:port,slave1ip:port,slave2ip:port
group_2=172.16.154.76:10010
```

## 3. 接口函函数 
### 3.1 参照protobuf文件 ./redisproxy.proto
```
service RedisProxyService{
	rpc RedisProxyCmd( RedisProxyRequest ) returns( RedisProxyResponse );
}
```

## 4. 测试
### 4.1 cpp 测试环境

#### 4.1.1 单线程测试用例：
```
$ cd ./client/cpp; ./redisproxy_client
Usage: 
	exe logpath logname ip port op[set|get|ping]
$ ./redisproxy_client ./ client 172.16.155.239 50051 set 
```

#### 4.1.2 多线程测试用例：
```
$ cd ./client/cpp; ./redisproxythreadclient
Usage:
	exe logpath logname ip port op[set|get|ping] threadnum pipelinenum qps

$ ./redisproxythreadclient ./ client 172.16.155.239 50051 set 1 1 10000
$ ok bodylist_size:1|pthreadid:140330524985088|totalreq:10000|threadnum:1|pipelinenum:1|totalcost:33967|percost:3.3967

$ ./redisproxythreadclient ./ client 172.16.155.239 50051 set 1 10 10000
ok bodylist_size:10|pthreadid:140038465308416|totalreq:10|threadnum:1|pipelinenum:10|totalcost:230|percost:23

$ ./redisproxythreadclient ./ client 172.16.155.239 50051 set 1 50 10000
ok bodylist_size:50|pthreadid:139936684951296|totalreq:10|threadnum:1|pipelinenum:50|totalcost:1013|percost:101.3
```
## 4.2 py 测试环境
+ 暂不支持

## 5. 使用到的框架
*  **grpc**		-- RPC框架
*  **protobuf**	-- 客户端跟 Proxy 之间协议 protobuf3.7.0
* **hiredis**	-- redis 提供的官方 C++ 
* **boost**		-- 使用的 boost 库
* **consul**	-- 服务发现组件

## 6. 贡献者
+ annulsluo
