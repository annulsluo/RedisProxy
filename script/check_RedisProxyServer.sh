#########################################################################
# File Name: check_RedisProxyServer.sh
# Author: annulsluo
# mail: annulsluo@webank.com
# Created Time: 二  1/14 16:35:10 2020
#########################################################################
#!/bin/bash

recivers="annulsluo"
ims_url=http://172.16.40.51:10811/ims_umg_api/send_msg.do
ip=`/sbin/ifconfig -a|grep inet|grep -v 127.0.0.1|grep -v inet6|awk '{print $2}'|tr -d "addr:"`
function message_notice() 
{
     local STAT="WARN"
	 local TITLE="$1"
     local MSG="$2"
     echo "title=${TITLE} [${STAT}]&content=${MSG}&recivers=${recivers}&way=3" 
     curl -s -d "title=${TITLE} [${STAT}]&content=${MSG}&recivers=${recivers}&way=3" $ims_url
}
function main()
{
	pattern="./bin/RedisProxyServer ./conf/RedisProxyServer.conf"
	cnt=`ps -ef | grep "$pattern" | grep -v grep | wc -l`
	BASEPATH='/data/projects'
        cd $BASEPATH
	PID=$BASEPATH/RedisProxyServer/bin/RedisProxyServer.pid
        echo "pid:$PID cnt:$cnt"
	if [ $cnt -gt 1 ]; 
	then 
		ps -ef | grep "$pattern" | grep -v grep | awk -F " " '{print $2}' | xargs kill -9
		if [ -f $PID ];then
			/bin/rm $PID
		fi
		sh /data/projects/RedisProxyServer/bin/RedisProxyServer.sh start
		title="RedisProxyServer $ip 服务重复实例 【重要】"
		message_notice "$title" "请关注！"
	fi

	if [ $cnt -lt 1 ];
	then
		if [ -f $PID ];then
			/bin/rm $PID
		fi
		sh /data/projects/RedisProxyServer/bin/RedisProxyServer.sh start
		title="RedisProxyServer $ip 重启告警【重要】"
		message_notice "$title" "请关注！"
	fi
}

main
