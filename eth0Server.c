#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#include "tcpService.h"
#include "cJSON.h"

int CheckRecovery(char *recvbuf, int recvsize, char *filename);
unsigned int GetFileSzie(FILE *fileprt);


#define TCP_HEADER_FILE "ETH0_TEST GET FILE"
#define TCP_HEADER_FILE_RECOVERY "ETH0_TEST GET FILE OK"

#define HEADRTSIZE 1024
#define RECV_WAIT_TIME_MS 5000

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("Please input the port for server...\r\n");
        return -1;
    }
    unsigned char *sendBuf = (unsigned char *)malloc(1024*1024*10);

    int mSocket;
    int mCliSock;
    int mRet;

    mSocket = socket(AF_INET, SOCK_STREAM, 0); //创建套接字
    if(mSocket < 0)
    {
        printf("socket error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;                   //协议族
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);    //地址, INADDR_ANY为系统宏
    servaddr.sin_port = htons(atoi(argv[1]));                 //端口号, 通常大于1024

    mRet = bind(mSocket, (struct sockaddr*)&servaddr, sizeof(servaddr));    //绑定
    if(mRet < 0)
    {
        printf("bind socket error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }

TRY_AGAIN_LISTEN:
    printf("\r\nStart to listen client...\r\n");
    mRet = listen(mSocket, 10);    //监听
    if(mRet < 0)
    {
        printf("listen socket error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    struct sockaddr_in cliaddr;
    int addrlen;
    mCliSock = accept(mSocket, (struct sockaddr*)&cliaddr, &addrlen);    //接收
    if(mCliSock < 0)
    {
        printf("accept socket error: %s(errno: %d)", strerror(errno), errno);
        return -1;
    }

    printf("have a new client connect, ip is: %s, port: %d\r\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

    unsigned char headBuf[HEADRTSIZE] = {0};
    char fileName[32] = {"./"};
    mRet = CheckRecovery(headBuf, tcpSviceRecv(mCliSock, headBuf, HEADRTSIZE, HEADRTSIZE, RECV_WAIT_TIME_MS, 0), fileName);
    if(mRet)
    {
        printf("Client request error: %s\r\n", headBuf);
        tcpSviceClose(mCliSock);
        goto TRY_AGAIN_LISTEN;
    }

    printf("get file name: %s\r\n", fileName);
    FILE *filePrt = fopen(fileName, "r");
    if(filePrt == NULL)
    {
        printf("file %s is no exist...\r\n", fileName);
        tcpSviceClose(mCliSock);
        goto TRY_AGAIN_LISTEN;
    }

    unsigned int fileSize = GetFileSzie(filePrt);
    printf("get the file size is: %d bytes\r\n", fileSize);

    cJSON *GetFileDtaInfo = cJSON_CreateObject();
    cJSON_AddItemToObject(GetFileDtaInfo, "mark", cJSON_CreateString(TCP_HEADER_FILE_RECOVERY));
    cJSON_AddItemToObject(GetFileDtaInfo, "filesize", cJSON_CreateNumber(fileSize));
    char *headerRecoverInfo = cJSON_PrintUnformatted(GetFileDtaInfo);

    tcpSviceSend(mCliSock, headerRecoverInfo, strlen(headerRecoverInfo), 100, 1);
    free(headerRecoverInfo);
    cJSON_Delete(GetFileDtaInfo);
    GetFileDtaInfo = NULL;
    
    int offset = 0;
    int getsize = 0;
    printf("Start transfrom data...\r\n");
    while(1)
    {
        memset(headBuf, 0, HEADRTSIZE);
        mRet = tcpSviceRecv(mCliSock, headBuf, HEADRTSIZE, HEADRTSIZE, RECV_WAIT_TIME_MS, 0);
        //mRet = recv(mCliSock, headBuf, HEADRTSIZE, 0);
        if(mRet < 0)
        {
            printf("client happend error...\r\n");
            fclose(filePrt);
            goto TRY_AGAIN_LISTEN;
        }
        printf("get data %s, start parse...\r\n", headBuf);
        GetFileDtaInfo = cJSON_Parse(headBuf);
        if(GetFileDtaInfo == NULL)
        {
            printf("parse error, end....");
            fclose(filePrt);
            goto TRY_AGAIN_LISTEN;
        }
        offset = cJSON_GetObjectItem(GetFileDtaInfo, (const char *)"offset")->valueint;
        getsize = cJSON_GetObjectItem(GetFileDtaInfo, (const char *)"getsize")->valueint;
        memset(sendBuf, 0, 1024*1024*10);
        fseek(filePrt, offset, SEEK_SET);
        int alyread = fread(sendBuf, 1, getsize, filePrt);
        mRet = tcpSviceSend(mCliSock, sendBuf, alyread, 100, 1);
        printf("server send %d bytes\r\n", mRet);
    }

    return 0;
}

int CheckRecovery(char *recvbuf, int recvsize, char *filename)
{
    int ok_len = strlen(TCP_HEADER_FILE);

    if(recvbuf == NULL || recvsize < ok_len)
    {
        printf("CheckRecovery: recvbuf %p or recvsize %d error\r\n", recvbuf, recvsize);
        return -1;
    }

    cJSON *GetFileDtaInfo = NULL;
    GetFileDtaInfo = cJSON_Parse(recvbuf);
    if(GetFileDtaInfo == NULL)
    {
        printf("CheckRecovery: parse error");
        return -1;
    }
    printf("%s\r\n", cJSON_GetObjectItem(GetFileDtaInfo, (const char *)"mark")->valuestring);
    if(!strcmp(TCP_HEADER_FILE, cJSON_GetObjectItem(GetFileDtaInfo, (const char *)"mark")->valuestring))
    {
        strcpy(filename + 2, cJSON_GetObjectItem(GetFileDtaInfo, (const char *)"file")->valuestring);
        printf("CheckRecovery: get file name: %s\r\n", filename);
        return 0;
    }
    else
        return -1;
}

unsigned int GetFileSzie(FILE *fileprt)
{
    unsigned int CurReadPos = ftell(fileprt);
    unsigned int fileSize = -1;
    fseek(fileprt, 0, SEEK_END);
    //获取文件的大小
    fileSize = ftell(fileprt);
    //恢复文件原来读取的位置
    fseek(fileprt, CurReadPos, SEEK_SET);
    return fileSize;
}