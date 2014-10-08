#include <iostream>
#include <queue>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include "header.h"

#define CONNECT_QUEUE 20
#define SERVER_PORT 8080
#define THREAD_WORKERS 100

std::queue <pthread_t> Workers ;

void initWorkers() {
    pthread_t workers[THREAD_WORKERS] ;
    for(int i = 0; i < THREAD_WORKERS; i++) {
        Workers.push(workers[i]) ;
    }
}

pthread_t getWorker() {
    pthread_t worker = Workers.front() ;
    Workers.pop() ;
    return worker ;
}

static void *threadHelper(void *arg) {
    int conn = *(int*)arg ;  
    Header hd(conn) ;
    hd.serviceRequest() ;
    shutdown(conn, 2) ;
    if (close(conn) == -1) {
        std::cout << "Unable to close the socket!" << std::endl ;
    }
    Workers.push(pthread_self()) ;
    return NULL ;
}

int main(int argc, char* argv[]) {

    // Create Worker Pool
    initWorkers() ;
    sem_t workPool ;
    sem_init(&workPool, 0, THREAD_WORKERS) ;

    // Create the Socket
    int sockfd, set = 1 ;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        std::cout << "Unable to create a socket!" << std::endl ;
        return 1 ;
    }
    setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) ;

    // Initialize the socket structure
    struct sockaddr_in address ;
    bzero(&address, sizeof(address)) ;
    address.sin_family      = AF_INET ;
    address.sin_addr.s_addr = INADDR_ANY ;
    address.sin_port        = htons(SERVER_PORT) ; 

    // Bind socket address to socket
    if (bind(sockfd, (struct sockaddr *) &address, sizeof(address)) == -1) {
        std::cout << "Cannot bind to the socket!" << std::endl ;
        return 1 ;
    }

    // Convert the socket to a listening socket
    if (listen(sockfd, CONNECT_QUEUE) == -1) {
        std::cout << "Unable to make the socket a listening socket!" << std::endl ;
        return 1 ;
    }

    // Loop indefintely to accept any connections
    while (true) {
        int conn ;
        struct sockaddr_in client ;
        socklen_t clientLen = sizeof(client) ;
        bzero(&client, clientLen) ;
        if ((conn = accept(sockfd, (struct sockaddr *) &client, &clientLen)) == -1) {
            std::cout << "Unable to accept any connections!" << std::endl ;
            return 1 ;
     	}
        //fcntl (conn, F_SETFL, fcntl(conn, F_GETFL) | O_NONBLOCK) ;  

        sem_wait (&workPool) ;
        pthread_t thread = getWorker () ;
        pthread_create (&thread, NULL, threadHelper, &conn) ;
        sem_post (&workPool) ;
    }

    // Should never get here
    sem_destroy(&workPool) ;    
    pthread_exit(NULL) ;
    return 1 ;
}
