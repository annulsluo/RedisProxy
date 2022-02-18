# -*- coding: utf-8 -*-
# @author: v_rcxiao
# @time: 2019/12/23
# Client.py

import grpc
import json
import redisproxy_pb2_grpc
import redisproxy_pb2 as redisproxy__pb2
from google.protobuf import json_format
import base64

def setex():
    redis = redisproxy__pb2.RedisProxyRequest()
    redis.sAppId = 'redis_test'
    redis.sAppPw = 'redis_teset'

    redis.oRequestHead.nCallMode = redisproxy__pb2.CALLMODE_PIPELINE
    redis.oRequestHead.nCallType = redisproxy__pb2.CALLTYPE_SYNC
    oRequestBodyList = redis.oRequestBodyList.add()

    oRequestBodyList.nVer = 3
    oRequestElementList = oRequestBodyList.oRequestElementList
    oelementlist0 = oRequestBodyList.oRequestElementList.add()
    oelementlist0.nCharType = redisproxy__pb2.CHARTYPE_STRING
    oelementlist0.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_CMD
    oelementlist0.sRequestElement = 'setex'

    oelementlist3 = oRequestBodyList.oRequestElementList.add()
    oelementlist3.nCharType = redisproxy__pb2.CHARTYPE_STRING
    oelementlist3.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_TIMEOUT
    oelementlist3.sRequestElement = '3600'

    oelementlist1 = oRequestBodyList.oRequestElementList.add()
    oelementlist1.nCharType = redisproxy__pb2.CHARTYPE_STRING
    oelementlist1.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_KEY
    oelementlist1.sRequestElement = '7ccce0931e6ea709fd68a73480aaef24'
    oelementlist2 = oRequestBodyList.oRequestElementList.add()
    oelementlist2.nCharType = redisproxy__pb2.CHARTYPE_BINARY
    oelementlist2.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_VALUE
    oelementlist2.sRequestElement = base64.b64decode('CnZEEAAAAAACAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAAAAAAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEAAAAAAAAACAAAAAAAgEgEH')
    print( "test: %d %s %s %s." % ( oelementlist0.sRequestElement, oelementlist1.sRequestElement, oelementlist3.sRequestElement, oelementlist2.sRequestElement ))

    channel = grpc.insecure_channel('172.16.155.239:50051')
    stub = redisproxy_pb2_grpc.RedisProxyServiceStub(channel=channel)
    response = stub.RedisProxyCmd(redis)
    print( response )

def get()
    redis = redisproxy__pb2.RedisProxyRequest()
    redis.sAppId = 'redis_test'
    redis.sAppPw = 'redis_teset'

    redis.oRequestHead.nCallMode = redisproxy__pb2.CALLMODE_PIPELINE
    redis.oRequestHead.nCallType = redisproxy__pb2.CALLTYPE_SYNC
    oRequestBodyList = redis.oRequestBodyList.add()

    oRequestBodyList.nVer = 3
    oRequestElementList = oRequestBodyList.oRequestElementList
    oelementlist0 = oRequestBodyList.oRequestElementList.add()
    oelementlist0.nCharType = redisproxy__pb2.CHARTYPE_STRING
    oelementlist0.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_CMD
    oelementlist0.sRequestElement = 'get'

    oelementlist1 = oRequestBodyList.oRequestElementList.add()
    oelementlist1.nCharType = redisproxy__pb2.CHARTYPE_STRING
    oelementlist1.nRequestElementType = redisproxy__pb2.REQUESTELEMENTTYPE_KEY
    oelementlist1.sRequestElement = '7ccce0931e6ea709fd68a73480aaef24'

    channel = grpc.insecure_channel('172.16.155.239:50051')
    stub = redisproxy_pb2_grpc.RedisProxyServiceStub(channel=channel)

    response = stub.RedisProxyCmd(redis)
    res = eval( json_format.MessageToJson( response ) )
    res = res['oResultBodyList'][0]
    print ("test: %s %s." % ( oelementlist0.sRequestElement, oelementlist1.sRequestElement ))
    print( response )
    print (base64.b64encode['sValue'])

if __name__ == '__main__':
    setex()
    get()
