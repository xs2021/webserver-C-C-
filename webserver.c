//web服务端程序---使用epoll模型
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>

#include "pub.h"
#include "wrap.h"

//int http_request(int cfd);
int http_request(int cfd, int epfd);
int send_header(int cfd, char *code, char *msg, char *fileType, int length);
int send_file(int cfd, char *fileName);

int main()
{
	//bug：若web服务器给浏览器发送数据时，浏览器此时关闭连接，则web服务器会收到SIGPIPE信号
	//解决：直接忽略SIGPIPE 信号
	struct sigaction st;  //sigaction 支持跨平台
	st.sa_handler = SIG_IGN; 
	sigemptyset(&st.sa_mask);
	st.sa_flags = 0;
	sigaction(SIGPIPE, &st, NULL);
	//signal(SIGPIPE, SIG_IGN);
	
	//改变当前进程工作目录
	//getenv：获取当前用户的家目录
	char path[255] = {0};
	sprintf(path, "%s/%s", getenv("HOME"), "webpath"); //注意：两个%s之间的斜杠
	chdir(path);
	
	//创建socket、设置端口复用、绑定
	//tcp4bind()---wrap.c
	int lfd = tcp4bind(9999, NULL);
	
	//设置监听
	Listen(lfd, 128);
	
	//创建epoll树 
	int epfd = epoll_create(1024); //参数>0即可
	if(epfd<0)
	{
		perror("epoll_create_error");
		close(lfd);
		return -1;
	}
	
	//将监听文件描述符lfd上树
	struct epoll_event ev;
	ev.data.fd = lfd;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	
	int nready;
	int sockfd;
	int cfd;
	struct epoll_event events[1024];
	while (1) {
		 //等待事件发生
		 nready = epoll_wait(epfd, events, 1024, -1);
		 if(nready<0)
		 {
		 		if(errno==EINTR)
		 		{
		 				continue;
		 		}
		 		break;
		 }
		 
		 //循环判断发生的事件：有两种情况
		 for (int i=0; i<nready; i++)
		 {
		 		sockfd = events[i].data.fd;
		 		//1、有客户端连接请求
		 		if(sockfd==lfd)
				{
						//接收新的客户端连接
						cfd = Accept(lfd, NULL, NULL);
						
						//设置cfd为非阻塞
						int flag = fcntl(cfd, F_GETFL);
						flag |= O_NONBLOCK;
						fcntl(cfd, F_SETFL, flag);
						
						//将通信描述符继续上树
						ev.data.fd = cfd;
						ev.events = EPOLLIN;
						epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
				}
				else 
				{
						//2、客户端有数据发来
						//http_request(cfd); //bug：参数较少&不准确
						http_request(sockfd, epfd);
				}
		 		
		 }
	
	}
	
}

//拼接头部
int send_header(int cfd, char *code, char *msg, char *fileType, int len)
{
		char buf[1024] = {0};
		sprintf(buf, "HTTP/1.1 %s %s\r\n", code, msg);
		sprintf(buf+strlen(buf), "Content-Type:%s\r\n", fileType);
		if(len>0)
		{
				sprintf(buf+strlen(buf), "Content-Length:%d\r\n", len);			
		}
		strcat(buf, "\r\n");
		//注意：拼接后记得发送
		Write(cfd, buf, strlen(buf));
		return 0;
}

//拼接正文
int send_file(int cfd, char *fileName)
{
		//打开文件
		int fd = open(fileName, O_RDONLY);
		if(fd<0)
		{
				perror("open error");
				return -1;
		}
		
		char buf[1024];
		while (1) 
		{
				memset(buf, 0x00, sizeof(buf));
				int n = read(fd, buf, sizeof(buf));	 
				if(n<=0)
				{
						break;
				}
				else 
				{
						Write(cfd, buf, n);
				}
		}
}

int http_request(int cfd, int epfd)
{
		//1、读取请求行
		int n;
		char buf[1024];
		memset(buf, 0x00, sizeof(buf));
		//Readline(cfd, buf, sizeof(buf)); //bug:当浏览器关闭时需要关闭连接
		n = Readline(cfd, buf, sizeof(buf));
		if(n<=0)
		{
				//关闭连接
				close(cfd);
				//将文件描述符从epoll树删除
				epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
		}
		printf("buf==[%s]\n", buf);
		
		//2、分析请求行（要请求的资源文件）
		//例如：GET /xiaoshuai.c HTTP/1.1
		char reqType[16] = {0};
		char fileName[255] = {0};
		char protocol[16] = {0};
		//int sscanf(const char *str, const char *format, ...);
		sscanf(buf, "%[^ ] %[^ ] %[^ \r\n]", reqType, fileName, protocol);
		//printf("[%s]\n", reqType);
		printf("==[%s]==\n", fileName);
		//printf("[%s]\n", protocol);
		
		//默认为：根目录
		char *pFile = fileName; 
		if(strlen(fileName)<=1) //比如：192.168.2.189:9999
		{
				strcpy(pFile, "./");
		}
		else //其它目录  //比如：192.168.2.189:9999/html
		{
				//跳过文件名称开头的斜杠
				pFile = fileName+1;
				printf("[%s]\n", pFile);
		}
		
		//bug：不能访问中文名称文件，需要转换汉字编码
		strdecode(pFile, pFile);
		printf("[%s]\n", pFile);
		
		//3、循环读完剩余的数据，避免产生粘包现象
		while((n=Readline(cfd, buf, sizeof(buf)))>0);

		//4、判断文件是否存在
		struct stat st;
		if(stat(pFile, &st)<0)  //4.1 若文件不存在
		{
				printf("file not exist\n");
				//组织应答信息：http响应信息+错误页内容

				//发送头部信息
				//get_mime_type---pub.c
				send_header(cfd, "404", "NOT FOUND", get_mime_type(".html"), 0);
				//发送文件内容
				send_file(cfd, "error.html");
		}
		else //4.2 若文件存在
		{
			//判断文件类型
			if(S_ISREG(st.st_mode))  //4.2.1 普通文件
			{
					printf("file exist\n");
					//发送头部信息
					//get_mime_type---pub.c
					send_header(cfd, "200", "OK", get_mime_type(pFile), st.st_size);
					//发送文件内容
					send_file(cfd, pFile);
			}
			else if(S_ISDIR(st.st_mode)) //4.2.2 目录文件
			{
					printf("目录文件\n");
					
					//发送头部信息
					//get_mime_type---pub.c
					send_header(cfd, "200", "OK", get_mime_type(".html"), 0);
					
					
					//1、发送html文件头部
					char buffer[1024];
					send_file(cfd, "html/dir_header.html");
					
					//scandir读取列表，拼接为html文件
					//2、然后发送文件列表
					struct dirent **namelist;
				  int num;

				  num = scandir(pFile, &namelist, NULL, alphasort);
				  if (num == -1)  //异常处理
				  {
				      perror("scandir");
				      close(cfd);
				      epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
				      return -1;
				  }

				  while (num--) 
				  {
				  	 	printf("%s\n", namelist[num]->d_name);
				   		memset(buf, 0x00, sizeof(buf));
				   		//判断目录内的文件类型
				   		if(namelist[num]->d_type==DT_DIR) //目录
				   		{
				   			//bug：%s后边跟上斜杠
				   			sprintf(buffer, "<li><a href = %s/> %s </a></li>", namelist[num]->d_name, namelist[num]->d_name);
				   		}
				   		else  //普通文件
				   		{
				   			sprintf(buffer, "<li><a href = %s> %s </a></li>", namelist[num]->d_name, namelist[num]->d_name);
				   		}

				      free(namelist[num]);
				      write(cfd, buffer, strlen(buffer));
				  }
				  free(namelist);
					
					//3、发送html文件尾部
					send_file(cfd, "html/dir_tail.html");
			}
		}
		
}
