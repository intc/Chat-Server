/* Simple Chat Server
 * Forked from: https://github.com/yorickdewid/Chat-Server / Yorick de Wid
 * (C) 2019
 * Authors: Antti Antinoja, Yorick de Wid
 * Version: 0.1a
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
			/* mt-safe: sprintf, printf	*/
#include <stdlib.h>
			/* mt-safe: free, exit */
#include <unistd.h>
			/* read, write */
#include <errno.h>
#include <string.h>
			/* mt-safe: strlen, strcpy, strcmp, strtok_r, strcat */
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENTS	3
#define DELIM " "
#define LPORT 5000

static unsigned int cli_count = 0;
static int uid = 10;

			/* Client structure */
typedef struct clinet_t {
	struct sockaddr_in addr;    /* Client remote address */
	int connfd;                 /* Connection file descriptor */
	int uid;                    /* Client unique identifier */
	char name[32];              /* Client name */
	pthread_mutex_t *lock;      /* Shared lock */
} client_t;

void queue_add(client_t *, pthread_mutex_t *);
void *handle_client(void *);
void queue_delete(int, pthread_mutex_t *);
int send_message(char *, int, pthread_mutex_t *);
int send_message_all(char *, pthread_mutex_t *);
int send_message_self(const char *, int);
int send_message_client(char *, int, pthread_mutex_t *);
void send_active_clients(int, pthread_mutex_t *);
void strip_newline(char *);
void print_client_addr(struct sockaddr_in);
void client_count_mod(int, pthread_mutex_t *);

client_t *clients[MAX_CLIENTS];

			/* Add client to queue */
void queue_add(client_t *cl, pthread_mutex_t *lock){
	int i;
	pthread_mutex_lock(lock);
	for(i=0;i<MAX_CLIENTS;i++){
		if(!clients[i]){
			clients[i] = cl;
			pthread_mutex_unlock(lock);
			return;
		}
	}
}
			/* Delete client from queue */
void queue_delete(int uid, pthread_mutex_t *lock){
	int i;
	pthread_mutex_lock(lock);
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				pthread_mutex_unlock(lock);
				return;
			}
		}
	}
}
			/* Send message to all clients but the sender */
int send_message(char *s, int uid, pthread_mutex_t *lock){
	int i, ret = 0;
	pthread_mutex_lock(lock);
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid != uid){
				if(write(clients[i]->connfd, s, strlen(s))<0){
					perror("send_message() write");
					ret = -1;
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(lock);
	return ret;
}
			/* Send message to all clients */
int send_message_all(char *s, pthread_mutex_t *lock){
	int i, ret = 0;
	pthread_mutex_lock(lock);
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(write(clients[i]->connfd, s, strlen(s))<0){
				perror("send_message_all() write");
				ret = -1;
				break;
			}
		}
	}
	pthread_mutex_unlock(lock);
	return ret;
}
			/* Send message to sender */
int send_message_self(const char *s, int connfd){
	if(write(connfd, s, strlen(s))<0){
		perror("send_message_self() write");
		return -1;
	}
	return 0;
}
			/* Send message to client
			 * return values are:
			 *   -2: user not found
			 *   -1: write failed
			 *    0: OK */
int send_message_client(char *s, int uid, pthread_mutex_t *lock){
	int i, ret = -2;
	pthread_mutex_lock(lock);
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid == uid){
				if(write(clients[i]->connfd, s, strlen(s))<0){
					perror("send_message_client() write");
					ret = -1;
					break;
				}
				ret = 0;
				break;
			}
		}
	}
	pthread_mutex_unlock(lock);
	return ret;
}
			/* Send list of active clients */
void send_active_clients(int connfd, pthread_mutex_t *lock){
	int i;
	char s[64];
	pthread_mutex_lock(lock);
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			sprintf(s, "<<CLIENT %d | %s\r\n", clients[i]->uid, clients[i]->name);
			send_message_self(s, connfd);
		}
	}
	pthread_mutex_unlock(lock);
}
			/* Strip CRLF */
void strip_newline(char *s){
	while(*s != '\0'){
		if(*s == '\r' || *s == '\n'){
			*s = '\0';
		}
		s++;
	}
}
			/* Print ip address */
void print_client_addr(struct sockaddr_in addr){
	printf("%d.%d.%d.%d",
		addr.sin_addr.s_addr & 0xFF,
		(addr.sin_addr.s_addr & 0xFF00)>>8,
		(addr.sin_addr.s_addr & 0xFF0000)>>16,
		(addr.sin_addr.s_addr & 0xFF000000)>>24);
}

void client_count_mod(int num, pthread_mutex_t *lock){
	pthread_mutex_lock(lock);
	cli_count = cli_count + num;
	pthread_mutex_unlock(lock);
}
			/* Handle all communication with the client */
void *handle_client(void *arg){
	char buff_out[2048];
	char buff_in[1024];
	int rlen;
	client_t *cli = (client_t *)arg;

	client_count_mod(+1, cli->lock);

	printf("INFO: New connection ");
	print_client_addr(cli->addr);
	printf(" Id %d\n", cli->uid);

	sprintf(buff_out, "<<INFO: Hello %s\r\n", cli->name);
	send_message_all(buff_out, cli->lock);

			/* Receive input from client */
	while((rlen = read(cli->connfd, buff_in, sizeof(buff_in)-1)) > 0){
		buff_in[rlen] = '\0';
		buff_out[0] = '\0';
		strip_newline(buff_in);

		printf("%i: read %i bytes.\n", cli->uid, rlen);

			/* Ignore empty buffer */
		if(!strlen(buff_in)){
			continue;
		}

			/* Special options */
		if(buff_in[0] == '\\'){
			char *command = NULL, *param = NULL, *b_pos = NULL;
			command = strtok_r(buff_in, DELIM, &b_pos);
			if(!strcmp(command, "\\exit")){
				break;
			}else if(!strcmp(command, "\\ping")){
				send_message_self("<<INFO: pong\r\n", cli->connfd);
			}else if(!strcmp(command, "\\nick")){
				param = strtok_r(NULL, DELIM, &b_pos);
				if(param){
					char *old_name = malloc( (strlen(cli->name) + 1) * sizeof(char) );
					strcpy(old_name, cli->name);	
					strcpy(cli->name, param);
					sprintf(buff_out, "<<INFO: Renamed %s TO %s\r\n", old_name, cli->name);
					free(old_name);
					send_message_all(buff_out, cli->lock);
				}else{
					send_message_self("<<ERROR: Empty name given\r\n", cli->connfd);
				}
			}else if(!strcmp(command, "\\msg")){
				param = strtok_r(NULL, DELIM, &b_pos);
				if(param){
					int uid = atoi(param);
					param = strtok_r(NULL, DELIM, &b_pos);
					if(param){
						sprintf(buff_out, "[PM][%s]", cli->name);
						while(param != NULL){
							strcat(buff_out, " ");
							strcat(buff_out, param);
							param = strtok_r(NULL, DELIM, &b_pos);
						}
						strcat(buff_out, "\r\n");
						int ret = send_message_client(buff_out, uid, cli->lock);
						if ( ret == -2 )
							send_message_self("<<ERROR: Unknown receiver_id\r\n", cli->connfd);
						if ( ret == -1 )
							send_message_self("<<ERROR (server): Write failed!\r\n", cli->connfd);
					}else{
						send_message_self("<<ERROR: Empty message\r\n", cli->connfd);
					}
				}else{
					send_message_self("<<ERROR: No receiver given\r\n", cli->connfd);
				}
			}else if(!strcmp(command, "\\who")){
				sprintf(buff_out, "<<INFO: Users %d\r\n", cli_count);
				send_message_self(buff_out, cli->connfd);
				send_active_clients(cli->connfd, cli->lock);
			}else if(!strcmp(command, "\\help")){
				strcat(buff_out, "\\exit     Quit chatroom\r\n");
				strcat(buff_out, "\\ping     Server test\r\n");
				strcat(buff_out, "\\nick     <name> Change nickname\r\n");
				strcat(buff_out, "\\msg      <receiver_id> <message> Send private message\r\n");
				strcat(buff_out, "\\who      Show active clients\r\n");
				strcat(buff_out, "\\help     This help\r\n");
				send_message_self(buff_out, cli->connfd);
			}else{
				send_message_self("<<ERROR: Unknown command\r\n", cli->connfd);
			}
		}else{
			/* Send message */
			snprintf(buff_out, sizeof(buff_out), "[%s] %s\r\n", cli->name, buff_in);
			send_message(buff_out, cli->uid, cli->lock);
		}
	}

			/* Close connection */
	sprintf(buff_out, "<<LEAVE, BYE %s\r\n", cli->name);
	send_message_all(buff_out, cli->lock);
	close(cli->connfd);

			/* Delete client from queue and yield thread */
	queue_delete(cli->uid, cli->lock);
	printf("<<LEAVE ");
	print_client_addr(cli->addr);
	printf(" REFERENCED BY %d\n", cli->uid);
	free(cli);
	client_count_mod(-1, cli->lock);
	pthread_detach(pthread_self());

	return NULL;
}

int main(void){
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;
	pthread_mutex_t lock; pthread_mutex_init(&lock, NULL);

			/* Socket settings */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(LPORT);

			/* About SGIPIE
			 * https://www.postgresql.org/message-id/1243914753.517466.918714025754.1.gpush%40pingu
			 */

			/* Bind */
	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		perror("Socket binding failed");
		return 1;
	}

			/* Listen */
	if(listen(listenfd, 10) < 0){
		perror("Socket listening failed");
		return 1;
	}

	printf("<[SERVER STARTED]>\n");

			/* Accept clients */
	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

			/* Check if max clients is reached */
		pthread_mutex_lock(&lock);
		if((cli_count+1) > MAX_CLIENTS){
			printf("<<MAX CLIENTS REACHED\n");
			printf("<<REJECT ");
			print_client_addr(cli_addr);
			printf("\n");
			close(connfd);
			pthread_mutex_unlock(&lock);
			continue;
		} else pthread_mutex_unlock(&lock);

			/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->addr = cli_addr;
		cli->connfd = connfd;
		cli->uid = uid++;
		cli->lock = &lock;
		sprintf(cli->name, "%d", cli->uid);

			/* Add client to the queue and create thread */
		queue_add(cli, &lock);
		pthread_create(&tid, NULL, handle_client, (void*) cli);
	}
}
