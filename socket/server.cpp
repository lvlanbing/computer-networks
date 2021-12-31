#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>


void* communication(void* arg) {
    int temp;
    memcpy(&temp,(void *)arg, 4);
    int connect_socket = temp;
    char send_buff[1024], recv_buff[1024];

    while (1) {
        int data = recv(connect_socket, recv_buff, 1024, 0);
        recv_buff[data] = '\0';
        if(strcmp(recv_buff, "quit") == 0) {
            printf("%d disconnect \n", connect_socket);
            break;
        }
        printf("client: %s\n", recv_buff);

        printf("Input information\n");
        scanf("%s", send_buff);
        send(connect_socket, send_buff, strlen(send_buff), 0);
    }

    close(connect_socket);
}


int main() {
    int server_socket, connect_socket;
    struct sockaddr_in server_addr, connect_addr;
    u_short port = 2333;
    int addr_len = sizeof(connect_addr);
    pthread_t newthread;

    // 创建socket
    if((server_socket= socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return 0;
    }


    //本地数据头部填写
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr= INADDR_ANY;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // server_socket与server_addr绑定，之后通信不用再填写头部数据，而是使用server_socket代替
    if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return 0;
    }

    if(listen(server_socket, 5) < 0) {
        perror("listen");
        return 0;
    }

    
    // 等待接收客户端的连接
    while (1)
    {
        connect_socket = accept(server_socket, (struct sockaddr*)&connect_addr, (socklen_t  *)&addr_len);
        if(connect_socket < 0) {
            perror("accept");
            return 0;
        } else {
            printf("%d connect ok!\n", connect_socket);
        }
        
        // communication(&connect_socket);
        if(pthread_create(&newthread, NULL, communication, &connect_socket)!= 0) {
            perror("pthread_create\n");
        }              
    }

    close(connect_socket);
    return 0;

}   