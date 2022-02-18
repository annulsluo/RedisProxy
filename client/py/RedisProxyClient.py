# -*- coding: utf-8 -*-
# @author: annulsluo
# @time: 2019/12/23
# Client.py

import grpc
import redisproxy_pb2_grpc
import redisproxy_pb2 as redisproxy__pb2
from google.protobuf import json_format
import datetime
import random
from RedisProxyConsul import RedisProxyConsul 
import time

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
    def __init__(self, appid, apppw, host):
        self.redis  = None 
        self.sAppId = appid
        self.sAppPw = apppw
        self.hosts   = host.split(',')
        self.mul_command = None
        pass
    
    # 每次发送pipeline时需要初始化一次，避免历史数据重复
    def init_pipeline( self ):
        # 1. 实现把命令放到列表的功能
        # 2. 通过遍历列表功能把命令拼接到 redis 结果中
        # 3. 触发执行的pipeline调用的接口
        self.mul_command = []

    def add_pipeline( self, command ):
        self.mul_command.append( command ) 
    
    # 实现请求包构造
    def command(self, command):
        self.redis = redisproxy__pb2.RedisProxyRequest()
        self.redis.sAppId    = self.sAppId
        self.redis.sAppPw    = self.sAppPw
        self.redis.oRequestHead.nCallMode = redisproxy__pb2.CALLMODE_PIPELINE
        self.redis.oRequestHead.nCallType = redisproxy__pb2.CALLTYPE_SYNC
        oRequestBodyList = self.redis.oRequestBodyList.add()

        print('test: ', command)
        oRequestBodyList.sCmd = command

        oRequestBodyList.nVer = 2
        oRequestElementList = oRequestBodyList.oRequestElementList.add()
        oRequestElementList.nCharType = redisproxy__pb2.CHARTYPE_STRING
        response = self.send()
        return response
    
    # 实现类似pipeline的模式
    def send_pipeline(self):
        self.redis = redisproxy__pb2.RedisProxyRequest()
        self.redis.sAppId = self.sAppId
        self.redis.sAppPw = self.sAppPw

        self.redis.oRequestHead.nCallMode = redisproxy__pb2.CALLMODE_PIPELINE
        self.redis.oRequestHead.nCallType = redisproxy__pb2.CALLTYPE_SYNC
        for i in range( len(self.mul_command) ):
            oRequestBodyList = self.redis.oRequestBodyList.add()
            oRequestBodyList.sCmd = self.mul_command[i]
            oRequestBodyList.nVer = 2
            oRequestElementList = oRequestBodyList.oRequestElementList.add()
            oRequestElementList.nCharType = redisproxy__pb2.CHARTYPE_STRING
        response = self.send()
        return response

    def send(self):
        with grpc.insecure_channel(self.hosts[random.randint(0,len(self.hosts)-1)]) as channel:
            stub = redisproxy_pb2_grpc.RedisProxyServiceStub(channel=channel)
            response = stub.RedisProxyCmd(self.redis)
        return response

    def expire(self, key, time):
        command = 'expire %s %s' % (key, time)
        response = self.command(command)
        res = None
        for oResultBody in response.oResultBodyList:
            res = oResultBody.sValue
            break
        return res

    def setnx(self, key, value):
        self.redis = redisproxy__pb2.RedisProxyRequest()
        self.redis.sAppId    = self.sAppId
        self.redis.sAppPw    = self.sAppPw
        self.redis.oRequestHead.nCallMode = redisproxy__pb2.CALLMODE_PIPELINE
        self.redis.oRequestHead.nCallType = redisproxy__pb2.CALLTYPE_SYNC
        oRequestBodyList = self.redis.oRequestBodyList.add()

        oRequestBodyList.nVer = 3 
        oRequestElementList = oRequestBodyList.oRequestElementList
        oelementlist0 = oRequestBodyList.oRequestElementList.add()
        oelementlist0.nCharType = redisproxy__pb2.CHARTYPE_STRING
        oelementlist0.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_CMD
        oelementlist0.sRequestElement = 'setnx'

        oelementlist1 = oRequestBodyList.oRequestElementList.add()
        oelementlist1.nCharType = redisproxy__pb2.CHARTYPE_STRING
        oelementlist1.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_KEY
        oelementlist1.sRequestElement = key 

        oelementlist2 = oRequestBodyList.oRequestElementList.add()
        oelementlist2.nCharType = redisproxy__pb2.CHARTYPE_STRING
        oelementlist2.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_VALUE
        oelementlist2.sRequestElement = value

        response = self.send()
        res = None
        for oResultBody in response.oResultBodyList:
            res = oResultBody.sValue
            break
        return res

    def set(self, ** kw):
        key   = kw["key"]
        value = kw["value"]
        self.redis = redisproxy__pb2.RedisProxyRequest()
        self.redis.sAppId    = self.sAppId
        self.redis.sAppPw    = self.sAppPw
        self.redis.oRequestHead.nCallMode = redisproxy__pb2.CALLMODE_PIPELINE
        self.redis.oRequestHead.nCallType = redisproxy__pb2.CALLTYPE_SYNC
        oRequestBodyList = self.redis.oRequestBodyList.add()

        oRequestBodyList.nVer = 3 
        oRequestElementList = oRequestBodyList.oRequestElementList
        oelementlist0 = oRequestBodyList.oRequestElementList.add()
        oelementlist0.nCharType = redisproxy__pb2.CHARTYPE_STRING
        oelementlist0.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_CMD
        oelementlist0.sRequestElement = 'set'

        oelementlist1 = oRequestBodyList.oRequestElementList.add()
        oelementlist1.nCharType = redisproxy__pb2.CHARTYPE_STRING
        oelementlist1.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_KEY
        oelementlist1.sRequestElement = key 

        oelementlist2 = oRequestBodyList.oRequestElementList.add()
        oelementlist2.nCharType = redisproxy__pb2.CHARTYPE_STRING
        oelementlist2.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_VALUE
        oelementlist2.sRequestElement = value

        response = self.send()
        res = None
        for oResultBody in response.oResultBodyList:
            res = oResultBody.sValue
            break
        return res

    def get(self, key):
        self.redis = redisproxy__pb2.RedisProxyRequest()
        self.redis.sAppId    = self.sAppId
        self.redis.sAppPw    = self.sAppPw
        self.redis.oRequestHead.nCallMode = redisproxy__pb2.CALLMODE_PIPELINE
        self.redis.oRequestHead.nCallType = redisproxy__pb2.CALLTYPE_SYNC
        oRequestBodyList = self.redis.oRequestBodyList.add()

        oRequestBodyList.nVer = 3 
        oRequestElementList = oRequestBodyList.oRequestElementList
        oelementlist0 = oRequestBodyList.oRequestElementList.add()
        oelementlist0.nCharType = redisproxy__pb2.CHARTYPE_STRING
        oelementlist0.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_CMD
        oelementlist0.sRequestElement = 'get'

        oelementlist1 = oRequestBodyList.oRequestElementList.add()
        oelementlist1.nCharType = redisproxy__pb2.CHARTYPE_STRING
        oelementlist1.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_KEY
        oelementlist1.sRequestElement = key 

        response = self.send()
        res = None
        for oResultBody in response.oResultBodyList:
            res = oResultBody.sValue
            break
        return res

    def ttl(self, key):
        command = 'ttl %s' % (key)
        response = self.command(command)
        res = None
        for oResultBody in response.oResultBodyList:
            res = oResultBody.sValue
            break
        return res

    def setex(self, ** kw):
        key   = kw["key"]
        value = kw["value"]
        seconds = kw["seconds"]

        self.redis = redisproxy__pb2.RedisProxyRequest()
        self.redis.sAppId    = self.sAppId
        self.redis.sAppPw    = self.sAppPw
        self.redis.oRequestHead.nCallMode = redisproxy__pb2.CALLMODE_PIPELINE
        self.redis.oRequestHead.nCallType = redisproxy__pb2.CALLTYPE_SYNC
        oRequestBodyList = self.redis.oRequestBodyList.add()

        oRequestBodyList.nVer = 3 
        oRequestElementList = oRequestBodyList.oRequestElementList
        oelementlist0 = oRequestBodyList.oRequestElementList.add()
        oelementlist0.nCharType = redisproxy__pb2.CHARTYPE_STRING
        oelementlist0.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_CMD
        oelementlist0.sRequestElement = 'setex'

        oelementlist1 = oRequestBodyList.oRequestElementList.add()
        oelementlist1.nCharType = redisproxy__pb2.CHARTYPE_STRING
        oelementlist1.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_KEY
        oelementlist1.sRequestElement = key 

        oelementlist3 = oRequestBodyList.oRequestElementList.add()
        oelementlist3.nCharType = redisproxy__pb2.CHARTYPE_STRING
        oelementlist3.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_TIMEOUT
        oelementlist3.sRequestElement = seconds

        oelementlist2 = oRequestBodyList.oRequestElementList.add()
        oelementlist2.nCharType = redisproxy__pb2.CHARTYPE_STRING
        oelementlist2.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_VALUE
        oelementlist2.sRequestElement = value

        response = self.send()
        res = None
        for oResultBody in response.oResultBodyList:
            res = oResultBody.sValue
            break
        return res

if __name__ == '__main__':
    appid   = "redis_test"
    apppw   = "redis_teset"
    host    = "172.16.155.239:50051,172.16.155.210:50051"
    RedisProxyClient = RedisProxyClient( appid, apppw, host )
    print(RedisProxyClient.set(key="test1",value="test_value_1"))
    print(RedisProxyClient.get(key="test1"))

    print(RedisProxyClient.setex(key="test3", value="test_value_2", seconds="3600"))
    print(RedisProxyClient.get(key="test3"))
    print(RedisProxyClient.ttl(key="test3"))
    print(RedisProxyClient.setnx(key="test4", value="test_value_4"))
    print(RedisProxyClient.get(key="test4"))
    '''
    RedisProxyClient.init_pipeline()
    RedisProxyClient.add_pipeline( "set cli_%d value_%d" % ( random.randint(0,10),random.randint(0,10) ) )
    RedisProxyClient.add_pipeline( "setex cli_2 3600 value_2" )
    print(RedisProxyClient.send_pipeline())

    RedisProxyClient.init_pipeline()
    RedisProxyClient.add_pipeline( "set cli_3 value_3" )
    RedisProxyClient.add_pipeline( "setex cli_4 3600 cli_value_4" )
    print(RedisProxyClient.send_pipeline())
    '''
