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

#define MAX_BUFF 100
#define MAX_NAME 50
#define TIMEOUT 15

#define MSG_OK "+OK\r\n"

char *prog_name;

int main (int argc, char *argv[])
{

        int sockfd;
	char *dest;
	short port;
	int n;
	struct in_addr *addr;
	struct sockaddr_in server;
	char buf[MAX_NAME+1];
	char *file_name;
	fd_set set;
	struct timeval tv;
	char recv_buff[MAX_BUFF];
	uint32_t file_size;

	prog_name = argv[0];

	/*check arguments*/
	if (argc < 4)
	  err_quit("(%s) wrong parameters, usage: <dest> <port> <filenames>\n",prog_name);

	dest = argv[1];
	port = atoi(argv[2]);
	
	/* create socket*/
	if ( (n = inet_aton(dest,addr)) < 0)
	  err_quit("(%s) error in the IP address\n",prog_name);
	sockfd = Socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

	/*specify the address*/
	memset(&server,0,sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = addr->s_addr;

	printf("(%s) socket created\n",prog_name);

	/*Connect the socket*/
	Connect(sockfd,(SA*) &server,sizeof(server));
	printf("(%s) connected to %s:%u\n",prog_name,dest,port);

	/*ciclo per ogni file richiesto*/
	for (int it = 3; it!=argc; it++){
	 
	  file_name = argv[it];
	  sprintf(buf,"GET %s\r\n",file_name);

	  Write(sockfd,buf,strlen(buf));
	  printf("(%s) sent string %s\n",prog_name,buf);

	  /*attendo risposta del server*/
	  FD_ZERO(&set);
	  FD_SET(sockfd, &set); 
	  tv.tv_sec = TIMEOUT;
	  tv.tv_usec = 0;

	  if ( Select(FD_SETSIZE, &set, NULL,NULL, &tv) > 0){
	    int nread=0;
	    char c;

	    /*leggo la risposta del server*/
	    for( int j = 0; j<5; j++){
	      Read(sockfd,&c,sizeof(c));
	      recv_buff[j] = c;
	      nread++;
	    }

	    recv_buff[nread]='\0';
	    printf("(%s) string received: %s\n",prog_name,recv_buff);
	    if ( strcmp(recv_buff,MSG_OK) == 0){
		/* leggo dimensione del file*/
		uint32_t size;
		Read(sockfd,&size,sizeof(size));
	        file_size = ntohl(size);
		printf("(%s) received file size %u\n",prog_name,file_size);

		FILE* file;
	        if((file = fopen(file_name, "wb")) == NULL){
		  err_quit("(%s) cannot open the file\n",prog_name);
		}
		for(int k=0;k<file_size;k++) {
		  Read(sockfd,&c,sizeof(c));
		  fwrite(&c,sizeof(c),1,file);
		}

		fclose(file);
		printf("(%s) received file %s\n",prog_name,file_name);
	
		uint32_t ts1,ts;
		Read(sockfd,&ts1,sizeof(ts1));
		ts = ntohl(ts1);
		printf("(%s) received last modification %u\n",prog_name,ts);

	      }else {
		err_quit("(%s) protocol error, received string %s\n",prog_name,recv_buff);
	      }
	    }
	    else {
	      err_quit("(%s) timeout, no response from server in 15 se\n",prog_name); 
	    }

	}

	  Close(sockfd);
	return 0;
}
