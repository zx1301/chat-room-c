#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT_NUM 1004

int MAXROOM = 0;

const char *colors[6] = {"[0;31m", "[0;32m", "[0;33m", "[0;34m", "[0;35m", "[0;36m"};

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

typedef struct _USR {
	int clisockfd;		// socket file descriptor
	char* name;
	int room;
	int color;
	struct _USR* next;	// for linked list queue
} USR;

USR *head = NULL;
USR *tail = NULL;

void add_tail(int newclisockfd)
{
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		head->next = NULL;
		tail = head;
	} else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		tail->next->next = NULL;
		tail = tail->next;
	}
}

void broadcast(int fromfd, char* message)
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

	//find sender
	USR* sender = head;
	while (sender != NULL){
		if (sender->clisockfd == fromfd) break;
		sender = sender->next;
	}
	if (sender == NULL) error("sender was not found");

	// traverse through all connected clients
	USR* cur = head;
	while (cur != NULL) {
		// check if cur is not the one who sent the message
		if (cur->clisockfd != fromfd && sender->room == cur->room) {
			char buffer[512];

			// prepare message
			sprintf(buffer, "\033%s[%s]:%s\033[0m", colors[sender->color], sender->name, message);
			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed at broadcast");
		}

		cur = cur->next;
	}
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	//-------------------------------
	// Receive name
	char buffer[512];
	char name[512];
	char room[512];
	int nsen, nrcv;

	memset(buffer, 0, 512);
	memset(name, 0, 512);
	nrcv = recv(clisockfd, buffer, 512, 0);
	if (nrcv < 0) error("ERROR recv() failed at name");

	//finding given USR in linked list
	USR* cur = head;
	while(cur != NULL){
		if(cur->clisockfd == clisockfd) break;
		cur = cur->next;
	}
	if (cur == NULL) error("Error with finding user that needs name");

	strcpy(name, buffer);
	cur->name = name;
	printf("%s has connected\n", name);

	//Send room info
	memset(buffer, 0, 512);
	buffer[0] = 'x';
	if (MAXROOM == 0){
		nsen = send(clisockfd, buffer, strlen(buffer), 0);
		if (nsen < 0) error("ERROR sending empty room info");
		//printf("empty room info's length: %ld\n", strlen(buffer));
	}
	else{
		memset(buffer, 0, 512);
		int rooms[MAXROOM];
		int i;
		for(i = 0; i < MAXROOM; ++i){
			rooms[i] = 0;
		}
		USR* curt = head;
		while(curt != NULL){
			if (curt->room == -1){
				curt = curt->next;
				continue;
			}
			rooms[curt->room - 1]++;
			curt = curt->next;
		}
		for(i = 0; i < MAXROOM; ++i){
			sprintf(buffer + strlen(buffer), "Room %d: %d people\n", i+1, rooms[i]);
		}
		nsen = send(clisockfd, buffer, strlen(buffer), 0);
		if(nsen < 0) error("Error sending room info");
	}

//	printf("room info sent\n");
	//Receive room info
	memset(buffer, 0, 512);
	memset(room, 0, 512);
	nrcv = recv(clisockfd, buffer, 512, 0);
	if (nrcv < 0) error("ERROR server recv() failed at room info");

	strcpy(room, buffer);
	//client creates and joins a new room
	if (strcmp(room, "new") == 0){
		MAXROOM++;
		cur->room = MAXROOM;
//		printf("receive new from client\n");
	}
	//client joins random existing room
	else if(atoi(room) > MAXROOM){
		srand(time(0));
		int randnum = rand();
		randnum = randnum % MAXROOM;
		if (randnum == 0){
			randnum = 1;
		}
		cur->room = randnum;
	}
	//Client joins desired existing room
	else{
		cur->room = atoi(room);
	}
	
	printf("%s wants room %s\n", name, room);
	printf("%s got room %d\n", name, cur->room);

	int randnum;
	while(1){
		randnum = rand();
		randnum = randnum % 6;
		USR* curt = head;
		while(curt != NULL){
			if (curt->color == randnum && curt->room == cur->room) break;
			curt = curt->next;
		}
		if (curt == NULL) break;
	}

	cur->color = randnum;

	//start receiving msgs
	memset(buffer, 0, 512);
	nrcv = recv(clisockfd, buffer, 512, 0);
	if (nrcv < 0) error("ERROR recv() failed at msg 1");
	while (nrcv > 0) {
		// we send the message to everyone except the sender
		broadcast(clisockfd, buffer);

		memset(buffer, 0, 512);
		nrcv = recv(clisockfd, buffer, 512, 0);
		if (nrcv < 0) error("ERROR recv() failed at msg");
	}
	
	//disconnected user is not in a room
	cur->room = -1;

	close(clisockfd);
	//-------------------------------
	printf("%s has disconnected\n", name);

	return NULL;
}

int main(int argc, char *argv[])
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	//serv_addr.sin_addr.s_addr = inet_addr("192.168.1.171");	
	serv_addr.sin_port = htons(PORT_NUM);

	int status = bind(sockfd, 
			(struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");

	listen(sockfd, 10); // maximum number of connections = 10

	while(1) {
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");

		printf("Connected: %s\n", inet_ntoa(cli_addr.sin_addr));
		add_tail(newsockfd); // add this new client to the client list

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	return 0; 
}

