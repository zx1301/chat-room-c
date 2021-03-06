#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <pthread.h>

#define PORT_NUM 1004

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void* thread_main_recv(void* args)
{
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep receiving and displaying message from server
	char buffer[512];
	int n;

	memset(buffer, 0, 512);
	n = recv(sockfd, buffer, 512, 0);
	if (n < 0) error("ERROR client recv() failed 1");
	while (n > 0) {
		printf("\n%s\n", buffer);

		memset(buffer, 0, 512);
		n = recv(sockfd, buffer, 512, 0);
		if (n < 0) error("ERROR client recv() failed");
		
		//if (strlen(buffer) == 1) buffer[0] = '\0';

	}

	return NULL;
}

void* thread_main_send(void* args)
{
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep sending messages to the server
	char buffer[512];
	int n;

	while (1) {
		// You will need a bit of control on your terminal
		// console or GUI to have a nice input window.
		printf("\nPlease enter the message: ");
		memset(buffer, 0, 512);
		fgets(buffer, 512, stdin);

		if (strlen(buffer) == 1) buffer[0] = '\0';

		n = send(sockfd, buffer, strlen(buffer), 0);
		if (n < 0) error("ERROR writing to socket");

		if (n == 0) break; // we stop transmission when user type empty string
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	if (argc < 2) error("Please specify hostname");

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);

	printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));

	int status = connect(sockfd, 
			(struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");
	
	//prompt user for name
	char buffer[512];
	char roomreply[512];
	int n;
	printf("\nPlease enter your name: ");
	memset(buffer, 0, 512);
	fgets(buffer, 512, stdin);
	buffer[strlen(buffer) - 1] = '\0';

	//send name
	n = send(sockfd, buffer, strlen(buffer), 0);
	if (n < 0) error("ERROR sending name");

	//receive room info
	memset(buffer, 0, 512);
	n = recv(sockfd, buffer, 512, 0);
	if (n < 0) error("receiving room info failed\n");
	//printf("received room info's length %ld\n", strlen(buffer));
	if (strcmp(buffer, "x") != 0) printf("Room info:\n%s\n", buffer);

	//send reply for room info
	memset(roomreply, 0, 512);
	if (strcmp(buffer, "x") == 0){
		strcpy(roomreply, "new");
		n = send(sockfd, roomreply, strlen(roomreply), 0);
		printf("First client, room #1 being created\n");
	}
	else{

		memset(roomreply, 0, 512);
		printf("Please enter a number for an existing room,\n a room number not yet made for random join,\n or 'new' for a new room:\n");
		fgets(roomreply, 512, stdin);
		roomreply[strlen(roomreply) - 1] = '\0';
		n = send(sockfd, roomreply, strlen(roomreply), 0);
		printf("Joining room\n");
	}
	
	pthread_t tid1;
	pthread_t tid2;

	ThreadArgs* args;
	
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid1, NULL, thread_main_send, (void*) args);

	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid2, NULL, thread_main_recv, (void*) args);

	// parent will wait for sender to finish (= user stop sending message and disconnect from server)
	pthread_join(tid1, NULL);

	close(sockfd);

	return 0;
}

