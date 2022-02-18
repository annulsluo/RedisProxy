/*************************************************************************
    > File Name: test_pthread_create.cpp
    > Author: annulsluo
    > Mail: annulsluo@webank.com
    > Created Time: 五 12/ 6 11:25:54 2019
 ************************************************************************/
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<string>
#include<time.h>
#include<iostream>
using namespace std;

/* 声明结构体 */
struct member
{
    int num;
    char *name;
	char *sIp;
	char *sPort;
	//string sIp;
	//string sPort;
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
    printf("member->num:%d\n",temp->num);
    printf("member->name:%s\n",temp->name);
    printf("member->ip:%s\n",temp->sIp);
    printf("member->port:%s\n",temp->sPort);
    
    return NULL;
}

/* main函数 */
int main(int agrc,char* argv[])
{
	pthread_t tidp;
    struct member *b;

	printf( "time:%d", time(NULL) );
    /* 为结构体变量b赋值 */
    b = (struct member *)malloc(sizeof(struct member));           
    b->num=1;
    b->name="mlq";              
	b->sIp="127.0.0.1";
	b->sPort="5051";

    /* 创建线程pthread */
    if ((pthread_create(&tidp, NULL, pthread, (void*)b)) == -1)
    {
        printf("create error!\n");
        return 1;
    }
    
    /* 令线程pthread先运行 */
    //sleep(1);
    
    /* 线程pthread睡眠2s，此时main可以先执行 */
    printf("mian continue!\n");
    
    /* 等待线程pthread释放 */
    if (pthread_join(tidp, NULL))                  
    {
        printf("thread is not exit...\n");
        return -2;
    }
    
    return 0;
}
