/*************************************************************************
    > File Name: xiaoshuai.c
    > Author: xiaoshuai
    > Company: gmengshuai.xydh.fun 
    > Created Time: 2021年09月13日 星期一 10时10分10秒
 ************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>



int main(int argc,char *argv[])
{

    union {
        char name[3];
        int xname;
    }un;
    strcpy(un.name,"帅");
    int i;
    for(i = 0; i < 3; i ++){
        printf("%x\n",un.name[i]);
    }
    printf("xname is %x\n",un.xname);
    return 0;
}

