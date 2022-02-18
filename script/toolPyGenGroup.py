#!/usr/bin/python
#-*- coding=utf-8 -*-

import sys,os,random
import time,string 
import datetime
#import MySQLdb
#import MySQLdb.cursors

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print "Usage: exe slotnum groupnum"
    InsertTime      = time.strftime('%Y-%m-%d %H:%M:%S',time.localtime(time.time() - 3600))
    print "nInsertTime:%s" % InsertTime
    nSlotNum  = int(sys.argv[1])
    nGroupNum = int(sys.argv[2])
    nBeginSlotId   = 0
    nEndSlotId     = 0
    sGroup2Slot    = ""
    for i in range( 1, nGroupNum+1 ):
        #group_slot=1:1~20,2:21~40,3:41~60,4:61~80,5:81~100,6:101~120,7:121~140,8:141~160:9:161~180,10:181~200;
        nBeginSlotId = nEndSlotId + 1 
        nEndSlotId   = float(nBeginSlotId) + (float(nSlotNum) / nGroupNum) - 1
        sGroup2Slot  = sGroup2Slot + "%d:%d~%d," % ( i, nBeginSlotId, nEndSlotId )
        #print "%d:%d~%d" % ( i, nBeginSlotId, nEndSlotId )
    print "%s" % sGroup2Slot.strip(',')
    # 生成具体group信息
    sGroupInfo = ""
    sGroupIpList = "172.16.154.154,172.16.154.188,172.16.154.162,172.16.154.172,172.16.154.190";
    sPortList    = "10010,10011,10012,10013"
    groupiplist    = sGroupIpList.split(",")
    portlist     = sPortList.split(",")
    nPortNum     = len(portlist)
    nGroupIpNum    = len(groupiplist)
    nCnt  = 1
    print "portnum:%d groupipnum:%d" % ( nPortNum, nGroupIpNum )
    for i in range( 0, nPortNum ):
        for j in range( 0, nGroupIpNum ):
            # group_1=172.16.154.5:10010
            sGroupInfo  = "group_" + str(nCnt) + "=" + "%s:%s" % ( groupiplist[j], portlist[i] )
            nCnt = nCnt + 1
            print sGroupInfo
        
    # 生成slot信息
