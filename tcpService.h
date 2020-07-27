#ifndef _TCP_SERVICE_H_
#define _TCP_SERVICE_H_


/**
 * 函数名：tcpSviceConet
 * 功能：连接服务器, 非阻塞模式
 * 参数：
 *      serIp：服务器地址
 *      serPort：服务器端口
 *      waitMs：select等待时间, 单位 ms
 * 返回值：
 *      > 0：socket套接字
 *      < 0：连接服务器失败
*/
extern int tcpSviceConet(char *serIp, unsigned short serPort, int waitMs);


/**
 * 函数名：tcpSviceConetNormal
 * 功能：连接服务器, 阻塞模式
 * 参数：
 *      serIp：服务器地址
 *      serPort：服务器端口
 *      waitMs：select等待时间, 单位 ms
 * 返回值：
 *      > 0：socket套接字
 *      < 0：连接服务器失败
*/
extern int tcpSviceConetNormal(char *serIp, unsigned short serPort);

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
extern int tcpSviceSend(int socket, const char *sendBuf, int sendSize, int waitMs, int trySendTims);

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
extern int tcpSviceRecv(int socket, char *recvBuf, unsigned int recvBufSzie, unsigned int needRecvSize, int waitMs, int tryRecvTims);

/**
 * 函数名：tcpSviceClose
 * 功能：关闭socket tcp 连接
 * 参数：
 *      socket：套接字
*/
extern int tcpSviceClose(int socket);

/**
 * 函数名：tcpSviceUsleep
 * 功能：tcp延时, 单位 微秒(us)
 * 参数：
 *      usec：延时时长
*/
extern int tcpSviceUsleep(unsigned int usec);


#endif