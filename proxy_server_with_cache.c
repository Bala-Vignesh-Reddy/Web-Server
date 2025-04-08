#include "proxy_parse.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <netdb.h>
#include <errno.h>

#define MAX_CLIENTS 10 // max no of clients sending request at a time
#define MAX_BYTES 4096 // max no of client request served at time
#define MAX_ELEMENT_SIZE 10 * (1<<10) //max size of element in cache
#define MAX_SIZE 10 * (1 << 20) // size of cache

typedef struct cache_element cache_element;

struct cache_element {
    char *data; // response is stored here.. 
    int len;   // length of the data that is bytes.. 
    char *url; // url of the site visited.. 
    time_t lru_time_track; // lru_time_track shows the latest time the url is accessed..  
    cache_element* next; // this is because we want to create a linked list of cache element.. stores the pointer to next element 
};

//declaring the functions
cache_element *find(char *url);
int add_cache_element(char *data, int size, char *url);
void remove_cache_element();

int port_number = 8080; //default port for our server.. 
int proxy_socketId; //for communication between the server and client we need to setup socketID
pthread_t tid[MAX_CLIENTS]; // initializing threads for multithreading.. each thread will handle a socket..  
sem_t semaphore; // semaphore has been declared.. for locking and stuff.. 
pthread_mutex_t lock; // mutex lock has been used to overcome race condition

cache_element *head; //global head.. can be used to find the cache
int cache_size;

int connectRemoteServer(char* host_addr, int port_number){
    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(remoteSocket < 0){
        printf("Error in creating your socket\n");
        return -1;
    }
    struct hostent *host = gethostbyname(host_addr); // just mapping the ip add
    if(host == NULL){
        fprintf(stderr, "No such host exist\n");
        return -1;
    }
    
    //inserts ip addr and port no. of host in server_addr
    struct sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number); // htons convert int into network  port

    bcopy((char *)host->h_addr, (char *)&server_addr.sin_addr.s_addr, host->h_length);
    if(connect(remoteSocket, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0){
        fprintf(stderr, "Error in connecting\n");
        return -1;
    }
    return remoteSocket;
}

int handle_request(int clientSocket, struct ParsedRequest *request, char *tempReq){
    char *buff = (char *)malloc(sizeof(char)*MAX_BYTES);
    strcpy(buff, "GET ");
    strcat(buff, request->path);
    strcat(buff, " ");
    strcat(buff, request->version);
    strcat(buff, "\r\n");
        
    size_t len = strlen(buff); // calculate the length of string

    if(ParsedHeader_set(request, "Connection", "close") < 0){
        printf("Set header key is not wokring..\n");
    }
    if(ParsedHeader_get(request, "Host") == NULL){
        if(ParsedHeader_set(request, "Host", request->host) < 0){
            printf("Set Host header key is not working..\n");
        }
    }

    if(ParsedRequest_unparse_headers(request, buff+len, (size_t)MAX_BYTES-len) < 0){
        printf("Unparsed Failed\n");
    }

    int server_port = 80; //default server port
    if(request->port != NULL){
        server_port = atoi(request->port);
    }
    int remoteSocketId = connectRemoteServer(request->host, server_port);
    if(remoteSocketId < 0){
        return -1;
    }
    int bytes_send = send(remoteSocketId, buff, strlen(buff), 0);
    bzero(buff, MAX_BYTES);

    bytes_send = recv(remoteSocketId, buff, MAX_BYTES-1, 0);
    char *temp_buffer = (char *)malloc(sizeof(char)*MAX_BYTES);
    int temp_buffer_size = MAX_BYTES;
    int temp_buffer_index = 0;

    while(bytes_send > 0){
        bytes_send = send(clientSocket, buff, bytes_send, 0);
        for(int i=0; i<bytes_send/sizeof(char); i++){
            temp_buffer[temp_buffer_index] = buff[i];
            temp_buffer_index++;
        }
        temp_buffer_size += MAX_BYTES;
        temp_buffer = (char*)realloc(temp_buffer, temp_buffer_size);
        if(bytes_send < 0){
            perror("Error in sending data to client\n");
            break;
        }
        bzero(buff, MAX_BYTES);
        bytes_send = recv(remoteSocketId, buff, MAX_BYTES-1, 0);
    }
    temp_buffer[temp_buffer_index] = '\0';
    free(buff);
    add_cache_element(temp_buffer, strlen(temp_buffer), tempReq);
    printf("Done\n");
    free(temp_buffer);
    close(remoteSocketId);
    return 0;
}

// http version
int checkHTTPversion(char *msg){
    int version = -1;
    if(strncmp(msg, "HTTP/1.1", 8) == 0)
        version = 1;
    else if(strncmp(msg, "HTTP/1,0", 8) == 0)
        version = 1;
    else
        version = 1;

    return version;
}   

int sendErrorMessage(int socket, int status_code)
{
	char str[1024];
	char currentTime[50];
	time_t now = time(0);

	struct tm data = *gmtime(&now);
	strftime(currentTime,sizeof(currentTime),"%a, %d %b %Y %H:%M:%S %Z", &data);

	switch(status_code)
	{
		case 400: snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", currentTime);
				  printf("400 Bad Request\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 403: snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
				  printf("403 Forbidden\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 404: snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
				  printf("404 Not Found\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 500: snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
				  //printf("500 Internal Server Error\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 501: snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
				  printf("501 Not Implemented\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 505: snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
				  printf("505 HTTP Version Not Supported\n");
				  send(socket, str, strlen(str), 0);
				  break;

		default:  return -1;

	}
	return 1;
}

// writing thread function
void* thread_fn(void *socketNew){
    sem_wait(&semaphore); // decrement the value of semaphore and checks if it is negative or not.. 
    int p;
    sem_getvalue(&semaphore, &p); 
    printf("semaphore value:%d\n", p);
    int *t = (int *)(socketNew);
    int socket = *t;
    int bytes_send_client, len;
    
    char *buffer = (char*)calloc(MAX_BYTES, sizeof(char));
    bzero(buffer, MAX_BYTES);
    bytes_send_client = recv(socket, buffer, MAX_BYTES, 0); //receiving request of client from proxy server

    while(bytes_send_client > 0){
        len = strlen(buffer);
        if(strstr(buffer, "\r\n\r\n") == NULL){ // end of string for any url.. 
            bytes_send_client = recv(socket, buffer + len, MAX_BYTES-len, 0);
        }else{
            break;
        }
    }

    char *tempReq = (char *)malloc(strlen(buffer) * sizeof(char) + 1);
    for(int i=0; i<strlen(buffer); i++){
        tempReq[i] = buffer[i];
    }
    struct cache_element *temp = find(tempReq);
    if(temp != NULL){
        int size = temp->len/sizeof(char);
        int pos = 0;
        char response[MAX_BYTES];
        while(pos < size){
            bzero(response, MAX_BYTES);
            for(int i=0; i < MAX_BYTES; i++){
                response[i] = temp->data[pos];
                pos++;
            }
            send(socket, response, MAX_BYTES, 0);
        }
        printf("Data retrieved from the cache\n");
        printf("%s\n\n", response);
    }
    // after accepting the client's request
    else if(bytes_send_client > 0){
        len = strlen(buffer);
        struct ParsedRequest *request = ParsedRequest_create();
        
        if(ParsedRequest_parse(request, buffer, len) < 0) {
            printf("Parsing failed\n");
        }
        else {
            bzero(buffer, MAX_BYTES);
            if(!strcmp(request -> method, "GET")){
                //only checks for the http1.0 version
                if(request->host && request->path && checkHTTPversion(request->version)==1){
                    bytes_send_client = handle_request(socket, request, tempReq);
                    if(bytes_send_client == -1){
                        sendErrorMessage(socket, 500);
                    }
                }
                else {
                        sendErrorMessage(socket, 500);
                }
            }
            else {
                    printf("For now.. this server only accepts GET request\n");
            }
        }
        ParsedRequest_destroy(request);
    }
    else if(bytes_send_client < 0){
        printf("Error in receiving from client\n");
    }
    // no request from the client
    else if(bytes_send_client == 0){
        printf("Client is disconnected");
    }
    shutdown(socket, SHUT_RDWR);
    close(socket);
    free(buffer);
    sem_post(&semaphore);
    sem_getvalue(&semaphore, &p);
    printf("Semaphore post value is %d\n", p);
    free(tempReq);
    return NULL;
}

int main(int argc, char *argv[]){
    int client_socketId, client_len; // socketId and length of client is declared
    struct sockaddr_in server_addr, client_addr; // socket address is created.. 
    sem_init(&semaphore, 0, MAX_CLIENTS); //initializing semaphore
    pthread_mutex_init(&lock, NULL); // initializing locking for thread

    if(argc == 2){ //checking for two arguments
        port_number = atoi(argv[1]); // converts the string to int
    }
    else {
        printf("Too few arguments!! \n");
        exit(1);
    }
    printf("Setting Proxy Server Port: %d\n", port_number);

    //creating socket for proxy
    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0); //using IPv4
    if(proxy_socketId < 0){
        perror("Failed to create a socket\n");
        exit(1);
    }
    int reuse = 1;
    if(setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0){
        perror("setSocketOpt failed\n");
    }
    
    bzero((char *)&server_addr, sizeof(server_addr)); // converting garbage value into zeros
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number); // convert the port number into network understandable form
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if(bind(proxy_socketId, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Port is not avaiable\n");
        exit(1);
    }
    printf("Binding on port %d\n", port_number);
    int listen_status = listen(proxy_socketId, MAX_CLIENTS);
    if(listen_status < 0){
        perror("Error in listening\n");
        exit(1);
    }

    // keeping the track of clients connected
    int i = 0;
    int connected_SocketId[MAX_CLIENTS];
    while(1){
        bzero((char *)&client_addr, sizeof(client_addr));
        client_len = sizeof(client_addr);
        client_socketId = accept(proxy_socketId, (struct sockaddr *)&client_addr, (socklen_t*)&client_len);
        if(client_socketId < 0) {
            printf("Connection not established\n");
            exit(1);
        }
        else {
            connected_SocketId[i] = client_socketId;
        }

        // we are doing this to print the details
        struct sockaddr_in * client_pt = (struct sockaddr_in *)&client_addr;
        struct in_addr ip_addr = client_pt->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
        printf("Client is connected with port number %d and IP address %s\n", ntohs(client_addr.sin_port), str);
        
        // assigning thread to the socket created.. 
        pthread_create(&tid[i],NULL,thread_fn, (void*)&connected_SocketId[i]);
        i++;
    }
    close(proxy_socketId);
    return 0;
}

cache_element *find(char *url){
    cache_element *site = NULL;
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Remove cache lock acquired %d\n", temp_lock_val);
    if(head != NULL){
        site = head;
        while(site != NULL){
            if(!strcmp(site->url, url)){
                printf("LRU time track before: %ld", site-> lru_time_track);
                printf("\n URL found\n");
                site->lru_time_track = time(NULL);
                printf("LRU time track after %ld", site->lru_time_track);
                break;
            }
            site = site->next;
        }
    }
    else {
        printf("url not found\n");
    }
    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("Lock is unlocked\n");
    return site;
}

void remove_cache_element(){
    cache_element *p;
    cache_element *q;
    cache_element *temp;

    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Remove cache lock is locked\n");
    if(head != NULL){
        for(q=head, p=head, temp=head; q->next != NULL; q=q->next){
            if(((q->next)->lru_time_track) < (temp->lru_time_track)){
                temp = q->next;
                p = q;
            }
        }
        if(temp == head)
            head = head->next;
        else
            p->next = temp->next;
        //if cache is not empty searches for the node which has the least lru_time_track and delete that...
        cache_size = cache_size - (temp->len)-sizeof(cache_element) - strlen(temp->url)-1;
        free(temp->data);
        free(temp->url);
        free(temp);
    }
    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("Remove cache lock\n");
}

int add_cache_element(char *data, int size, char *url){
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Add cache lock acquired %d\n", temp_lock_val);
    int element_size = size+1+strlen(url)+sizeof(cache_element);
    if(element_size > MAX_ELEMENT_SIZE){
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Add cache lock is unlocked\n");
        return 0;
    }
    else {
        while(cache_size + element_size > MAX_SIZE){
            remove_cache_element();
        }
        cache_element *element = (cache_element*)malloc(sizeof(cache_element));
        element->data = (char*)malloc(size+1);
        strcpy(element->data, data);
        element->url = (char*)malloc(1 + (strlen(url)*sizeof(char)));
        strcpy(element->url, url);
        element->lru_time_track = time(NULL);
        element->next = head;
        element->len = size;
        head = element;
        cache_size += element_size;
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Add cache lock is unlocked\n");
        return 1;
    }
    return 0;
}
