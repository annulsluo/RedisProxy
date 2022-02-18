/*************************************************************************
    > File Name: test_pthread_create.cpp
    > Author: annulsluo
    > Mail: annulsluo@gmail.com
    > Created Time: 五 12/ 6 11:25:54 2019
 ************************************************************************/
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<string>
#include<time.h>
#include<iostream>
#include<unistd.h>
using namespace std;

/* 声明结构体 */
struct member
{
	int nThreadNum;
	int nPipeLineNum;
	int nQps;
	char *sIp;
	char *sPort;
};     

/* 定义线程pthread */
static void * pthread(void *arg)       
{
    struct member *temp;
    
    /* 线程pthread开始运行 */
    printf("pthread start!\n");
    
    /* 令主线程继续执行 */
    //sleep(2);
    
    /* 打印传入参数 */
    temp = (struct member *)arg;      
    printf("member->num:%d\n",temp->nThreadNum);
    printf("member->num:%d\n",temp->nPipeLineNum);
    printf("member->num:%d\n",temp->nQps);
    printf("member->ip:%s\n",temp->sIp);
    printf("member->port:%s\n",temp->sPort);
    
    return NULL;
}

/* main函数 */
int main(int argc,char* argv[])
{
	printf( "argc size:%d\n", argc );
	std::string sIp		= argv[1];
	std::string sPort	= argv[2];
	int nThreadNum		= atoi(argv[3]);
	int nPipeLineNum	= atoi(argv[4]);
	int nQps			= atoi(argv[5]);
	printf( "ip:%s port:%s thread:%d pipe:%d qps:%d\n", sIp.c_str(), sPort.c_str(), nThreadNum, nPipeLineNum, nQps);

	//printf( "time:%d", time(NULL) );
	struct member ** pData = (struct member **)malloc( sizeof( struct member * ) * nThreadNum );
	pthread_t *nTid = (pthread_t*)malloc(sizeof(pthread_t)*nThreadNum );
	printf("init\n");
    /* 为结构体变量b赋值 */
	for( size_t i = 0; i < nThreadNum; ++ i )
	{
		pData[i] = (struct member*)malloc(sizeof(struct member));
		printf("index:%d\n", i );
		pData[i]->nPipeLineNum 	= nPipeLineNum;
		printf("index0:%d\n", i );
		pData[i]->nQps			= nQps;
		pData[i]->nThreadNum 	= nThreadNum;
		pData[i]->sIp		= argv[1];
		pData[i]->sPort		= argv[2];

		printf( "pipelinenum:%d qps:%d threadnum:%d ip:%s port:%s \n", 
				pData[i]->nPipeLineNum, pData[i]->nQps, pData[i]->nThreadNum, pData[i]->sIp, pData[i]->sPort );
		int nErr = pthread_create( &nTid[i], NULL, pthread, (void*)pData[i]);
	}

    /* 线程pthread睡眠2s，此时main可以先执行 */
	sleep(2);
    printf("mian continue!\n");
    
	for( size_t i = 0; i < nThreadNum; ++ i )
	{
		if( pthread_join( nTid[i], NULL ) != 0 )
		{
			printf( "threadid %u is not exit.\n", nTid[i] );
		}
	}

    return 0;
}
