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


int main(int argc, char *argv[])
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
    mServer.sin_port = htons(atoi(argv[2]));
    if(inet_pton(AF_INET, argv[1], &mServer.sin_addr) <= 0)
    {
        printf("ip:%s inet_pton error:%d %s\n", argv[1], errno, strerror(errno));
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

    printf("connect success...\r\n");

    mRet = 0;

    close(mSocket);

    return 0;
}