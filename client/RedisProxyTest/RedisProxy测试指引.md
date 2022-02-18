# RedisProxy测试指引

## 测试资源列表
- 部署的RedisProxy机器服务
  172.16.155.239 172.16.155.210 
  用户： app
  密码：Apps@123
 
## 测试前准备
1. 熟悉 grpc、protobuf、redis命令 和 redisproxy.proto文件协议
2. 依据 redisproxy.proto service 接口进行对 redis 的数据读取等操作
3. 开发相应的测试工具对功能和性能进行测试，测试工具参(目前仅有C++版本)考 app@172.16.155.239 /data/dmp/app/RedisProxyClient 下:
3.1 单线程工具：redisproxy_client.cpp
3.2 多线程工具：redisproxythreadclient.cpp
4. 功能测试要求：
4.2 支持如下命令操作
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
5. 性能测试要求：
5.1 单机性能要求1.5w QPS
5.2 单次请求耗时要求 < 10ms
6. 测试数据要求：
6.1 字符串数据
6.2 二进制数据
6.3 数据建议长度 500 字节；最大长度建议 1M

## 测试中步骤
1. 登陆 172.16.155.239
2. 进入/data/dmp/app/RedisProxyClient/
3. 部署相应的测试工具到该目录
4. 按照 测试前准备 4.2 中描述的各命令操作进行测试，确认功能正常；按照测试前准备5 中的性能要求测试
5. 结束

