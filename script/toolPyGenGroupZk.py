#!/usr/bin/python
#-*- coding=utf-8 -*-

import sys,os,random
import time,string 
import datetime
#import MySQLdb
#import MySQLdb.cursors

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print "Usage: exe slotnum groupnum appname tablename"
        exit(1)
    InsertTime      = time.strftime('%Y-%m-%d %H:%M:%S',time.localtime(time.time() - 3600))
    print "nInsertTime:%s" % InsertTime
    nSlotNum    = int(sys.argv[1])
    nGroupNum   = int(sys.argv[2])
    appname     = sys.argv[3]
    tablename   = sys.argv[4]
    nBeginSlotId   = 0
    nEndSlotId     = 0
    sGroup2Slot    = ""
    dGroup2slotNum  = {}
    sSlotInfos      = "\"/AIAD-DMP/RedisProxyServer/RedisAppTable/%s_%s/slotinfos\":" % ( appname, tablename )
    print sSlotInfos
    for i in range( 1, nGroupNum+1 ):
        #group_slot=1:1~20,2:21~40,3:41~60,4:61~80,5:81~100,6:101~120,7:121~140,8:141~160:9:161~180,10:181~200;
        nBeginSlotId = nEndSlotId + 1 
        nEndSlotId   = float(nBeginSlotId) + (float(nSlotNum) / nGroupNum) - 1
        sGroup2Slot  = sGroup2Slot + "%d:%d~%d," % ( i, nBeginSlotId, nEndSlotId )
        dGroup2slotNum[i]=(int(nEndSlotId) - int(nBeginSlotId) + 1)
        print "%d:%d~%d,%d" % ( i, nBeginSlotId, nEndSlotId, dGroup2slotNum[i])
    print "{\t\"redisslot\":\"1024\",\r\n\"group_slot\":\"%s\"}," % sGroup2Slot.strip(',')
    # 输入是路径+IP列表+端口号
    # "/AIAD-DMP/RedisProxyServer/RedisAppTable/user_labels/groups/group_1":
    #       {"iid":1,"gid":1,"host":"172.16.155.239:20010","state":"ok","slotnum":170,"ctime":"2020-03-26 17:05:32","lftime":""},
    # 生成具体group信息
    sPath           = "/NGI_PRO/RedisProxyServer/RedisAppTable/%s_%s/groups/group_" % ( appname, tablename )
    sGroupInfo      = ""
    sGroupIpList    = "172.16.154.154,172.16.154.188,172.16.154.162,172.16.154.172,172.16.154.190";
    sPortList       = "10010,10011,10012,10013"
    groups          = sGroupIpList.split(",")
    ports           = sPortList.split(",")
    nPortNum        = len(ports)
    nGroupIpNum     = len(groups)
    nCnt  = 1 
    print "portnum:%d groupipnum:%d" % ( nPortNum, nGroupIpNum )
    for i in range( 0, nPortNum ):
        for j in range( 0, nGroupIpNum ):
            sGroupPath  = "\""+ sPath + str(nCnt) + "\":"
            print sGroupPath
            sGroupInfo  = "\t\t{\"iid\":1," + "\"gid\":"+str(nCnt) + "," + "\"host\":\"%s:%s\"," % ( groups[j], ports[i] ) + "\"state\":\"ok\"," + "\"slotnum\":%d," % (dGroup2slotNum[nCnt]) + "\"ctime\":\"%s\"," % InsertTime + "\"lftime\":\"\"},"
            if nCnt == nGroupNum:
                sGroupInfo = sGroupInfo.strip(',')
            nCnt = nCnt + 1
            print sGroupInfo
    # 生成slot信息
