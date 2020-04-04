#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <string.h>

#include "atomic.h"

#include "debug.h"

#define MAXEVENTS 1024

int create_and_bind(int port)
{
    int sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sfd == -1)
    {
        return -1;
    }
    struct sockaddr_in sa;
    bzero(&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sfd, (struct sockaddr *)&sa, sizeof(struct sockaddr)) == -1)
    {
        return -1;
    }
    return sfd;
}

int make_socket_non_blocking(int sfd)
{
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1)
    {
        return -1;
    }
    if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        return -1;
    }
    return 0;
}

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

int new_id(atomic_t* value) {
    return ++value->counter;
}

typedef struct Channel {
    int fd;
    uint32_t events;
    int id;
} Channel;

int main(int argc, char *argv[])
{
    atomic_t genid;

    int sfd, s;
    int efd;
    struct epoll_event event;
    struct epoll_event *events;
    int port = read_param(argc, argv);
    /* 创建并绑定socket */
    sfd = create_and_bind(port);
    if (sfd == -1)
    {
        perror("create_and_bind");
        abort();
    }
    /* 设置sfd为非阻塞 */
    s = make_socket_non_blocking(sfd);
    if (s == -1)
    {
        perror("make_socket_non_blocking");
        abort();
    }
    /* SOMAXCONN 为系统默认的backlog */
    s = listen(sfd, SOMAXCONN);
    if (s == -1)
    {
        perror("listen");
        abort();
    }
    efd = epoll_create1(0);
    if (efd == -1)
    {
        perror("epoll_create");
        abort();
    }
    Channel server_ch;
    server_ch.fd = sfd;
    server_ch.id = new_id(&genid);
    /* 设置ET模式 */
    server_ch.events = EPOLLIN | EPOLLET;

    event.data.ptr = &server_ch;
    event.events = server_ch.events;
    s = epoll_ctl(efd, EPOLL_CTL_ADD, server_ch.fd, &event);
    if (s == -1)
    {
        perror("epoll_ctl");
        abort();
    }
    /* 创建事件数组并清零 */
    events = calloc(MAXEVENTS, sizeof event);
    /* 开始事件循环 */
    while (1)
    {
        int n, i;
        n = epoll_wait(efd, events, MAXEVENTS, -1);
        for (i = 0; i < n; i++)
        {
            if (events[i].events & (EPOLLERR | EPOLLHUP))
            {
                /* 监控到错误或者挂起 */
                fprintf(stderr, "epoll error\n");
                close(((Channel*)events[i].data.ptr)->fd);
                continue;
            }
            if (events[i].events & EPOLLIN)
            {
                Channel* ch = (Channel*)events[i].data.ptr;
                if (sfd == ch->fd)
                {
                    /* 处理新接入的socket */
                    while (1)
                    {
                        struct sockaddr_in sa;
                        socklen_t len = sizeof(sa);
                        char hbuf[INET_ADDRSTRLEN];
                        int infd = accept(sfd, (struct sockaddr *)&sa, &len);
                        if (infd == -1)
                        {
                            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                            {
                                /* 资源暂时不可读，再来一遍 */
                                break;
                            }
                            else
                            {
                                perror("accept");
                                break;
                            }
                        }
                        inet_ntop(AF_INET, &sa.sin_addr, hbuf, sizeof(hbuf));
                        int id = new_id(&genid);
                        printf("Accepted connection on descriptor %d "
                               "(host=%s, port=%d, id=%d)\n",
                               infd, hbuf, sa.sin_port, id);
                        /* 设置接入的socket为非阻塞 */
                        s = make_socket_non_blocking(infd);
                        if (s == -1)
                            abort();
                        /* 为新接入的socket注册事件 */
                        Channel* inch = (Channel*)malloc(sizeof(struct Channel));
                        inch->fd = infd;
                        inch->events = EPOLLIN | EPOLLET;
                        inch->id = id;
                        event.data.ptr = inch;
                        event.events = inch->events;
                        s = epoll_ctl(efd, EPOLL_CTL_ADD, inch->fd, &event);
                        if (s == -1)
                        {
                            perror("epoll_ctl");
                            abort();
                        }
                    }
                    //continue;
                }
                else
                {
                    /* 接入的socket有数据可读 */
                    while (1)
                    {
                        ssize_t count;
                        char buf[512];
                        Channel* ch = (Channel*)events[i].data.ptr;
                        count = read(ch->fd, buf, sizeof buf);
                        if (count == -1)
                        {
                            if (errno != EAGAIN)
                            {
                                perror("read");
                                close(ch->fd);
                            }
                            break;
                        }
                        else if (count == 0)
                        {
                            /* 数据读取完毕，结束 */
                            close(ch->fd);
                            printf("Closed connection on descriptor fd=%d, id=%d\n", ch->fd, ch->id);
                            free(ch);
                            ch = NULL;
                            break;
                        }
                        /* 输出到stdout */
                        s = write(STDOUT_FILENO, buf, count);
                        if (s == -1)
                        {
                            perror("write");
                            abort();
                        }
                        ch->events = EPOLLOUT | EPOLLET;
                        event.events = ch->events;
                        epoll_ctl(efd, EPOLL_CTL_MOD, ch->fd, &event);
                    }
                }
            }
            else if ((events[i].events & EPOLLOUT) && (((Channel*)events[i].data.ptr)->fd != sfd))
            {
                Channel * ch = (Channel*)events[i].data.ptr;
                /* 接入的socket有数据可写 */
                s = write(ch->fd, "it's echo man\n", 14);
                ch->events = EPOLLET | EPOLLIN;
                event.events = ch->events;
                epoll_ctl(efd, EPOLL_CTL_MOD, ch->fd, &event);
            }
        }
    }
    free(events);
    close(sfd);
    return EXIT_SUCCESS;
}

// https://blog.lucode.net/linux/epoll-tutorial.html
// gcc echoman.c -o echoman
// nc 127.0.0.1 8000

// 使用epoll一定要加定时器，否则后患无穷
// 如果多个线程观察的fd相同(通常是server socket fd)，据说epoll_wait会有惊群问题(accept那个问题早就解决了)，但我暂时没有发现
// 联合体data中的那个ptr是很有用的，只不过这就意味着你将该对象的生命周期交给了epoll，不排除会有潜在bug的影响，需要辅以timeout
// 多线程环境下使用epoll，多考虑EPOLLONESHOT
// EPOLLLT也是一个不错的选择，除非你的框架能够确保每次事件触发后，都读/写至EAGAIN
// epoll和kqueue很像，可以通过封装统一二者，虽然后者看似更加强大，其实IOCP也可统一，只不过这样的代价很大
// 使用前请仔细阅读man 7 epoll，勿做傻事
