#!/usr/bin/python
#-*- coding=utf-8 -*-

import sys,os,random
import time,string 
import datetime
import redis
from RedisProxyClient import RedisProxyClient

if __name__ == "__main__":
    redis_cli   = redis.Redis(host="172.16.154.33", port="6379", password="mhxs1688hjlh")
    redisproxy_cli  = RedisProxyClient( "aiad_caixin_user_history", "UhGDIayBjeljslNi", host="172.16.154.51:50051" )
    idx         = -1
    keycount    = 0 
    patterns     = ['*_user_profile_*','*_article_status_*']
    for pattern in patterns:
        while True:
            idx, keys = redis_cli.scan( idx, pattern, 10000 )
            if idx == 0:
                break
            for key in keys:
                try:
                    value = redis_cli.get( name=key )
                    keycount = keycount + 1
                    redisproxy_cli.set( key=key, value=value )
                except Exception as msg:
                    print( "error msg:%s" % msg )
            time.sleep(1)
        print( "parttern :%s keycount:%d" % ( pattern, keycount ) )
    
