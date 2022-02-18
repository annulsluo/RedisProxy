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

patternlist="378_*	390_* 420_* dsp:recallclick* dsp:recallfreq* request_* freq_* lineItem_* campaign_* customer_* bandit_* geohash_* dsp:pacing:click* dsp:pacing:impressio* dsp:pacing:win* myhash* 100*"
REDIS_CLIENT='/data/dmp/redis-5.0.0/src/redis-cli -h 172.16.155.239 -p 6379 -a dsp12345'
staticLog=/data/dmp/redisscanstatic.log

function StaticCAP()
{
	echo "StaticCap:"$1
	running_file_name=$1
	totalcap=0
	while read -r line
	do
		singlecap=echo "debug object ${line}" | $REDIS_CLIENT | awk -F "[ :]" '{print $9}'
		totalcap=$((totalcap + singlecap))
	done < $running_file_name.cache
	return $totalcap	
}

function process() {
    echo "processintelargv:"$1 $2 $3
    index=-1
    count=0
    step=100000
    totalsize=0 
	totalcap=0
	running_file_name=$3
    while ((index!=0))
    do
      if [ $index -le 0 ];then
        index=0
      fi
      echo "scan $index match $1 count $step | $REDIS_CLIENT > $running_file_name.cache"
      echo "scan $index match $1 count $step" | $REDIS_CLIENT > $running_file_name.cache
	  while read 
      read index <<< `head -1 $running_file_name.cache`
      read inc incsize filename <<< `wc -lc $running_file_name.cache`
      echo "index:"$index "inc:"$inc "incsize:"$incsize
      inc=$((inc - 1))
      if [ $? -ne 0 ];then
        break
      fi
      count=$((count + inc))
      totalsize=$((totalsize + incsize - 10))
	  totalcap=$((totalcap + $(StaticCAP, "$running_file_name)))
    done
    echo $2 "$1 count:"$count "totalsize":$totalsize "totalcap":$totalcap >> $staticLog
}

function main()
{
	currtime=`date +"%Y-%m-%d %H:%M:%S"`
	echo $currtime
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

