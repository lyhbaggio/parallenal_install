#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>


#define IPADDRESS  "127.0.0.1"
#define PORT      9999
#define MAXSIZE   1024
#define LISTENQ   5
#define FDSIZE    1000
#define EPOLLEVENTS 600
#define MAXEPOLLSIZE 10000

/* 函数声明 */
/* 创建套接字并进行绑定 */
static int socket_bind(const char* ip, int port);

/* 设置socket为非阻塞模式 */
static int setnonblocking(int fd);

/* IO多路复用epoll */
static void do_epoll(int listenfd);
/* 事件处理函数 */
static void handle_events(int epollfd,struct epoll_event *events, int num, int listenfd, char *buf);
/* 处理接收到的连接 */
static void handle_accpet(int epollfd, int listenfd);
/* 读处理 */
static void do_read(int epollfd, int fd, char *buf);
/* 写处理 */
static void do_write(int epollfd, int fd, char *buf);
/* 添加事件 */
static void add_event(int epollfd, int fd, int state);
/* 修改事件 */
static void modify_event(int epollfd, int fd, int state);
/* 删除事件 */
static void delete_event(int epollfd, int fd, int state);

/* LT工作模式 */
//static void lt(struct epoll_event* events, int number, int epollfd, int listenfd );

/* ET工作模式 */
//static void et(struct epoll_event* events, int number, int epollfd, int listenfd );

int main(int argc,char *argv[])
{
	int listenfd;
	listenfd = socket_bind(IPADDRESS, PORT);
	listen(listenfd, LISTENQ);
	do_epoll(listenfd);
	return 0;
}

/* 设置句柄为非阻塞方式 */
int setnonblocking(int sockfd)
{
    if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1) {
        return -1;
    }
    return 0;
}

static int socket_bind(const char* ip, int port)
{
	int listenfd;
	struct sockaddr_in servaddr;
	struct rlimit rt;
	int opt = SO_REUSEADDR;

	/* 设置每个进程允许打开的最大文件数 */
	rt.rlim_max = rt.rlim_cur = MAXEPOLLSIZE;
	if (setrlimit(RLIMIT_NOFILE, &rt) == -1) {
		perror("setrlimit");
		exit(1);
	}
	else printf("设置系统资源参数成功！\n");

	listenfd = socket(PF_INET, SOCK_STREAM, 0);
	if (listenfd == -1)
	{
		perror("socket error:");
		exit(1);
	}
	else
		printf("socket 创建成功！\n");

	/* 设置socket属性，端口可以重用 */
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	/* 设置socket为非阻塞模式 */
	setnonblocking(listenfd);

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = PF_INET;
	//inet_pton(AF_INET, ip, &servaddr.sin_addr);
	servaddr.sin_port = htons(port);
	if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr)) == -1)
	{
		perror("bind error: ");
		exit(1);
	}else
		printf("IP 地址和端口绑定成功\n");

	return listenfd;
}

static void do_epoll(int listenfd)
{
	int epollfd;
	struct epoll_event events[EPOLLEVENTS];
	int ret;
	char buf[MAXSIZE];
	memset(buf, 0, MAXSIZE);
	/* 创建一个描述符 */
	epollfd = epoll_create(FDSIZE);
	/* 添加监听描述符事件 */
	add_event(epollfd, listenfd, EPOLLIN);
	
	while(1)
	{
		/* 获取已经准备好的描述符事件 */
		ret = epoll_wait(epollfd, events, EPOLLEVENTS, -1);

		if (ret < 0)
		{
			if (ret == EINTR) 
				continue;	
			perror("epoll wait failure\n");	
		}

		handle_events(epollfd, events, ret, listenfd, buf);
		
		/* 使用LT模式 未写好 */
        //lt( events, ret, epollfd, listenfd );
		/* 使用ET模式 未写好 */
        //et( events, ret, epollfd, listenfd );
	}
	close(epollfd);
}

/*
static void lt(struct epoll_event* events, int number, int epollfd, int listenfd )
{
    char buf[ BUFFER_SIZE ];
    for ( int i = 0; i < number; i++ )
    {
        int sockfd = events[i].data.fd;
        if ( sockfd == listenfd )
        {
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof( client_address );
            int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
            add_event( epollfd, connfd, false );
        }
        else if ( events[i].events & EPOLLIN )
        {
            printf( "event trigger once\n" );
            memset( buf, '\0', BUFFER_SIZE );
            int ret = recv( sockfd, buf, BUFFER_SIZE-1, 0 );
            if( ret <= 0 )
            {
                close( sockfd );
                continue;
            }
            printf( "get %d bytes of content: %s\n", ret, buf );
        }
        else
        {
            printf( "something else happened \n" );
        }
    }
}

static void et(struct epoll_event* events, int number, int epollfd, int listenfd )
{
    char buf[ BUFFER_SIZE ];
    for ( int i = 0; i < number; i++ )
    {
        int sockfd = events[i].data.fd;
        if ( sockfd == listenfd )
        {
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof( client_address );
            int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
            addfd( epollfd, connfd, true );
        }
        else if ( events[i].events & EPOLLIN )
        {
            printf( "event trigger once\n" );
            while( 1 )
            {
                memset( buf, '\0', BUFFER_SIZE );
                int ret = recv( sockfd, buf, BUFFER_SIZE-1, 0 );
                if( ret < 0 )
                {
                    if( ( errno == EAGAIN ) || ( errno == EWOULDBLOCK ) )
                    {
                        printf( "read later\n" );
                        break;
                    }
                    close( sockfd );
                    break;
                }
                else if( ret == 0 )
                {
                    close( sockfd );
                }
                else
                {
                    printf( "get %d bytes of content: %s\n", ret, buf );
                }
            }
        }
        else
        {
            printf( "something else happened \n" );
        }
    }
}
*/

static void handle_events(int epollfd, struct epoll_event *events, int num, int listenfd, char *buf)
{
	int i;
	int fd;
	/* 按类型遍历事件 */
	for (i = 0 ; i < num ; i++)
	{
		fd = events[i].data.fd;
		/* 根据描述符的类型和事件类型进行处理 */
		if ((fd == listenfd) && (events[i].events & EPOLLIN))
			handle_accpet(epollfd, listenfd);
		else if (events[i].events & EPOLLIN)
			do_read(epollfd, fd, buf);
		else if (events[i].events & EPOLLOUT)
			do_write(epollfd, fd, buf);
	}
}

static void handle_accpet(int epollfd, int listenfd)
{
	int clifd;
	struct sockaddr_in cliaddr;
	socklen_t cliaddrlen;
	clifd = accept(listenfd, (struct sockaddr*)&cliaddr, &cliaddrlen);
	if (clifd == -1)
		perror("accpet error:");
	else
	{
		printf("accept a new client: %s:%d\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
		/* 添加一个客户描述符和事件 */
		add_event(epollfd, clifd, EPOLLIN);
	}
}

static void do_read(int epollfd, int fd, char *buf)
{
	int nread;
	nread = read(fd, buf, MAXSIZE);
	if (nread == -1)
	{
		perror("read error:");
		close(fd);
		delete_event(epollfd, fd, EPOLLIN);
	}
	else if (nread == 0)
	{
		fprintf(stderr, "client close.\n");
		close(fd);
		delete_event(epollfd, fd, EPOLLIN);
	}
	else
	{
		printf("read message is : %s\n", buf);
		/* 修改描述符对应的事件，由读改为写 */
		modify_event(epollfd, fd, EPOLLOUT);
	}
}

static void do_write(int epollfd, int fd, char *buf)
{
	int nwrite;
	nwrite = write(fd, buf, strlen(buf));
	if (nwrite == -1)
	{
		perror("write error:");
		close(fd);
		delete_event(epollfd, fd, EPOLLOUT);
	}
	else
		modify_event(epollfd, fd, EPOLLIN);
	memset(buf, 0, MAXSIZE);
}

//static void add_event(int epollfd, int fd, bool enable_et)
static void add_event(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	//ev.events = state | EPOLLET;
	ev.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
    setnonblocking( fd );
}

static void delete_event(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	//ev.events = state | EPOLLET;
	ev.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
}

static void modify_event(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	//ev.events = state | EPOLLET;
	ev.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
    setnonblocking( fd );
}
