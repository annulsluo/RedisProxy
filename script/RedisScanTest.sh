#########################################################################
# File Name: RedisScan.sh
# Author: annulsluo
# mail: annulsluo@webank.com
# Created Time: 二 12/17 21:37:52 2019
#########################################################################
#!/bin/bash
# 读取文件中的每行数据值，然后去统计该记录大小
# 循环执行所有业务数据
# 并且把结果输出到统计文件中

patternlist="hash*"
REDIS_CLIENT='/Users/luoshaohua/Documents/GitWork/redis/src/redis-cli -h 127.0.0.1 -p 6379 -a dsp12345'
#staticLog=/data/dmp/redisscanstatic.log
staticLog=/Users/luoshaohua/Documents/WeBankWork/AI项目/RedisProxy/RedisProxyServer/script/Scan/redisscanstatic.log
nottlLog=/Users/luoshaohua/Documents/WeBankWork/AI项目/RedisProxy/RedisProxyServer/script/Scan/nottl.log

function StaticCAP()
{
	running_file_name=$1
	totalcap=0
	cursorttl=0
	cursorindex=0
	while read -r line
	do
		if [ ${cursorindex} -eq 0 ];then
		    cursorindex=1
			continue	
		fi
		#echo "echo debug object ${line} | $REDIS_CLIENT"
		singlecap=`echo "debug object ${line}" | $REDIS_CLIENT | awk -F "[ :]" '{print $9}'`
		totalcap=$((totalcap + singlecap))
		singlettl=`echo "ttl ${line}" | $REDIS_CLIENT | awk -F " " '{print $1}'`
		cursorttl=$((cursorttl + singlettl))
		# 没有过期时间则会打印结果
		if [ ${singlettl} -eq -1 or ${singlettl} -gt  ];then
			echo "${line}" >> $nottlLog
		fi
	done < $running_file_name.cache
	echo $totalcap
	echo $cursorttl
}

function process() {
    echo "processintelargv:"$1 $2 $3
    index=-1
    count=0
    step=100000
    totalsize=0 
	totalserializelen=0
	totalttl=0
	running_file_name=$3
    while ((index!=0))
	do
		if [ $index -le 0 ];then
			index=0
		fi
		echo "scan $index match $1 count $step | $REDIS_CLIENT > $running_file_name.cache"
		echo "scan $index match $1 count $step" | $REDIS_CLIENT > $running_file_name.cache
		read index <<< `head -1 $running_file_name.cache`
		read inc incsize filename <<< `wc -lc $running_file_name.cache`
		echo "index:"$index "inc:"$inc "incsize:"$incsize
		inc=$((inc - 1))
		if [ $? -ne 0 ];then
			break
		fi
		count=$((count + inc))
		totalsize=$((totalsize + incsize - 10))
		returnvalue=$(StaticCAP "$running_file_name" "$1")
		array=(${returnvalue// / })
		tmpcap=${array[0]}
		tmpttl=${array[1]}
		echo "process:returnarray0:"${array[0]},"array1:"${array[1]}
		echo "process totalserializelen begin:"$totalserializelen,$totalttl
		totalserializelen=$((totalserializelen + tmpcap))
		totalttl=$((totalttl + tmpttl))
		echo "process totalserializelen end:"$totalserializelen,$totalttl
    done
	if [ $count -eq 0 ];then
		echo $2 "$1 count:"$count "totalsize:"$((totalsize/1024/1024)) "totalserializelen:"$((totalserializelen/1024/1024)) "avg_ttl:"$((totalttl/86400)) >> $staticLog
	else 
		echo $2 "$1 count:"$count "totalsize:"$((totalsize/1024/1024)) "totalserializelen:"$((totalserializelen/1024/1024)) "avg_ttl:"$((totalttl/count/86400)) >> $staticLog
	fi
}

function main()
{
	currtime=`date +"%Y-%m-%d %H:%M:%S"`
	#echo $currtime
	for pattern in $patternlist
	do
		running_file_name="RedisScan"
		running_flag="run.$running_file_name"
		if [ -f "$running_flag" ] ; then
			echo "$currtime is running..." >> $staticLog
			exit 0
		fi
		echo "running_flag":$running_flag
		touch $running_flag
		echo "processing A:"$pattern
			process "$pattern" "$currtime" "$running_file_name"
		rm -rf $running_flag
	done
	echo "ok!"
}

main

