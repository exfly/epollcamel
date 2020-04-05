#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "message.h"

/* 此函数用于读取参数或者错误提示 */
int read_param(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    return atoi(argv[1]);
}

int main(int argc, char *argv[])
{
    //创建套接字
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    //向服务器（特定的IP和端口）发起请求
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));           //每个字节都用0填充
    serv_addr.sin_family = AF_INET;                     //使用IPv4地址
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //具体的IP地址
    int port = read_param(argc, argv);
    serv_addr.sin_port = htons(port); //端口
    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    // 发送数据到服务器
    Message msg = message_builder(MSG, "it's client\n");

    ssize_t wsize = write(sock, &msg, sizeof(msg));
    // 读取服务器传回的数据
    char buffer[40];
    ssize_t rsize = read(sock, buffer, sizeof(buffer) - 1);

    printf("Message form server: w(%zu), r(%zu), msg(%s)\n", wsize, rsize, buffer);

    //关闭套接字
    close(sock);
    return 0;
}
