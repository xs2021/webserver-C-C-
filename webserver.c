//web����˳���---ʹ��epollģ��
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
	//bug����web���������������������ʱ���������ʱ�ر����ӣ���web���������յ�SIGPIPE�ź�
	//�����ֱ�Ӻ���SIGPIPE �ź�
	struct sigaction st;  //sigaction ֧�ֿ�ƽ̨
	st.sa_handler = SIG_IGN; 
	sigemptyset(&st.sa_mask);
	st.sa_flags = 0;
	sigaction(SIGPIPE, &st, NULL);
	//signal(SIGPIPE, SIG_IGN);
	
	//�ı䵱ǰ���̹���Ŀ¼
	//getenv����ȡ��ǰ�û��ļ�Ŀ¼
	char path[255] = {0};
	sprintf(path, "%s/%s", getenv("HOME"), "webpath"); //ע�⣺����%s֮���б��
	chdir(path);
	
	//����socket�����ö˿ڸ��á���
	//tcp4bind()---wrap.c
	int lfd = tcp4bind(9999, NULL);
	
	//���ü���
	Listen(lfd, 128);
	
	//����epoll�� 
	int epfd = epoll_create(1024); //����>0����
	if(epfd<0)
	{
		perror("epoll_create_error");
		close(lfd);
		return -1;
	}
	
	//�������ļ�������lfd����
	struct epoll_event ev;
	ev.data.fd = lfd;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	
	int nready;
	int sockfd;
	int cfd;
	struct epoll_event events[1024];
	while (1) {
		 //�ȴ��¼�����
		 nready = epoll_wait(epfd, events, 1024, -1);
		 if(nready<0)
		 {
		 		if(errno==EINTR)
		 		{
		 				continue;
		 		}
		 		break;
		 }
		 
		 //ѭ���жϷ������¼������������
		 for (int i=0; i<nready; i++)
		 {
		 		sockfd = events[i].data.fd;
		 		//1���пͻ�����������
		 		if(sockfd==lfd)
				{
						//�����µĿͻ�������
						cfd = Accept(lfd, NULL, NULL);
						
						//����cfdΪ������
						int flag = fcntl(cfd, F_GETFL);
						flag |= O_NONBLOCK;
						fcntl(cfd, F_SETFL, flag);
						
						//��ͨ����������������
						ev.data.fd = cfd;
						ev.events = EPOLLIN;
						epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
				}
				else 
				{
						//2���ͻ��������ݷ���
						//http_request(cfd); //bug����������&��׼ȷ
						http_request(sockfd, epfd);
				}
		 		
		 }
	
	}
	
}

//ƴ��ͷ��
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
		//ע�⣺ƴ�Ӻ�ǵ÷���
		Write(cfd, buf, strlen(buf));
		return 0;
}

//ƴ������
int send_file(int cfd, char *fileName)
{
		//���ļ�
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
		//1����ȡ������
		int n;
		char buf[1024];
		memset(buf, 0x00, sizeof(buf));
		//Readline(cfd, buf, sizeof(buf)); //bug:��������ر�ʱ��Ҫ�ر�����
		n = Readline(cfd, buf, sizeof(buf));
		if(n<=0)
		{
				//�ر�����
				close(cfd);
				//���ļ���������epoll��ɾ��
				epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
		}
		printf("buf==[%s]\n", buf);
		
		//2�����������У�Ҫ�������Դ�ļ���
		//���磺GET /xiaoshuai.c HTTP/1.1
		char reqType[16] = {0};
		char fileName[255] = {0};
		char protocol[16] = {0};
		//int sscanf(const char *str, const char *format, ...);
		sscanf(buf, "%[^ ] %[^ ] %[^ \r\n]", reqType, fileName, protocol);
		//printf("[%s]\n", reqType);
		printf("==[%s]==\n", fileName);
		//printf("[%s]\n", protocol);
		
		//Ĭ��Ϊ����Ŀ¼
		char *pFile = fileName; 
		if(strlen(fileName)<=1) //���磺192.168.2.189:9999
		{
				strcpy(pFile, "./");
		}
		else //����Ŀ¼  //���磺192.168.2.189:9999/html
		{
				//�����ļ����ƿ�ͷ��б��
				pFile = fileName+1;
				printf("[%s]\n", pFile);
		}
		
		//bug�����ܷ������������ļ�����Ҫת�����ֱ���
		strdecode(pFile, pFile);
		printf("[%s]\n", pFile);
		
		//3��ѭ������ʣ������ݣ��������ճ������
		while((n=Readline(cfd, buf, sizeof(buf)))>0);

		//4���ж��ļ��Ƿ����
		struct stat st;
		if(stat(pFile, &st)<0)  //4.1 ���ļ�������
		{
				printf("file not exist\n");
				//��֯Ӧ����Ϣ��http��Ӧ��Ϣ+����ҳ����

				//����ͷ����Ϣ
				//get_mime_type---pub.c
				send_header(cfd, "404", "NOT FOUND", get_mime_type(".html"), 0);
				//�����ļ�����
				send_file(cfd, "error.html");
		}
		else //4.2 ���ļ�����
		{
			//�ж��ļ�����
			if(S_ISREG(st.st_mode))  //4.2.1 ��ͨ�ļ�
			{
					printf("file exist\n");
					//����ͷ����Ϣ
					//get_mime_type---pub.c
					send_header(cfd, "200", "OK", get_mime_type(pFile), st.st_size);
					//�����ļ�����
					send_file(cfd, pFile);
			}
			else if(S_ISDIR(st.st_mode)) //4.2.2 Ŀ¼�ļ�
			{
					printf("Ŀ¼�ļ�\n");
					
					//����ͷ����Ϣ
					//get_mime_type---pub.c
					send_header(cfd, "200", "OK", get_mime_type(".html"), 0);
					
					
					//1������html�ļ�ͷ��
					char buffer[1024];
					send_file(cfd, "html/dir_header.html");
					
					//scandir��ȡ�б�ƴ��Ϊhtml�ļ�
					//2��Ȼ�����ļ��б�
					struct dirent **namelist;
				  int num;

				  num = scandir(pFile, &namelist, NULL, alphasort);
				  if (num == -1)  //�쳣����
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
				   		//�ж�Ŀ¼�ڵ��ļ�����
				   		if(namelist[num]->d_type==DT_DIR) //Ŀ¼
				   		{
				   			//bug��%s��߸���б��
				   			sprintf(buffer, "<li><a href = %s/> %s </a></li>", namelist[num]->d_name, namelist[num]->d_name);
				   		}
				   		else  //��ͨ�ļ�
				   		{
				   			sprintf(buffer, "<li><a href = %s> %s </a></li>", namelist[num]->d_name, namelist[num]->d_name);
				   		}

				      free(namelist[num]);
				      write(cfd, buffer, strlen(buffer));
				  }
				  free(namelist);
					
					//3������html�ļ�β��
					send_file(cfd, "html/dir_tail.html");
			}
		}
		
}
