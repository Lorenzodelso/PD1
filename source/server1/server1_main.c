
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../errlib.h"
#include "../sockwrap.h"

#define LISTENQ 15 
#define MAXBUFF 100
#define MAXFILE 30
#define MSG_OK "+OK\r\n"
#define MSG_GET "GET"
#define MSG_ERR "-ERR\r\n"

#ifdef TRACE
#define trace(x) x
#else 
#define trace(x)
#endif

char *prog_name;

int main (int argc, char *argv[])
{
  int listenfd,connfd;
  short port;
  struct sockaddr_in servaddr,cliaddr;
  socklen_t cliaddrlen = sizeof(cliaddr);
  fd_set cset; 
  int n,byteRead;
  struct timeval tval;
  char read_buff[MAXBUFF];
  char get[4];
  char file_name[MAXFILE];
  FILE* file;
  struct stat file_stat;
  int file_size;


  /*set the program name for errlib*/
  prog_name = argv[0];

  /*check arguments */
  if (argc !=2 )
    err_quit("usage %s <address> <port>\n",prog_name);
  port = atoi(argv[1]);

  /*create socket */
  listenfd = Socket(AF_INET,SOCK_STREAM,0);

  /*bind*/
  memset(&servaddr,0,sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  Bind(listenfd,(SA*)&servaddr,sizeof(servaddr));

  printf("(%s) socket created\n",prog_name);
  printf("(%s) listening on %s:%u\n", prog_name, inet_ntoa(servaddr.sin_addr),ntohs(servaddr.sin_port));

  /*listen*/
  Listen(listenfd,LISTENQ);

  while(1){
            printf("(%s) waiting for connection..\n",prog_name);
	    int retry = 0;
	    do{
	      connfd = accept(listenfd,(SA*) &cliaddr, &cliaddrlen);
	      if(connfd<0){
		if(INTERRUPTED_BY_SIGNAL || errno == EPROTO || errno == ECONNABORTED || errno == EMFILE || errno == ENFILE || errno == ENOBUFS || errno == ENOMEM ) {
		  retry = 1;
		  err_ret("(%s) error-accept failed\n",prog_name);
		} else{
		  err_ret("(%s) error-accept failed\n",prog_name);
		  return 1;
		}
	      } else {
		printf("(%s) new connection from client %s:%u\n",prog_name, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
		retry = 0;
	      }
	    } while(retry);

	    /*select the correct socket*/
	    FD_ZERO(&cset);
	    FD_SET(connfd,&cset);
	    tval.tv_sec = LISTENQ;
	    if( (n=Select(FD_SETSIZE,&cset,NULL,NULL,&tval)) == -1)
	      err_ret("(%s) select failed\n",prog_name);

	    /*read the GET instruction from client */
	    if(n>0){
	      if( (byteRead = readline(connfd,read_buff,MAXBUFF)) < 0)
		err_quit("(%s) readline-error\n",prog_name);
	      else{
		read_buff[byteRead]='\0';
		printf("(%s) string received: %s\n",prog_name,read_buff);
	
		if(sscanf(read_buff,"GET %s\r\n",file_name)==0){   /*caso di input nel formato errato*/
		  printf("(%s) protocol-error: usage: GET <file_name> CR LF\n",prog_name);
		  Write(connfd,MSG_ERR,strlen(MSG_ERR));
		  break;
		}
		else{

		  /*apro il file, lo leggo e invio i byte al client */

		  if  ( ( file = fopen(file_name,"r") ) ==NULL){
		    printf("(%s) cannot find the file in the working directory\n",prog_name);
		    Write(connfd,MSG_ERR,strlen(MSG_ERR));
		    break;
		  }

		  /*informazioni sul file*/ 
		  if (stat(file_name,&file_stat) < 0){
		    err_quit("(%s) stat-error\n",prog_name);
		  }

		  file_size = file_stat.st_size;

		  Write(connfd,MSG_OK,strlen(MSG_OK));

		  uint32_t val = htonl(file_size);
		  Write(connfd,&val,sizeof(file_size));

		  int i;
		  char c;
		  /*Send the file */
		  for(i = 0;i< file_size;i++){
		    fread(&c,sizeof(char),1,file);
		    Write(connfd,&c,sizeof(char));
		  }
		  uint32_t ts = htonl(file_stat.st_mtim.tv_sec);
		  Write(connfd,&ts,sizeof(ts));
		      fclose (file);
		      break;
		}
	      }
	    }
	    else {
	      printf("(%s) no client request in 15 sec: timeout expired\n",prog_name);
	      return 1;
	    }
	   }

  Close(connfd);

	return 0;
}
