#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

//stl head

#include <ext/hash_map> //包含hash_map 的头文件

//#include <map> //stl的map

using namespace std; //std 命名空间

using namespace __gnu_cxx; //而hash_map是在__gnu_cxx的命名空间里的


#define	BUFFER_MAX_LEGTH		1024
#define MAX_PTHREAD_NUMBER		100

pthread_t pthread_pool[MAX_PTHREAD_NUMBER];
int pthread_pool_size;

int init_thread_pool(int threadNum);
int join_thread_pool();
void *epoll_loop(void* para);
void *check_connect_timeout(void* para);


struct sockStruct
{
    time_t time;

    unsigned int* recvBuf;
};

//hash-map

//hash_map<int, unsigned int>        sock_map;

hash_map<int, sockStruct>        sock_map;

 
#define MAXRECVBUF 4096
#define MAXBUF MAXRECVBUF+10 

int fd_Setnonblocking(int fd)
{
    int op;
 
    op=fcntl(fd,F_GETFL,0);
    fcntl(fd,F_SETFL,op|O_NONBLOCK);
 
    return op;
}
 
void on_sigint(int signal)
{
    exit(0);
}
 
int listenfd;
int sock_op=1;
struct sockaddr_in address;
struct epoll_event event;
struct epoll_event events[BUFFER_MAX_LEGTH];
int epfd;
int n;
int i;
char buf[BUFFER_MAX_LEGTH];
int off;
int result;
char *p;
int iport;

int main(int argc,char* argv[])
{
	if (argc != 2)
    {
		fprintf (stderr, "Usage: %s [port]\n", argv[0]);
		exit (EXIT_FAILURE);
    }
	iport = atoi(argv[1]);

    init_thread_pool(10);

    signal(SIGPIPE,SIG_IGN);
    signal(SIGCHLD,SIG_IGN);
    signal(SIGINT,&on_sigint);
    listenfd=socket(AF_INET,SOCK_STREAM,0);
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&sock_op,sizeof(sock_op));
 
    memset(&address,0,sizeof(address));
    address.sin_addr.s_addr=htonl(INADDR_ANY);
    address.sin_port=htons(iport);
    bind(listenfd,(struct sockaddr*)&address,sizeof(address));
    listen(listenfd,BUFFER_MAX_LEGTH);
    fd_Setnonblocking(listenfd);
 
    epfd=epoll_create(655350);
    memset(&event,0,sizeof(event));
    event.data.fd=listenfd;
    event.events=EPOLLIN|EPOLLET;
    epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&event);

	join_thread_pool();
    while(1){
        sleep(1000);
    }
    return 0;
}

/*************************************************
* Function: * init_thread_pool
* Description: * 初始化线程
* Input: * threadNum:用于处理epoll的线程数
* Output: * 
* Others: * 此函数为静态static函数,
*************************************************/
pthread_t pthread_timeout;
int init_thread_pool(int threadNum)
{
    int i,ret;

    if (threadNum >= MAX_PTHREAD_NUMBER)
	{
		printf("Too many thread numbers(%d), max is %d\n", threadNum, MAX_PTHREAD_NUMBER);
		return -1;
	}
	pthread_pool_size = threadNum;

    //初始化epoll线程池
    for ( i = 0; i < pthread_pool_size; i++)
    {
        ret = pthread_create(&pthread_pool[i], 0, epoll_loop, (void *)0);
        if (ret != 0)
        {
            printf("pthread_%d create failed!\n", i);
            return(-1);
        }
		else
		{
			printf("pthread_%d create succeed!\n", i);
		}
    }

    ret = pthread_create(&pthread_timeout, 0, check_connect_timeout, (void *)0);

    return(0);
}


int join_thread_pool()
{
	void *retval;
	int ret;
	
	for ( i = 0; i < pthread_pool_size; i++)
    {
		ret = pthread_join(pthread_pool[i], &retval);
		printf("thread_%d return value(retval) is %x\n", i, retval);
		printf("thread_%d return value(tmp) is %d\n", i, ret);
		if (ret != 0) {
			printf("cannot join with thread_%d\n", i);
		}
		printf("thread_%d end\n", i);
	}
}

void printids()
{
	pid_t 		pid;
	pthread_t 	tid;
	
	pid = getpid();
	tid = pthread_self();
	printf("thread: pid %lu tid %d\n", (unsigned long)pid, tid);
}

/*************************************************
* Function: * epoll_loop
* Description: * epoll检测循环
* Input: * 
* Output: * 
* Others: * 
*************************************************/
static int count111 = 0;
static time_t oldtime = 0, nowtime = 0;
void *epoll_loop(void* para)
{
    while(1)
    {
        n=epoll_wait(epfd,events,4096,-1);
        //printf("n = %d\n", n);
		printids();
		
		struct sockaddr in_addr;
		socklen_t in_len;
		int s;
		char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
		in_len = sizeof in_addr;
        if(n>0)
        {
            for(i=0;i<n;++i)
            {
                if(events[i].data.fd==listenfd)
                {
                    while(1)
                    {
                        event.data.fd=accept(listenfd, &in_addr, &in_len);
						s = getnameinfo (&in_addr, in_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf, NI_NUMERICHOST | NI_NUMERICSERV);
						if (s == 0)
						{
							printf("Descriptor %d (host=%s, port=%s)\n", events[i].data.fd, hbuf, sbuf);
						}
                        if(event.data.fd>0)
                        {
                            fd_Setnonblocking(event.data.fd);
                            event.events=EPOLLIN|EPOLLET;
                            epoll_ctl(epfd,EPOLL_CTL_ADD,event.data.fd,&event);
                        }
                        else
                        {
                            if(errno==EAGAIN)
                            break;
                        }
                    }
                }
                else
                {
                    if(events[i].events&EPOLLIN)
                    {
                        //handle_message(events[i].data.fd);


                        char recvBuf[BUFFER_MAX_LEGTH] = {0}; 

                        int ret = 999;

                        int rs = 1;


                        while(rs)
                        {
                            ret = recv(events[i].data.fd,recvBuf,BUFFER_MAX_LEGTH,0);// 接受客户端消息

                            if(ret < 0)
                            {
                                //由于是非阻塞的模式,所以当errno为EAGAIN时,表示当前缓冲区已无数据可//读在这里就当作是该次事件已处理过。

                                if(errno == EAGAIN)
                                {
                                    printf("EAGAIN\n");
                                    break;
                                }
                                else{
                                    printf("recv error!\n");
                                    epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, &event);
                                    close(events[i].data.fd);
                                    break;
                                }
                            }
                            else if(ret == 0)
                            {
                                // 这里表示对端的socket已正常关闭. 

                                rs = 0;
                            }
                            if(ret == sizeof(recvBuf))
                                rs = 1; // 需要再次读取

                            else
                                rs = 0;
                        }

                        if(ret>0)
						{	
							printf("Recv Msg:%s\n",recvBuf);
							
                            count111 ++;



                            struct tm *today;
                            time_t ltime;
                            time( &nowtime );

                            if(nowtime != oldtime){
                                printf("newtime handle count:%d\n", count111);
                                oldtime = nowtime;
                                count111 = 0;
                            }
					

                            char buf[BUFFER_MAX_LEGTH] = {0};
                            sprintf(buf,"epoll test server response(EPOLLIN):%s",recvBuf);
                            send(events[i].data.fd,buf,strlen(buf),0);


                            //    CGelsServer Gelsserver;

                            //    Gelsserver.handle_message(events[i].data.fd);

                        }


                        epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, &event);
                        close(events[i].data.fd);

                    }
                    else if(events[i].events&EPOLLOUT)
                    {
                        sprintf(buf,"epoll test server response(EPOLLOUT)");
                        send(events[i].data.fd,buf,strlen(buf),0);
                        /*
                        if(p!=NULL)
                        {
                            free(p);
                            p=NULL;
                        }
                        */
                        close(events[i].data.fd);
                    }
                    else
                    {
                        close(events[i].data.fd);
                    }
                }
            }
        }
		else if (n==0)
		{
			perror("epoll_wait return 0:");
		}
		else if (n==-1) 
		{
			perror("epoll_wait return -1:");
		}
		else
		{
			perror("epoll_wait error:");
		}
    }

}
/*************************************************
* Function: * check_connect_timeout
* Description: * 检测长时间没反应的网络连接，并关闭删除
* Input: * 
* Output: * 
* Others: * 
*************************************************/
void *check_connect_timeout(void* para)
{
    hash_map<int, sockStruct>::iterator it_find;
    for(it_find = sock_map.begin(); it_find!=sock_map.end(); ++it_find){
        if( time((time_t*)0) - (it_find->second).time > 120){                //时间更改


            free((it_find->second).recvBuf);
            sock_map.erase(it_find);

            close(it_find->first);
        }
    }

}
