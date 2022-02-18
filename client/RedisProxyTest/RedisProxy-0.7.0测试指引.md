# RedisProxy测试指引

## 测试资源列表
- 部署的RedisProxy机器服务
  172.16.155.239 172.16.155.210 
  用户： app
  密码：Apps@123
 
## 测试前准备
1. 熟悉 grpc、protobuf、redis命令 和 redisproxy.proto文件协议
2. 依据 redisproxy.proto service 接口进行对 redis 的数据读取等操作
3. 开发相应的测试工具对功能和性能进行测试，测试工具参(目前C++/py版本)考 app@172.16.155.239 /data/projects/RedisProxyClient 下:
4. 功能测试要求：
4.1 支持如下命令操作
	get
	exists
	expire
	set
	zcount
	zrangeByScore
	zadd
	zremrangeByScore
	zremrangeByRank
	hget
	hmget
	hgetAll
	hdel
	hset
	hmset
	hincrBy
	hincrByFloat
	incrBy
	setex
	zrevrangebyscore withscores
    setbit
    getbit 
5. 测试数据要求：
5.1 字符串数据
5.2 二进制数据
key: cfcd208495d565ef66e7dff9f98764da	value: CnZEEAAAAAACAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAAAAAAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEAAAAAAAAACAAAAAAAgEgEH
对value写入前做base64.b64decode，再使用二进制类型写入
5.3 数据建议长度 500 字节；最大长度建议 1M

## 测试中步骤
1. 登陆 172.16.155.239
2. 进入/data/projects/RedisProxyClient/
3. 部署相应的测试工具到该目录
4. 按照 测试前准备 
	4.1 中描述的各命令操作进行测试，确认功能正常；按照测试前准备5 中要求测试
	4.2 正常发送写的命令(set/hset/hmset/zadd/zremrange/hincryBy/hincrByFloat/incrBy/setex/expire/hdel)
	4.3 模拟redis节点下线情况：发送读的命令(get/hget/hgetall/hmget/zcount/zrangeByScore/exists)，检查数据是否正常读
	4.4 模拟redis节点下线后再正常上线，测试读写数据是否正常
	4.5 模拟所有节点下线(集群下线)情况
	PS: 集群下线即所有节点都下线( 
		1. 修改配置文件/data/projects/RedisProxyServer/conf/RedisProxyServer failover_ratio=1; 
		2. kill 掉所有redis 实例的线程 )
		redis节点下线：kill掉特定要测试下线的线程(可任意指定)

5. 结束

