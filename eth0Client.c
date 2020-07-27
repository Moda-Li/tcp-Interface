#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#include "tcpService.h"
#include "cJSON.h"

#define TCP_HEADER_FILE "ETH0_TEST GET FILE"

#define TCP_HEADER_FILE_RECOVERY "ETH0_TEST GET FILE OK"

#define RECV_WAIT_TIME_MS 5000

unsigned int CheckRecovery(char *recvbuf, int recvsize);

int main(int argc, char *argv[])
{
    if(argc < 6)
    {
        printf("Please input the server ip, prot, file, locapath, GetSize, speed...\r\n");
        printf("./XXXX ip port file loca_path(/xx/xx) bufsize(bytes) speed(ms)\r\n");

        return -1;
    }

    printf("Start to get file ....\r\n");
    printf("    Server ip is: %s ....\r\n", argv[1]);
    printf("    Server port is: %d ....\r\n", atoi(argv[2]));
    printf("    file name: %s ....\r\n", argv[3]);
    char filepath[32] = {0};
    sprintf(filepath, "%s/%s", argv[4], argv[3]);
    printf("    file local path: %s ....\r\n", filepath);
    printf("    Get Size: %s bytes ....\r\n", argv[5]);
    printf("    Delay time: %s (ms) ....\r\n", argv[6]);


    int mSocket = -1;
    char headBuf[32] = {0};
    unsigned char *mRecvBuf = NULL;
    int mRecvBufSize = 1024*1024*10;
    int mGetSize = atoi(argv[5]);
    int mDelayMs = atoi(argv[6]) * 1000;
    int mOffSet = 0;
    unsigned int mFileSize = 0;
    int mRet = 0;

    if(mGetSize > mRecvBufSize)
    {
        printf("mGetSize is too length, over mRecvBufSize %d...\r\n", mRecvBufSize);
        return -1;
    }

    mRecvBuf = (char *)malloc(mRecvBufSize);
    if(mRecvBuf == NULL)
    {
        printf("Bufsize over, system memery no enugh\r\n");
        return -1;
    }

    printf("Start to conect server...\r\n");
    mSocket = tcpSviceConetNormal(argv[1], atoi(argv[2]));
    if(mSocket < 0)
    {
        printf("Server connect error, please check ip, port or other\r\n");
        return -1;
    }
    printf("conect server success...\r\n");
    cJSON *headerInfo = cJSON_CreateObject();
    cJSON_AddItemToObject(headerInfo, "mark", cJSON_CreateString(TCP_HEADER_FILE));
    cJSON_AddItemToObject(headerInfo, "file", cJSON_CreateString(argv[3]));
    char *headerInfoPrt = cJSON_PrintUnformatted(headerInfo);

    printf("TCP_HEADER_FILE: %s\r\n", headerInfoPrt);

    tcpSviceSend(mSocket, headerInfoPrt, strlen(headerInfoPrt), 100, 1);
    free(headerInfoPrt);
    cJSON_Delete(headerInfo);

    mFileSize = CheckRecovery(mRecvBuf, tcpSviceRecv(mSocket, mRecvBuf, mRecvBufSize, mRecvBufSize, RECV_WAIT_TIME_MS, 0));
    if(mFileSize == 0)
    {
        printf("Server recovery error: %s\r\n", mRecvBuf);
        free(mRecvBuf);
        tcpSviceClose(mSocket);
        return -1;
    }
    else
        printf("Get File success, file size %u bytes\r\n", mFileSize);
    

    FILE *locafile = fopen(filepath, "w");
    if(locafile == NULL)
    {
        printf("open file %s failed...\r\n", filepath);
        free(mRecvBuf);
        tcpSviceClose(mSocket);
        return -1;
    }

    char *GetInfoPtr = NULL;
    cJSON *GetFileDtaInfo = cJSON_CreateObject();
    cJSON_AddItemToObject(GetFileDtaInfo, "offset", cJSON_CreateNumber(mOffSet));
    cJSON_AddItemToObject(GetFileDtaInfo, "getsize", cJSON_CreateNumber(mGetSize));

    tcpSviceUsleep(1500*1000);

    int mCurRecvSize = 0;
    mOffSet = 0;
    while(mOffSet < mFileSize)
    {
        cJSON_ReplaceItemInObject(GetFileDtaInfo, "offset", cJSON_CreateNumber(mOffSet));

        GetInfoPtr = cJSON_PrintUnformatted(GetFileDtaInfo);
        printf("GetInfoPtr: %s\r\n", GetInfoPtr);

        mRet = tcpSviceSend(mSocket, GetInfoPtr, strlen(GetInfoPtr), 100, 1);
        if(mRet < 1)
        {
            printf("Happend error: NetWork error...\r\n");
            free(GetInfoPtr);
            continue;
        }

        printf("send success %d bytes...\r\n", mRet);

        memset(mRecvBuf, 0, mRecvBufSize);
    
        mRet = 0;
        mCurRecvSize = 0;
TRY_GET_AGAIN:
        mRet = tcpSviceRecv(mSocket, mRecvBuf, mRecvBufSize, mGetSize, 2000, 1);
        if(mRet > 0)
        {
            mCurRecvSize = mRet;
            printf("recv %d bytes data, need %d\r\n", mCurRecvSize, mGetSize);
        }
        else
        {
            printf("happend error: connect was disconect or select failed\r\n");
            break;
        }
        printf("get file data, start write...\r\n");
        fwrite(mRecvBuf, mCurRecvSize, 1, locafile);
        fflush(locafile);

        mOffSet += mCurRecvSize;
        free(GetInfoPtr);

        tcpSviceUsleep(mDelayMs);
    }

    printf("pro end, mFileSize %d, get size %d...\r\n", mFileSize, mOffSet);
    if(mFileSize == mOffSet)
        printf("^_^ haha..., download file %s success\r\n", filepath);
    else
        printf("!_! ho no..., download file %s failed\t\n", filepath);

    tcpSviceClose(mSocket);
    free(mRecvBuf);
    fclose(locafile);

    return 0;
}

unsigned int CheckRecovery(char *recvbuf, int recvsize)
{
    int ok_len = strlen(TCP_HEADER_FILE_RECOVERY);

    if(recvbuf == NULL || recvsize < 0) // 4 个字节的文件长度
    {
        printf("CheckRecovery: recvbuf %p or recvsize %d error\r\n", recvbuf, recvsize);
        return 0;
    }
    
    cJSON *GetFileDtaInfo = cJSON_Parse(recvbuf);

    if(!strcmp(TCP_HEADER_FILE_RECOVERY, cJSON_GetObjectItem(GetFileDtaInfo, (const char *)"mark")->valuestring))
    {
        return cJSON_GetObjectItem(GetFileDtaInfo, (const char *)"filesize")->valueint;
        cJSON_Delete(GetFileDtaInfo);
    }
    else
    {
        cJSON_Delete(GetFileDtaInfo);
        return 0;
    }
}