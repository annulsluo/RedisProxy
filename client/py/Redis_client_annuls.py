# -*- coding: utf-8 -*-
# @author: v_rcxiao
# @time: 2019/12/23
# Client.py

import grpc
import redisproxy_pb2_grpc
import redisproxy_pb2 as redisproxy__pb2
from google.protobuf import json_format
import datetime


def SerializeToString(redis):
    res = redis.SerializeToString()
    print(res)
    return res


def ParseFromString(res):
    Redis = redisproxy__pb2.RedisProxyRequest()
    Redis.ParseFromString(res)
    p = json_format.MessageToJson(Redis)
    print(p)
    return p


class RedisProxyClient():
    def __init__(self):
        pass

    def test_command(self, command):
        redis = redisproxy__pb2.RedisProxyRequest()
        redis.sAppId = 'aiad_dsp'
        redis.sAppPw = '6ghb3km9gLoozeYk'

        redis.oRequestHead.nCallMode = redisproxy__pb2.CALLMODE_PIPELINE
        redis.oRequestHead.nCallType = redisproxy__pb2.CALLTYPE_SYNC
        oRequestBodyList = redis.oRequestBodyList.add()

        print('test: ', command)
        oRequestBodyList.sCmd = command

        oRequestBodyList.nVer = 2
        oRequestElementList = oRequestBodyList.oRequestElementList.add()
        oRequestElementList.nCharType = redisproxy__pb2.CHARTYPE_STRING
        # nRequestElementType = oRequestElementList.nRequestElementType.add()
        response = self.send(redis)
        return response

    def test_mul_command(self, mul_command):
        redis = redisproxy__pb2.RedisProxyRequest()
        redis.sAppId = 'aiad_dsp'
        redis.sAppPw = '6ghb3km9gLoozeYk'

        redis.oRequestHead.nCallMode = redisproxy__pb2.CALLMODE_PIPELINE
        redis.oRequestHead.nCallType = redisproxy__pb2.CALLTYPE_SYNC
        for i in range( len(mul_command) ):
            oRequestBodyList = redis.oRequestBodyList.add()
            print('index: %d command: %s' % ( i, mul_command[i] ) )
            oRequestBodyList.sCmd = mul_command[i]
            oRequestBodyList.nVer = 2
            oRequestElementList = oRequestBodyList.oRequestElementList.add()
            oRequestElementList.nCharType = redisproxy__pb2.CHARTYPE_STRING

        # nRequestElementType = oRequestElementList.nRequestElementType.add()
        response = self.send(redis)
        return response


    def send(self, redis):
        #channel = grpc.insecure_channel('172.16.155.210:50051')
        channel = grpc.insecure_channel('172.16.155.239:50051')
        #channel = grpc.insecure_channel('10.107.116.42:50051')
        stub = redisproxy_pb2_grpc.RedisProxyServiceStub(channel=channel)
        response = stub.RedisProxyCmd(redis)
        return response

    def ping(self):
        command = 'ping'
        response = self.test_command(command)
        print(response)
        return response

    def set(self):
        command = 'set key1 1'
        response = self.test_command(command)
        print(response)
        return response

    def get(self):
        command = '' 
        response = self.test_command(command)
        print(response)
        return response

    def exists(self):
        command = 'exists notexistkey'
        response = self.test_command(command)
        print(response)
        return response

    def zadd(self):
        command = 'zadd myzset1 2 "world" 3 "bar" 4 "four" 5 "five" '
        response = self.test_command(command)
        print(response)
        return response

    def zcount(self):
        command = 'zcount myzset1 1 4'
        response = self.test_command(command)
        print(response)
        return response

    def zrangeByScore(self):
        command = 'zrange sortset_741801 0 -1'
        response = self.test_command(command)
        print(response)
        return response

    def zremrangeByScore(self):
        command = 'zremrangeByScore myzset1 0 2'
        response = self.test_command(command)
        print(response)
        return response

    def zremrangeByRank(self):
        command = 'zremrangeByRank myzset1 2 3 WITHSCORES '
        response = self.test_command(command)
        print(response)
        return response

    def hget(self):
        command = 'hget myhash field1'
        response = self.test_command(command)
        print(response)
        return response

    def hmget(self):
        command = 'hmget request_8fDstOqeWPQ4E_dP4VSbY6z7m7Q customerid didMd5'
        response = self.test_command(command)
        print(response)
        return response

    def hgetAll(self):
        mul_command = ['hgetAll bandit_73_Feed_07998be472ae13c6f013ce925d576306','hgetAll bandit_73_Feed_f3c10d96b718a96f292353439d461cd8', 'hgetAll bandit_73_Feed_cc93568dd7a8cdf717989c994227ec5b']
        start_ms = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')
        response = self.test_mul_command(mul_command)
        end_ms = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')
        print( "starttime:%s endtime:%s" % (start_ms,end_ms ))
        print(response)
        return response

    def hdel(self):
        command = 'hdel myzset1 bar four field2'
        response = self.test_command(command)
        print(response)
        return response

    def hset(self):
        command = 'hset myhash field1 2.1'
        response = self.test_command(command)
        print(response)
        return response

    def hmset(self):
        command = 'hmset myhash field3 5 field2 6.02'
        response = self.test_command(command)
        print(response)
        return response

    def hincrBy(self):
        command = 'hincrBy myhash field3'
        response = self.test_command(command)
        print(response)
        return response

    def hincrByFloat(self):
        command = 'hincrByFloat myhash field2 2.08'
        response = self.test_command(command)
        print(response)
        return response

    def incrBy(self):
        command = 'incrBy  field3 5'
        response = self.test_command(command)
        print(response)
        return response

    def setex(self):
        command = 'setex mykey 60 redis'
        response = self.test_command(command)
        print(response)
        return response

    def getTTL(self):
        command = 'ttl mykey'
        response = self.test_command(command)
        print(response)
        return response

    def zrangebyscore_withscores(self):
        command = 'zrange sortset_741801 0 -1 withscores'
        response = self.test_command(command)
        print(response)
        return response

    def zrevrangebyscore_withscores(self):
        command = 'zrevrangebyscore sortset_741801 +inf -inf withscores'
        response = self.test_command(command)
        print(response)
        return response


if __name__ == '__main__':
    RedisProxyClient = RedisProxyClient()
    #RedisProxyClient.ping()
    #RedisProxyClient.set()
    #RedisProxyClient.get()
    #RedisProxyClient.zadd()
    #RedisProxyClient.zrangeByScore()
    #RedisProxyClient.zrangebyscore_withscores()
    #RedisProxyClient.hget()
    #RedisProxyClient.get()
    #RedisProxyClient.exists()
    #RedisProxyClient.hset()
    RedisProxyClient.hgetAll()
    #RedisProxyClient.hmget()
    #RedisProxyClient.hmget()
    '''
    RedisProxyClient.zcount()
    RedisProxyClient.zremrangeByScore()
    RedisProxyClient.zremrangeByRank()
    RedisProxyClient.hmget()
    RedisProxyClient.hdel()
    RedisProxyClient.hset()
    RedisProxyClient.hincrBy()
    RedisProxyClient.hincrByFloat()
    RedisProxyClient.incrBy()
    RedisProxyClient.setex()
    RedisProxyClient.getTTL()
    RedisProxyClient.zrevrangebyscore_withscores()
    '''
