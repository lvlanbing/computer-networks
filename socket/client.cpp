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

int main () {
    int client_socket;
    struct sockaddr_in client_addr;

    // 创建socket
    if((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return 0;
    }

    // 填写发送请求的头部信息
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client_addr.sin_port = htons(2333);
    client_addr.sin_family = AF_INET;

    // 客户端不用bind，os会自动bind

    // 发起请求连接
    if(connect(client_socket, (struct sockaddr *)&client_addr, sizeof(client_addr))< 0) {
        perror("connect");
        return 0;
    }

    // 现在双方建立起了连接，但是还没有开始交流
    char send_buff[1024], recv_buff[1024];
    // 交流
    while(1) {
        memset(send_buff, 0, sizeof(send_buff));
        printf("Input information\n");
        scanf("%s", send_buff);
        send(client_socket, send_buff, strlen(send_buff), 0);
        if(strcmp(send_buff, "quit") == 0) {
            break;
        }

        int data = recv(client_socket, recv_buff, 1024, 0);
        recv_buff[data] = '\0';
        printf("server: %s\n", recv_buff);
    }

    close(client_socket);
    return 0;
}