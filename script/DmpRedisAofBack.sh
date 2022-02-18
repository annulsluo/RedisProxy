#########################################################################
# File Name: DmpRedisAofBack.sh
# Author: annulsluo
# mail: annulsluo@gmail.com
# Created Time: 四  4/23 15:13:42 2020
#########################################################################
#!/bin/bash

ip=`/sbin/ifconfig -a|grep inet|grep -v 127.0.0.1|grep -v inet6|awk '{print $2}'|tr -d "addr:"`
portlist="10010 10011 10012 10013 10014"
receivers="annulsluo"
ims_url=http://172.16.40.51:10811/ims_umg_api/send_msg.do
nofile="No such file or directory"

function message_notice() 
{
     local STAT="WARN"
	 local TITLE="$1"
     local MSG="$2"
     echo "title=${TITLE} [${STAT}]&content=${MSG}&recivers=${recievers}&way=3" 
     curl -s -d "title=${TITLE} [${STAT}]&content=${MSG}&recivers=${recievers}&way=3" $ims_url
}

function BackUp()
{
	echo "Start BackUp DmpRedisAof ... "
	for port in $portlist
	do
		if [ $port == "10010" ]; then
			mv /data/projects/redis-10010/db/appendonly.aof /data/projects/redis-10010/db/appendonly.aof.bak; 
		elif [ $port \> "10010" ]; then
			index=${port: -1}
			mv /data$index/projects/redis-$port/db/appendonly.aof /data$index/projects/redis-$port/db/appendonly.aof.bak; 
		fi
	done
	echo "End BackUp DmpRedisAof ... "
}

function RollBack()
{
	echo "Start RollBack DmpRedisAof ... "
	for port in $portlist
	do
		if [ $port == "10010" ]; then
			mv /data/projects/redis-10010/db/appendonly.aof.bak /data/projects/redis-10010/db/appendonly.aof; 
		elif [ $port \> "10010" ]; then
			index=${port: -1}
			mv /data$index/projects/redis-$port/db/appendonly.aof.bak /data$index/projects/redis-$port/db/appendonly.aof; 
		fi
	done
	echo "End RollBack DmpRedisAof ... "
}

function ReStart()
{
	echo "Start Redis ReStart ... "
	for port in $portlist
	do
		ps -ef | grep "/data/projects/redis-$port/src/redis-server" | grep -v grep | awk -F " " '{print $2}' | xargs kill -9;
		/data/projects/redis-$port/src/redis-server /data/projects/redis-$port/bin/user_profile_redis.conf;
		sleep 3s	
	done
	echo "End Redis ReStart ... "
}

function Start()
{
	echo "Start Redis start... "
	for port in $portlist
	do
		/data/projects/redis-$port/src/redis-server /data/projects/redis-$port/bin/user_profile_redis.conf;
		sleep 3s	
	done
	echo "End Redis start... "
}

function Stop()
{
	echo "Start Redis Stop... "
	for port in $portlist
	do
		ps -ef | grep "/data/projects/redis-$port/src/redis-server" | grep -v grep | awk -F " " '{print $2}' | xargs kill -9;
	done
	echo "End Redis Stop... "
}

function CheckRedis()
{
	echo "Start Redis Stop... "
	for port in $portlist
	do
		cnt=$(ps -ef | grep "/data/projects/redis-$port/src/redis-server" | grep -v grep | awk -F " " '{print $2}' | wc -l);
		if [ $cnt -lt 1 ]; then
			echo "error redis-$port not run. please check."
		fi
	done
	echo "End Redis Stop... "
}

function CheckBackUp()
{
	echo "Start CheckBackUp DmpRedisAof ... "
	for port in $portlist
	do
		local index=0
		if [ $port == "10010" ]; then
			cnt=$(ls -lrht /data/projects/redis-10010/db/appendonly.aof.bak | wc -l); 
		elif [ $port \> "10010" ]; then
			index=${port: -1}
			cnt=$(ls -lrht /data$index/projects/redis-$port/db/appendonly.aof.bak | wc -l);
		fi
		if [ $cnt -lt 1 ]; then
			echo "error /data$index/projects/redis-$port/db/appendonly.aof.bak no exist. Please check it!"
		fi
	done
	echo "End BackUp DmpRedisAof ... "
}

function CheckRollBack()
{
	echo "Start CheckRollBack DmpRedisAof ... "
	for port in $portlist
	do
		local index=0
		if [ $port == "10010" ]; then
			cnt=$(ls -lrht /data/projects/redis-10010/db/appendonly.aof | wc -l); 
		elif [ $port \> "10010" ]; then
			index=${port: -1}
			cnt=$(ls -lrht /data$index/projects/redis-$port/db/appendonly.aof | wc -l)
		fi
		if [ $cnt -lt 1 ]; then
			echo "error /data$index/projects/redis-$port/db/appendonly.aof no exist. Please check it!"
		fi
	done
	echo "End CheckRollBack DmpRedisAof ... "
}


function main()
{
	case "$1" in
		'backup')
			BackUp	
			;;
		
		'rollback')
			RollBack	
			;;

		'restart')
			ReStart
			;;

		'start')
			Start	
			;;

		'stop')
			Stop	
			;;

		'checkredis')
			CheckRedis	
			;;

		'checkbackup')
			CheckBackUp	
			;;

		'checkrollback')
			CheckRollBack	
			;;
		*)
			echo "Usage:$0{backup|rollback|restart|stop|checkredis|checkbackup|checkrollback}"
			exit 1
			;;
	esac
}

main $1
