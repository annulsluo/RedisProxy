#########################################################################
# File Name: RedisProxyServer.sh
# Author: annulsluo
# mail: annulsluo@gmail.com
# Created Time: 一 12/ 2 10:09:29 2019
#########################################################################

#!/bin/bash

#$0 表示shell命令第一个参数，即脚本本身
#$(dirname $0) 定位脚本文件所在的目录
BASE_DIR=$(dirname $0)
#$(basename $0 .sh) 定位脚本名称，.sh表示去除.sh后缀
PID=$(dirname $0)/$(basename $0 .sh).pid
ip=`/sbin/ifconfig -a|grep inet|grep -v 127.0.0.1|grep -v inet6|awk '{print $2}'|tr -d "addr:"`
port=50051
BASEPATH='/data/projects/RedisProxyServer'
#BASEPATH='/data/dmp/app/RedisProxyServer'

echo "PID:$PID"
start()
{
	#proc_num=$(pgrep -f "./RedisProxyServer ../conf/RedisProxyServer.conf"|wc -l|xargs)
	#if [ ${proc_num} -lt 1 ]
	if [ -f $PID ]
	then
		echo "RedisProxyServer already started"
	else
		cd $BASEPATH
		echo "Starting RedisProxyServer ... "
		export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
		ulimit -c unlimited
		ulimit -SHn 65535
		./bin/RedisProxyServer ./conf/RedisProxyServer.conf $ip $port $BASEPATH &
		 # $? 为上条命令执行结果，成功执行则返回0
		if [[ "$?" -eq 0 ]];
		then 
			# $! 为上条shell命令的进程号，如执行成功，则将进程号写入pid文件
			echo $!>./bin/$PID
		else
			echo "Start RedisProxyServer Fail.Please Check."
		fi
	fi
}

stop()
{
	if [ -f $PID ]
	then 
		echo "Safe Stoping RedisProxyServer ... "
		kill -9 `cat $PID`
		sleep 1 
		/bin/rm $PID
		echo "Stop Succ."
	else
		echo "No Pid File."
	fi
}

case "$1" in
	'start')
		start
		;;
	
	'stop')
		stop
		;;

	'restart')
		stop
		sleep 1
		start
		;;
	*)
		echo "Usage:$0{start|stop|restart}"
		exit 1
		;;
esac

exit 0

