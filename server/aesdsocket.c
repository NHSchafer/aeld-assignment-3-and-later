/*
** server.c -- a stream socket server demo
*/

//for accept4()
#define _GNU_SOURCE             /* See feature_test_macros(7) */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>
#include <stdbool.h>

#define PORT "9000"  // the port users will be connecting to

#define BACKLOG 20   // how many pending connections queue will hold

#define MAXDATASIZE 100 // max number of bytes we can get at once

bool caught_sigint = false;
bool caught_sigterm = false;


static void signal_handler( int signal_number)
{
        if( signal_number == SIGINT)
                caught_sigint=true;
        else if( signal_number == SIGTERM)
                caught_sigterm=true;
}




// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main( int argc, char **argv)
{

    //start syslog
    openlog(NULL, 0, LOG_USER);

	// listen on sock_fd, new connection on new_fd
	int sockfd, new_fd, numbytes;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address info
	socklen_t sin_size;
	//struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	char buf[MAXDATASIZE];
	int rv;
	const char *filepath = "/var/tmp/aesdsocketdata";
	FILE *temp_fd;

	//signal handling vars
        struct sigaction new_action;
        //bool success = true;
        memset( &new_action, 0, sizeof(struct sigaction));
        new_action.sa_handler=signal_handler;


	//setup SIGTERM and SIGINT handler
        if(sigaction(SIGTERM, &new_action, NULL) !=0){
		syslog(LOG_ERR, "Error %d (%s) registering for SIGTERM", errno, strerror(errno));
                //printf("Error %d (%s) registering for SIGTERM", errno, strerror(errno));
                //success = false;
        }

        if(sigaction(SIGINT, &new_action, NULL) !=0){
		syslog(LOG_ERR, "Error %d (%s) registering for SIGINT", errno, strerror(errno));
                //printf("Error %d (%s) registering for SIGINT", errno, strerror(errno));
                //success = false;
        }




	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP


	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(-1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(-1);
	}

//listen for connection
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(-1);
	}


	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		   // syslog(LOG_DEBUG, "PRE Accept\n");
		new_fd = accept4(sockfd, (struct sockaddr *)&their_addr,
				&sin_size, SOCK_NONBLOCK);
		if (new_fd == -1) {
		   // syslog(LOG_DEBUG, "FAILED Accept\n");
		    
		    if(caught_sigint || caught_sigterm){
		        syslog(LOG_INFO, "Caught signal, exiting");
		        //delete file
		        if(remove(filepath) != 0)
		    	syslog(LOG_ERR, "Error deleting file");

		        //close connections
		        //close(new_fd);
		        close(sockfd);
		        syslog(LOG_DEBUG, "Closed sockets");
		        //end program
		        break;
		    }
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);
		syslog(LOG_INFO, "Accepted connection from %s\n", s);

		//open file
		temp_fd = fopen(filepath, "ab+");
		//error check on file open and log message
	        if(temp_fd == NULL)//error check
   	        {
       		    syslog(LOG_ERR, "fopen failure\n");
        	    //printf("Error on file open\n");
        	    return -1;
    		}
		syslog(LOG_DEBUG, "Moving to receive data\n");

		//recieve data and save to file
		do{
		    if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1) {
        	        perror("recv");
			syslog(LOG_DEBUG, "Failed receive data: %d\n", numbytes);
       		        //exit(-1);

	            }else{
		        //paste to open file
	    		//append to file
    			//fr = fprintf(fd, "%s", buf);
    			if(fwrite(buf, 1, numbytes, temp_fd)<0)
    			{
    			    syslog(LOG_ERR, "Writing failure\n");
    			    //printf("Error on writing to file\n");
    			    return -1;

    			}
	
		
		    }
		}while(buf[numbytes-1] != '\n');//keep reading until \n received   

    		syslog(LOG_DEBUG, "Received all data and Wrote to file\n");

		//read data from file and send
		rewind(temp_fd);//return to beginning of file
		while((numbytes = fread(buf, 1, MAXDATASIZE, temp_fd)))
		{
     		    syslog(LOG_DEBUG, "Read %d bytes\n", numbytes);
		    if(send(new_fd, buf, numbytes, 0) == -1)
    			syslog(LOG_ERR, "Sending failure\n");


		
		}

		fclose(temp_fd);
		syslog(LOG_INFO, "Closed temp write file: %s\n", filepath);

//		if (!fork()) { // this is the child process
//			close(sockfd); // child doesn't need the listener
//			if (send(new_fd, "Hello, world!", 13, 0) == -1)
//				perror("send");
//			close(new_fd);
//			exit(0);
//		}
//		close(new_fd);  // parent doesn't need this
//
		syslog(LOG_INFO, "Closed connection from %s\n", s);
		syslog(LOG_DEBUG, "SigInt %d & SigTerm %d\n", caught_sigint, caught_sigterm);
	        
	}
      	syslog(LOG_DEBUG, "Outside main loop");

	return 0;
}

