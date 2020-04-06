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
#include "log.h"

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

atomic_t genid;

int new_id(atomic_t *value)
{
    return ++value->counter;
}

typedef struct Channel
{
    int id;
    int fd;
    int epollfd;
    struct epoll_event event;

    struct sockaddr_in sa;
    char host[INET_ADDRSTRLEN];
    size_t datalen;
    char *rev_buf;
    char *send_buf;
    char data[512];
} Channel;

int handle_input(Channel *ch)
{
    while (1)
    {
        ssize_t count = read(ch->fd, ch->data, ch->datalen);
        if (count == -1)
        {
            if (errno != EAGAIN)
            {
                perror("read");
                close(ch->fd);
            }
            debug("try again\n");
            break;
        }
        if (count == 0)
        {
            debug("read finished\n");
            /* 数据读取完毕，结束 */
            close(ch->fd);
            log_trace("Closed connection on descriptor fd=%d, id=%d", ch->fd, ch->id);
            ch->event.data.ptr = NULL;
            epoll_ctl(ch->epollfd, EPOLL_CTL_DEL, ch->fd, &ch->event);
            close(ch->fd);
            debug("free ptr: id(%d)\n", ch->id);
            free(ch);
            break;
        }
    }

    log_trace("%p\n", ch->data);

    ch->event.events = EPOLLOUT | EPOLLET;
    epoll_ctl(ch->epollfd, EPOLL_CTL_MOD, ch->fd, &ch->event);
    return 0;
}

int handle_output(Channel *ch)
{
    char buf[27];
    sprintf(buf, "id(%0d),msg(%s)\n", ch->id, "it's echo man");
    ssize_t flag = write(ch->fd, buf, sizeof buf);
    debug("handle_output write id(%d) len(%zu)\n", ch->id, flag);
    ch->event.events = EPOLLET | EPOLLIN;
    epoll_ctl(ch->epollfd, EPOLL_CTL_MOD, ch->fd, &ch->event);
    return 0;
}

int main(int argc, char *argv[])
{
    debug("main\n");
    Channel server_ch;
    int s;
    struct epoll_event *events;
    int port = read_param(argc, argv);
    /* 创建并绑定socket */
    server_ch.fd = create_and_bind(port);
    if (server_ch.fd == -1)
    {
        perror("create_and_bind");
        abort();
    }
    /* 设置sfd为非阻塞 */
    s = make_socket_non_blocking(server_ch.fd);
    if (s == -1)
    {
        perror("make_socket_non_blocking");
        abort();
    }
    /* SOMAXCONN 为系统默认的backlog */
    s = listen(server_ch.fd, SOMAXCONN);
    if (s == -1)
    {
        perror("listen");
        abort();
    }
    server_ch.epollfd = epoll_create1(0);
    if (server_ch.epollfd == -1)
    {
        perror("epoll_create");
        abort();
    }
    /* 设置ET模式 */
    server_ch.event.events = EPOLLIN | EPOLLET;
    server_ch.event.data.ptr = &server_ch;
    s = epoll_ctl(server_ch.epollfd, EPOLL_CTL_ADD, server_ch.fd, &server_ch.event);
    if (s == -1)
    {
        perror("epoll_ctl");
        abort();
    }
    /* 创建事件数组并清零 */
    events = calloc(MAXEVENTS, sizeof(struct epoll_event));
    /* 开始事件循环 */
    while (1)
    {
        int n, i;
        n = epoll_wait(server_ch.epollfd, events, MAXEVENTS, -1);
        for (i = 0; i < n; i++)
        {
            void *dataptr = events[i].data.ptr;
            if (dataptr == NULL)
            {
                debug("dataptr == NULL\n");
                continue;
            }
            Channel *ch = (Channel *)dataptr;
            // error
            if (events[i].events & (EPOLLERR | EPOLLHUP))
            {
                /* 监控到错误或者挂起 */
                fprintf(stderr, "epoll error\n");
                if (dataptr != NULL)
                {
                    struct epoll_event event = ch->event;
                    event.data.ptr = NULL;
                    epoll_ctl(ch->epollfd, EPOLL_CTL_DEL, ch->fd, &event);
                    close(ch->fd);
                    debug("free ptr: id(%d)\n", ch->id);
                    free(ch);
                    events[i].data.ptr = NULL;
                }
                continue;
            }
            // accept new client
            if (events[i].events & EPOLLIN && server_ch.fd == ch->fd)
            {
                while (1)
                {
                    struct sockaddr_in sa;
                    socklen_t len = sizeof(sa);
                    char hbuf[INET_ADDRSTRLEN];
                    int infd = accept(server_ch.fd, (struct sockaddr *)&sa, &len);
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
                    printf("Accepted connection on descriptor %d "
                           "(host=%s, port=%d)\n",
                           infd, hbuf, sa.sin_port);
                    /* 设置接入的socket为非阻塞 */
                    s = make_socket_non_blocking(infd);
                    if (s == -1)
                    {
                        abort();
                    }
                    /* 为新接入的socket注册事件 */
                    Channel *inch = (Channel *)malloc(sizeof(struct Channel));
                    inch->id = new_id(&genid);
                    inch->fd = infd;
                    inch->datalen = sizeof inch->data;
                    inch->epollfd = server_ch.epollfd;
                    inch->sa = sa;
                    strncpy(inch->host, hbuf, INET_ADDRSTRLEN);

                    inch->event.data.ptr = inch;
                    inch->event.events = EPOLLIN | EPOLLET;
                    s = epoll_ctl(inch->epollfd, EPOLL_CTL_ADD, inch->fd, &inch->event);
                    if (s == -1)
                    {
                        perror("epoll_ctl");
                        abort();
                    }
                    debug("debug: accept id(%d),fd(%d), data(%p)\n", inch->id, inch->fd, inch);
                }
            }
            // consume client request
            if (events[i].events & EPOLLIN && server_ch.fd != ch->fd)
            {
                handle_input(ch);
            }
            // client response
            if ((events[i].events & EPOLLOUT) && dataptr != NULL && (((Channel *)dataptr)->fd != server_ch.fd))
            {
                handle_output(ch);
            }
        }
    }
    free(events);
    close(server_ch.fd);
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
