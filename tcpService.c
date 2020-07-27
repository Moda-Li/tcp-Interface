#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>


#include "tcpService.h"

/**
 * 函数名：tcpviceConet
 * 功能：连接服务器, 非阻塞模式
 * 参数：
 *      serIp：服务器地址
 *      serPort：服务器端口
 *      waitMs：select等待时间, 单位 ms
 * 返回值：
 *      > 0：socket套接字
 *      < 0：连接服务器失败
*/
int tcpSviceConet(char *serIp, unsigned short serPort, int waitMs)
{
    int mSocket = -1;

    struct  sockaddr_in mService = {};
    struct  timeval mWaitim;
    fd_set  mReadSet, mWriteSet;

    socklen_t mLen = sizeof(int);
    int     mError = -1;

    int mRet = 0;
    int mFlag = 0;

    mSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(mSocket < 0)
    {
        printf("socket error:%d %s\n", errno, strerror(errno));
        return -1;
    }


    memset(&mService, 0, sizeof(struct sockaddr_in));
    mService.sin_family = AF_INET;
    mService.sin_port = htons(serPort);
    if(inet_pton(AF_INET, serIp, &mService.sin_addr) <= 0)
    {
        printf("ip:%s inet_pton error:%d %s\n", serIp, errno, strerror(errno));
        close(mSocket);
        return -1;
    }

    /* set no block O_NONBLOCK */
    mFlag = fcntl(mSocket, F_GETFL, 0);
    mRet = fcntl(mSocket, F_SETFL, (unsigned int)mFlag | O_NONBLOCK);
    if(mRet == -1)
    {
        close(mSocket);
        return -1;
    }

    /*Connect to server.*/
    mRet = connect(mSocket, (struct sockaddr *)&mService, sizeof(mService));
    if(mRet == -1)
    {
        if(errno != EINPROGRESS)
        {
            printf("connect error:%d %s\n", errno, strerror(errno));
            close(mSocket);
            return -1;
        }

        mWaitim.tv_sec = waitMs/1000;
        mWaitim.tv_usec = waitMs%1000;
        FD_ZERO(&mReadSet);
        FD_ZERO(&mWriteSet);
        FD_SET(mSocket, &mReadSet);
        FD_SET(mSocket, &mWriteSet);

        mRet = select(mSocket + 1, &mReadSet, &mWriteSet, NULL, &mWaitim);
        if (mRet < 0) /* select调用失败 */
        {
            printf("connect error: %s\r\n", strerror(errno));
            mRet = -1;
        }
        /*连接超时*/
        if (mRet == 0) 
        {
            printf("Connect %s:%d timeout.\r\n", serIp, serPort);
            mRet = -1;
        }
        /*[1] 当连接成功建立时，描述符变成可写,fdflags=1*/
        if (mRet == 1 && FD_ISSET(mSocket, &mWriteSet)) 
        {
            mRet = 0;
        }
        /*[2] 当连接建立遇到错误时，描述符变为即可读，也可写，rc=2 遇到这种情况，可调用getsockopt函数*/
        else if (mRet == 2) 
        {
            if (getsockopt(mSocket, SOL_SOCKET, SO_ERROR, &mError, &mLen) == -1) 
            {
                printf("getsockopt(SO_ERROR): %s\r\n", strerror(errno));
                mRet = -1;
            }
            else if (mError) 
            {
                errno = mError;
                printf("connect error: %s\r\n", strerror(errno));
                mRet = -1;	         
            }
            else
            {
                mRet = 0;
            }
        }
    }

    /*set to block*/
#if 0
    mFlag &= ~O_NONBLOCK;
    mRet = fcntl(mSocket, F_SETFL, mFlag);
    if(mRet == -1)
    {
        close(mSocket);
        return -1;
    }
#endif

    if(mRet != 0)
    {
        close(mSocket);
        return -1;
    }

    return mSocket;
}


/**
 * 函数名：tcpviceConetNormal
 * 功能：连接服务器, 阻塞模式
 * 参数：
 *      serIp：服务器地址
 *      serPort：服务器端口
 *      waitMs：select等待时间, 单位 ms
 * 返回值：
 *      > 0：socket套接字
 *      < 0：连接服务器失败
*/
int tcpSviceConetNormal(char *serIp, unsigned short serPort)
{
    /* Create a SOCKET for connecting to server */
    int mSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(mSocket < 0)
    {
        printf("socket error:%d %s\n", errno, strerror(errno));
        return -1;
    }

    struct  sockaddr_in mServer = {};
    memset(&mServer, 0, sizeof(struct sockaddr_in));
    mServer.sin_family = AF_INET;
    mServer.sin_port = htons(serPort);
    if(inet_pton(AF_INET, serIp, &mServer.sin_addr) <= 0)
    {
        printf("ip:%s inet_pton error:%d %s\n", serIp, errno, strerror(errno));
        close(mSocket);
        return -1;
    }

    int mRet = connect(mSocket, (struct sockaddr*)&mServer, sizeof(mServer));
    if(mRet < 0)
    {
        printf("connect error:%d %s\n", errno, strerror(errno));
        close(mSocket);
        return -1;
    }

    return mSocket;
}

/**
 * 函数名：tcpSviceSend
 * 功能：tcp发送数据
 * 参数：
 *      socket：套接字
 *      sendBuf：发送地址
 *      sendSize：发送长度
 *      waitMs：select可写等待时间
 *      trySendTims：重发尝试次数
 * 返回值：
 *      > 0：成功发送的长度
 *      = -1：selet监听失败或超时
 *      = -2：socket发送出错
*/
int tcpSviceSend(int socket, const char *sendBuf, int sendSize, int waitMs, int trySendTims)
{
    int     mAlySendSize = 0;
    int     Retry = 0;
    fd_set  mWriteSet;
    struct  timeval mWaitim;

    int     mRet = -1;
    
    FD_ZERO(&mWriteSet);
    FD_SET(socket, &mWriteSet);
    mWaitim.tv_sec = waitMs/1000;
    mWaitim.tv_usec = waitMs%1000;

    mRet = select(socket + 1, NULL, &mWriteSet, NULL, &mWaitim);
    if (mRet <= 0)
    {
        printf("select error:%d %s", errno, strerror(errno));
        return -1;
    }

    while (mAlySendSize < sendSize)
    {
        mRet = send(socket, sendBuf + mAlySendSize, sendSize - mAlySendSize, 0);

        if (mRet <= 0)
        {
            if ( errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK )
            {
                tcpSviceUsleep(5000);
                if (Retry++ < trySendTims)
                    continue;
            }

            printf("send error:%d %s", errno, strerror(errno));
            return -2;
        }

        mAlySendSize += mRet;
    }

    return mAlySendSize;
}

/**
 * 函数名：tcpSviceRecv
 * 功能：tcp接收数据
 * 参数：
 *      socket：套接字
 *      sendBuf：接收地址
 *      sendSize：接收缓存长度
 *      needRecvSize：需要接收数据的长度
 *      waitMs：select可写等待时间
 *      trySendTims：重新接收尝试次数
 * 返回值：
 *      > 0：成功接收的长度
 *      = -1：selet监听失败或超时
 *      = -2：与服务器断开连接
*/
int tcpSviceRecv(int socket, char *recvBuf, unsigned int recvBufSzie, unsigned int needRecvSize, int waitMs, int tryRecvTims)
{
    int     mAlyRecvSize = 0;
    int     Retry = 0;
    fd_set  mReadSet;
    struct  timeval mWaitim;

    int MaxRecvLen = needRecvSize >= recvBufSzie ? recvBufSzie : needRecvSize;

    int     mRet = -1;
  
    FD_ZERO(&mReadSet);
    FD_SET(socket, &mReadSet);
    mWaitim.tv_sec = waitMs/1000;
    mWaitim.tv_usec = waitMs%1000;

    while (mAlyRecvSize < MaxRecvLen)
    {
        mRet = select(socket + 1, &mReadSet, NULL, NULL, &mWaitim);
        if (mRet < mRet)
        {
            if( errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK )
            {
                if(Retry++ < tryRecvTims)
                    continue;
            }

            printf("select fail:%s\n", strerror(errno));
            return -1;
        }
        else if (0 == mRet)
        {
            //printf("connection timeout\n");
            return mAlyRecvSize;
        }

        mRet = recv(socket, recvBuf + mAlyRecvSize, MaxRecvLen - mAlyRecvSize, 0);
        if(mRet < 0)
        {
            if ( errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK )
            {
                if(Retry++ < tryRecvTims)
                    continue;
            }

            printf("recv fail:%s\n", strerror(errno));
            break;
        }
        else if (0 == mRet)
        {
            printf("connection shutdown\n");
            return -2;
        }

        mAlyRecvSize += mRet;
        if(Retry++ >= tryRecvTims)
            break;
    }

    return mAlyRecvSize;
}

int tcpSviceClose(int socket)
{
    close(socket);
    return 0;
}

/**
 * 函数名：tcpSviceUsleep
 * 功能：tcp延时, 单位 微秒(us)
 * 参数：
 *      usec：延时时长
*/
int tcpSviceUsleep(unsigned int usec)
{
    int     Ret;
    struct  timespec requst;
    struct  timespec remain;
    remain.tv_sec = usec / 1000000;
    remain.tv_nsec = (usec % 1000000) * 1000;
    do{
        requst = remain;
        Ret = nanosleep(&requst, &remain);
    }while(-1 == Ret && errno == EINTR);
    return Ret;
}