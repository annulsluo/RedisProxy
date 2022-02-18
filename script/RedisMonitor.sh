#########################################################################
# File Name: RedisMonitor.sh
# Author: annulsluo
# mail: annulsluo@webank.com
# Created Time: 一 12/16 22:24:36 2019
#########################################################################
#!/bin/bash

#通过shell 定期执行 redis-cli info 的命令，然后发现某些指标超过阀值就进行告警
#告警 是否支持中文

if [ $# -ne 1 ]; then
	echo "Usage: $0 redisclientpath"
	exit
fi

redisclientpath=$1
ip=`/sbin/ifconfig -a|grep inet|grep -v 127.0.0.1|grep -v inet6|awk '{print $2}'|tr -d "addr:"`
portlist="10010 10011 10012 10013 10014 10020 10030 6379"
receivers="annulsluo"
passwd="dsp12345"
ims_url=http://172.16.40.51:10811/ims_umg_api/send_msg.do

function message_notice() 
{
     local STAT="WARN"
	 local TITLE="$1"
     local MSG="$2"
     echo "title=${TITLE} [${STAT}]&content=${MSG}&recivers=${recievers}&way=3" 
     curl -s -d "title=${TITLE} [${STAT}]&content=${MSG}&recivers=${recievers}&way=3" $ims_url
}

function main()
{
	local MaxConnectClients=0
	local MaxRejectClient=1
	for port in $portlist
	do
		#title="$ip:$port RedisStatusStatic Warning"
		title="$ip:$port Redis 链接告警【重要】"
		message="【服务信息】"
		# 缺少延迟命令
		bIsNeedNotify="false"
		result=`$redisclientpath/redis-cli -h $ip -p $port -a $passwd info`
		echo "${result}" > /tmp/redis_info.txt
		while read -r line 
		do 
			if [[ ${line} =~ ':' ]]; then
				# 删除末尾的\r\n符号，避免拼接message时出现问题
				line=`echo $line| tr -d '\r' | tr -d '\n'`
				array=(${line//:/ })
				if [ ${array[0]} == "redis_version" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "tcp_port" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "connected_clients" ]; then
					message="${message}<br/><br/>【客户端信息】<br/>${array[0]}:${array[1]}"
					if [ ${array[1]} -gt "$MaxConnectClients" ]; then
						bIsNeedNotify="true"
					fi
				elif [ ${array[0]} == "blocked_clients" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "used_momory_human" ]; then
					message="${message}<br/><br/>【内存信息】<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "used_memory_peak_human" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "used_memory_peak_perc" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "used_memory_rss_human" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "maxmemory_human" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "used_memory_overhead" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "used_memory_startup" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "used_memory_dataset" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "used_memory_dataset_perc" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "maxmemory_policy" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "mem_fragmentation_ratio" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "instantaneous_ops_per_sec" ]; then
					message="${message}<br/><br/>【其他统计信息】<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "instantaneous_input_kbps" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "instantaneous_output_kbps" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "rejected_connections" ]; then
					if [ ${array[1]} -gt "$MaxRejectClient" ]; then
						bIsNeedNotify="true"
					fi
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "expired_keys" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "expired_stale_perc" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "evicted_keys" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "keyspace_hits" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "keyspace_misses" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				elif [ ${array[0]} == "db0" ]; then
					message="${message}<br/>${array[0]}:${array[1]}"
				fi 
			fi
			# html 格式
		done < /tmp/redis_info.txt
		echo $ip:$port,${message}
		echo "notify:"${bIsNeedNotify}
		if [ ${bIsNeedNotify} == "true" ]; then
			message_notice "$title" "$message"
		fi
	done
}

main
