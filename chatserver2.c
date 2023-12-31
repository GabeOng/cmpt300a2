#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include "list.h"
#include <stdlib.h>
//int mport=6004;
//int yport=6005;
//how do we allow all threads to access variables
//like arguments taken from main
//also not sure how the for loop is gonna work for this

pthread_mutex_t mutexlist, mutexbuff, mutexflag;
int mfd=0;
Node* voidP;
struct sockaddr_in myaddr;
struct sockaddr_in youaddr;
char* buffSending="";
char* buffRec = "";
int flag = 0;
char* getIP(char* hostName){
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((status = getaddrinfo(hostName, NULL, &hints, &res)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return NULL;
    }

    
    for(p = res; p != NULL; p = p->ai_next){
        void *addr;
        if(p->ai_family == AF_INET){
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
        }
        else{
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;;
            addr = &(ipv6->sin6_addr);
        }
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
    }
    freeaddrinfo(res);

    char* result = malloc(strlen(ipstr)+1);
    strcpy(result, ipstr);
    return result;
}

void* readingInput(void *sendingList){
    pthread_mutex_lock(&mutexlist);
    printf("enter the message: ");
    
    fgets(buffSending,1024,stdin);
    buffSending[strcspn(buffSending,"\n")]='\0';
    if(strcmp(buffSending, "!") == 0){
        pthread_mutex_lock(&mutexflag);
        flag = 1;
        pthread_mutex_unlock(&mutexflag);
        return NULL;
    }
    List_append(sendingList, &buffSending);

    pthread_mutex_unlock(&mutexlist);
    
}

void* sendMessage(void *sendingList){
    //pull a message out ofthe list and assign it to buff
    pthread_mutex_lock(&mutexbuff);
    
    voidP = List_remove(sendingList);
    if(voidP){
        buffSending = (voidP)->pItem;      
        if(sendto(mfd, buffSending, 1024, 0, (struct sockaddr*)&youaddr, (size_t)sizeof(youaddr))<0){
            perror("sendto failed");
        }
    }       
    pthread_mutex_unlock(&mutexbuff);
}

void* receiveMessage(void *receivingList){
    pthread_mutex_lock(&mutexbuff);
    socklen_t size=sizeof(myaddr);
    if(recvfrom(mfd, buffRec, 1024,0,(struct sockaddr*)&myaddr,&size)<0){
        perror("received failed");
    }
    List_append(receivingList, &buffRec);

    pthread_mutex_unlock(&mutexbuff);
}

void* printMessage(void *receivingList){
    //pull a message out of the list and assign it to buff
    pthread_mutex_lock(&mutexbuff);
    voidP = List_remove(receivingList);
    
    if(voidP){
        buffRec = voidP->pItem;
        if(buffRec){
            ;
        }else{
            printf("received message: %s \n", buffRec);            
        }
        
    }
    pthread_mutex_unlock(&mutexbuff);
}

int main (int argc, char *argv[]){
    buffSending=(char*)malloc(sizeof(char)*1024);
    buffRec=(char*)malloc(sizeof(char)*1024);
    pthread_t readInput, sendMsg, receiveMsg, printMsg;
    int iret1, iret2, iret3, iret4;
    List* sendingList = List_create();
    List* receivingList = List_create();
    pthread_mutex_init(&mutexlist, NULL);
    pthread_mutex_init(&mutexbuff, NULL);
    int mport = atoi(argv[1]);
    char* ip = getIP(argv[2]);
    if(!ip){
        return 0;
    }
    int yport = atoi(argv[3]);
    //my endpoint socket
    if((mfd=socket(AF_INET, SOCK_DGRAM, 0))<0){
        perror("cannot create socket");
        return 0;
    }

    //destination socket

    //setting up my addr and binding it to a socket
    memset((char*)&myaddr,0,sizeof(myaddr));
    myaddr.sin_family = AF_INET; //protocall
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY); //use my ip address
    myaddr.sin_port=htons(mport);//my port
    if(bind(mfd, (struct sockaddr*)&myaddr, sizeof(myaddr))<0){//binding my socket
        perror("bind failed");
        return 0;
    }
    
    //setting up destination addr
    memset((char*)&youaddr,0,sizeof(youaddr));
    youaddr.sin_family = AF_INET; //protocall
    youaddr.sin_port=htons(yport);//their port
    if (inet_aton(ip, &youaddr.sin_addr)==0) {//setting destination addr
		fprintf(stderr, "inet_aton() failed\n");
	}

    for(;;){
        iret1 = pthread_create(&readInput, NULL, readingInput, (void*)sendingList);
        pthread_join(readInput, NULL);
        if(flag){
            break;
        }
        iret2 = pthread_create(&sendMsg, NULL, sendMessage, (void*)sendingList);
        pthread_join(sendMsg, NULL);
        iret3 = pthread_create(&receiveMsg, NULL, receiveMessage, (void*)receivingList);
        pthread_join(receiveMsg, NULL);
        iret4 = pthread_create(&printMsg, NULL, printMessage, (void*)receivingList);
        pthread_join(printMsg,NULL);
    }
    free(ip);
    free(buffSending);
    free(buffRec);
}
