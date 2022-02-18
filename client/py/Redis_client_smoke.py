# -*- coding: utf-8 -*-
# @author: v_rcxiao
# @time: 2019/12/23
# Client.py

import grpc
import json
import redisproxy_pb2_grpc
import redisproxy_pb2 as redisproxy__pb2
from google.protobuf import json_format

'''
命令用例设计仅供参考，可根据需求改变命令函数
'''


def SerializeToString(redis):
    res = redis.SerializeToString()
    return res


def ParseFromString(res):
    Redis = redisproxy__pb2.RedisProxyRequest()
    Redis.ParseFromString(res)
    print(Redis)
    p = json_format.MessageToJson(Redis)
    print(p)
    return p


class RedisProxyClient():
    def __init__(self):
        pass

    def test_command(self, command):
        redis = redisproxy__pb2.RedisProxyRequest()
        redis.sAppId = 'redis_test'
        redis.sAppPw = 'redis_teset'
        redis.sAppName = 'aiad'
        redis.sTableName = 'userprofile'

        redis.oRequestHead.nCallMode = redisproxy__pb2.CALLMODE_PIPELINE
        redis.oRequestHead.nCallType = redisproxy__pb2.CALLTYPE_SYNC
        oRequestBodyList = redis.oRequestBodyList.add()

        print('test: ', command)
        oRequestBodyList.sCmd = command
        # oRequestBodyList.sKey = ''
        # oRequestBodyList.sValue = ''

        oRequestBodyList.nVer = 2
        oRequestElementList = oRequestBodyList.oRequestElementList.add()
        oRequestElementList.nCharType = redisproxy__pb2.CHARTYPE_STRING
        # nRequestElementType = oRequestElementList.nRequestElementType.add()
        response = self.send(redis)
        # with open('test.json', 'a', encoding='utf-8') as fp:
        #     fp.write(json_format.MessageToJson(response) + '\n')
        print(response)
        return response

    def send(self, redis):
        channel = grpc.insecure_channel('172.16.155.239:50051')
        stub = redisproxy_pb2_grpc.RedisProxyServiceStub(channel=channel)
        response = stub.RedisProxyCmd(redis)
        return response

    def ping(self):
        command = 'ping'
        response = self.test_command(command)

        res = eval(json_format.MessageToJson(response))
        res = res['oResultBodyList'][0]
        if res["sValue"] == "PONG" and res["nRet"] == 0 and res["nResultType"] == 1 and res["nVer"] == 2:
            print("status: success" + '\n')
        else:
            print("status: failed" + '\n')
        return response

    def set(self):
        command = 'set key_1 1'
        response = self.test_command(command)

        res = eval(json_format.MessageToJson(response))
        res = res['oResultBodyList'][0]
        if res["sValue"] == "OK" and res["nRet"] == 0 and res["nResultType"] == 1 and res["nVer"] == 2:
            print("status: success" + '\n')
        else:
            print("status: failed" + '\n')
        return response

    def get(self):
        command = 'get key_1'
        response = self.test_command(command)

        res = eval(json_format.MessageToJson(response))
        res = res['oResultBodyList'][0]
        if res["sValue"] == "1" and res["nRet"] == 0 and res["nResultType"] == 1 and res["nVer"] == 2:
            print("status: success" + '\n')
        else:
            print("status: failed" + '\n')
        return response

    def exists(self):
        command = 'exists key_1'
        response = self.test_command(command)

        res = eval(json_format.MessageToJson(response))
        res = res['oResultBodyList'][0]
        if res["sValue"] == "1" and res["nRet"] == 0 and res["nResultType"] == 2 and res["nVer"] == 2:
            print("status: success" + '\n')
        else:
            print("status: failed" + '\n')
        return response

    def zadd(self):
        command = 'zadd myzset 2 "world" 3 "bar"" 4 "four" 5 "five" '
        response = self.test_command(command)
        res = eval(json_format.MessageToJson(response))
        res = res['oResultBodyList'][0]
        if res["sValue"] == "1" and res["nRet"] == 0 and res["nResultType"] == 2 and res["nVer"] == 2:
            print("status: success" + '\n')
        else:
            print("status: failed" + '\n')
        return response

    def zcount(self):
        command = 'zcount myzset 1 4'
        response = self.test_command(command)
        res = eval(json_format.MessageToJson(response))
        res = res['oResultBodyList'][0]
        if res["sValue"] == "3" and res["nRet"] == 0 and res["nResultType"] == 2 and res["nVer"] == 2:
            print("status: success" + '\n')
        else:
            print("status: failed" + '\n')
        return response

    def zrangeByScore(self):
        command = 'zrangeByScore myzset 2 3'
        response = self.test_command(command)
        return response

    def zremrangeByScore(self):
        command = 'zremrangeByScore myzset 0 2'
        response = self.test_command(command)

        return response

    def zremrangeByRank(self):
        command = 'zremrangeByRank myzset 2 3'
        response = self.test_command(command)

        return response

    def hset(self):
        command = 'HSET site redis redis.com'
        response = self.test_command(command)

        return response

    def hget(self):
        command = 'HGET site redis'
        response = self.test_command(command)

        return response

    def hmset(self):
        command = 'hmset myhash field1 "hello" field3 5 field2 6.02'
        response = self.test_command(command)

        return response

    def hmget(self):
        command = 'hmget myhash field1 field2'
        response = self.test_command(command)

        return response

    def hgetAll(self):
        command = 'hgetAll myhash'
        response = self.test_command(command)

        return response

    def hdel(self):
        command = 'hdel myhash field1'
        response = self.test_command(command)

        return response

    def hset(self):
        command = 'hset myhash field1 2.1'
        response = self.test_command(command)

        return response

    def hincrBy(self):
        command = 'hincrBy myhash field3 5'
        response = self.test_command(command)

        return response

    def hincrByFloat(self):
        command = 'hincrByFloat myhash field2 2.08'
        response = self.test_command(command)

        return response

    def incrBy(self):
        command = 'incrBy  key_1 5'
        response = self.test_command(command)

        return response

    def setex(self):
        command = 'setex mykey 60 redis'
        response = self.test_command(command)

        return response

    def getTTL(self):
        command = 'ttl mykey'
        response = self.test_command(command)

        return response

    def zrevrangebyscore_withscores(self):
        command = 'zrevrangebyscore myzset +inf -inf withscores'
        response = self.test_command(command)

        return response

    def expire(self):
        command = 'expire key_1 60'
        response = self.test_command(command)

        return response

    def expire(self):
        command = 'expire key_2 60'
        response = self.test_command(command)

        return response


if __name__ == '__main__':
    RedisProxyClient = RedisProxyClient()
    RedisProxyClient.ping()
    RedisProxyClient.set()
    RedisProxyClient.get()
    RedisProxyClient.exists()
    RedisProxyClient.zadd()
    RedisProxyClient.zcount()
    RedisProxyClient.zrangeByScore()
    RedisProxyClient.zremrangeByScore()
    RedisProxyClient.zremrangeByRank()
    RedisProxyClient.hget()
    RedisProxyClient.hmget()
    RedisProxyClient.hgetAll()
    RedisProxyClient.hdel()
    RedisProxyClient.hset()
    RedisProxyClient.hincrBy()
    RedisProxyClient.hincrByFloat()
    RedisProxyClient.incrBy()
    RedisProxyClient.setex()
    RedisProxyClient.getTTL()
    RedisProxyClient.zrevrangebyscore_withscores()
