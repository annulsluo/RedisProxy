# -*- coding: utf-8 -*-
# @author: v_rcxiao
# @time: 2019/12/23
# Client.py

import grpc
import json
import redisproxy_pb2_grpc
import redisproxy_pb2 as redisproxy__pb2
from google.protobuf import json_format


def RedisProxyClient():

    redis = redisproxy__pb2.RedisProxyRequest()
    redis.sAppId = 'redis_test'
    redis.sAppPw = 'redis_teset'
    redis.sAppName = 'aiad'
    redis.sTableName = 'userprofile'

    redis.oRequestHead.nCallMode = redisproxy__pb2.CALLMODE_PIPELINE
    redis.oRequestHead.nCallType = redisproxy__pb2.CALLTYPE_SYNC
    oRequestBodyList = redis.oRequestBodyList.add()

    oRequestBodyList.nVer = 3
    oRequestElementList = oRequestBodyList.oRequestElementList
    oelementlist0 = oRequestBodyList.oRequestElementList.add()
    oelementlist0.nCharType = redisproxy__pb2.CHARTYPE_STRING
    oelementlist0.sRequestElement = 'set'

    oelementlist1 = oRequestBodyList.oRequestElementList.add()
    oelementlist1.nCharType = redisproxy__pb2.CHARTYPE_STRING
    oelementlist1.sRequestElement = 'key_3'

    # oelementlist2 = oRequestBodyList.oRequestElementList.add()
    # oelementlist2.nCharType = redisproxy__pb2.CHARTYPE_STRING
    # oelementlist2.sRequestElement = 'redis'
    #
    oelementlist3 = oRequestBodyList.oRequestElementList.add()
    oelementlist3.nCharType = redisproxy__pb2.CHARTYPE_BINARY
    oelementlist3.sRequestElement = '不知道'.encode()

    # oelementlist4 = oRequestBodyList.oRequestElementList.add()
    # oelementlist4.nCharType = redisproxy__pb2.CHARTYPE_STRING
    # oelementlist4.sRequestElement = '4'
    #
    # oelementlist5 = oRequestBodyList.oRequestElementList.add()
    # oelementlist5.nCharType = redisproxy__pb2.CHARTYPE_BINARY
    # oelementlist5.sRequestElement = '是'.encode()



    channel = grpc.insecure_channel('172.16.155.239:50051')
    stub = redisproxy_pb2_grpc.RedisProxyServiceStub(channel=channel)
    response = stub.RedisProxyCmd(redis)
    return response
if __name__ == '__main__':
    response = RedisProxyClient()
    print(response)
    res = eval(json_format.MessageToJson(response))
    res = res['oResultBodyList'][0]
    print("判断输入和输出的中文字符是否符合：")
    print(eval(res["sValue"]).decode())