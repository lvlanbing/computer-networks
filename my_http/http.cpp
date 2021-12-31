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

#define ISspace(x) isspace((int)(x))
#define SERVER_STRING "Server: jdb/0.1.0\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2


void server_file(int connect_socket, const char* filename);
void headers(int client, const char *filename);
void headers2(int client, const char *filename);
void headers3(int client, const char *filename);
void cat(int connect_socket, FILE* resource);
void cat2(int client, FILE *resource);
int get_line(int, char*, int);
void unimplemented(int);
void bad_request(int client);
void not_found(int);
void cannot_execute(int client);
void execute_cgi(int client, const char *path,
        const char *method, const char *query_string);


void* accept_request(void* arg) {
    int temp;
    memcpy(&temp, (void *)arg, 4);
    int connect_socket = temp;
    char recv_buff[1024];
    char method[255]; //请求的方法
    char url[255]; // 用户请求的路径
    char path[512]; //服务器根据请求返回请求文件的路径
    char* query_string = NULL;
    int numchars;
    int cgi = 0;
    int i = 0, j = 0;
    struct stat st;
    
    numchars = get_line(connect_socket, recv_buff, sizeof(recv_buff));
    // printf("%d\n", numchars);
    printf("第一行： %s\n", recv_buff);

    while (!ISspace(recv_buff[i]) && (i < sizeof(method) - 1))
    {
        //把GET 或者 POST 放到method中
        method[i] = recv_buff[i];
        i++;
    }
    j = i;
    method[i] = '\0';
    printf("method %s\n", method);
    if(strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
        //  请求的方法错误, 也要对浏览器反馈信息
        unimplemented(connect_socket);
    }
    // 表单请求 的  方法 为post
    if(strcasecmp(method, "POST") == 0) {
        cgi = 1;
    }
    i = 0;
    while(ISspace(recv_buff[j]) && (j < numchars))
        j++;
    while(!ISspace(recv_buff[j]) && (i < sizeof(url) - 1) && (j < numchars)){
        url[i] = recv_buff[j];
        i++; j++;
    }
    url[i] = '\0';
    if(strcasecmp(method, "GET") == 0) {
        query_string = url;
        while((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if(*query_string == '?') {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }
    sprintf(path, "resource%s", url);
    if(path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
    // if (stat(path, &st) == -1) {
    //     while ((numchars > 0) && strcmp("\n", recv_buff))  /* read & discard headers */
    //         numchars = get_line(connect_socket, recv_buff, sizeof(recv_buff));
    //     not_found(connect_socket);
    // } 
    // else {
    //     if ((st.st_mode & S_IFMT) == S_IFDIR)
    //         strcat(path, "/index.html");
    //     if ((st.st_mode & S_IXUSR) ||
    //             (st.st_mode & S_IXGRP) ||
    //             (st.st_mode & S_IXOTH)    )
    //         cgi = 1;
    //     printf("path %s\n", path);
    //     if(!cgi)
    //         server_file(connect_socket, path);
    //     else
    //         execute_cgi(connect_socket, path, method, query_string);
    // }
    printf("path %s\n", path);
    if(!cgi)
        server_file(connect_socket, path);
    else
        execute_cgi(connect_socket, path, method, query_string);
    
    close(connect_socket);
}

void execute_cgi(int connect_socket, const char* path, 
                const char* method,  const char* query_string) {
    char buf[1024];
    int cgi_output[2];
    int  cgi_input[2];
    pid_t  pid; //进程号
    int status;
    int i; 
    char c;
    int numchars = 1;
    int content_lenght =  1;//描述消息实体的传输长度

    buf[0] = 'A'; buf[1] = '\0';
    if(strcasecmp(method,  "GET") == 0) {
        // 丢弃请求消息
        while ((numchars  > 0) && strcmp("\n", buf))  
        {
            numchars  =  get_line(connect_socket, buf, sizeof(buf));
        }
        
    } else if(strcasecmp(method,  "POST") ==  0) {
        numchars = get_line(connect_socket, buf, sizeof(buf));
        //获得content_length字段的值
        while ((numchars > 0)  && strcmp("\n", buf))        
        {
            buf[15] = '\0';
            if(strcasecmp(buf, "Content-Length:") == 0)
                content_lenght = atoi(&buf[16]);
            numchars =  get_line(connect_socket, buf,  sizeof(buf));
        }
        if(content_lenght == -1) {
            bad_request(connect_socket);
            return;
        }
    }
    else  {

    }
    //创建管道
    if(pipe(cgi_output) < 0)  {
        // 创建失败
        cannot_execute(connect_socket);
        return;
    }
    if(pipe(cgi_input) < 0)  {
        cannot_execute(connect_socket);
        return;
    }
    //  创建子进程
    if((pid = fork()) < 0) {
        cannot_execute(connect_socket);
        return;
    }

    //发送响应消息
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(connect_socket, buf,  strlen(buf), 0);
   
    if(pid  == 0) { /* child: CGI script */
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        dup2(cgi_output[1], STDOUT);
        dup2(cgi_input[0], STDIN);
        close(cgi_output[0]);
        close(cgi_input[1]);
        sprintf(meth_env,  "REQUEST_METHOD=%s", method);
        // putenv函数用来向环境表中添加或修改环境变量
        putenv(meth_env);
        if(strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s",query_string);
            putenv(query_env);
        } else  {
            sprintf(length_env, "CONTENT_LENGTH=%d", content_lenght);
            putenv(length_env);
        }
        //path 为执行文件的路径
        execl(path, NULL);
        exit(0);
    }  else { //父进程
        close(cgi_output[1]);
        close(cgi_input[0]);
        if(strcasecmp(method, "POST") == 0)  {
            for(i = 0; i < content_lenght; i++)  {
                recv(connect_socket,  &c,  1,  0);
                write(cgi_input[1],  &c, 1);
            }
        }
        while (read(cgi_output[0], &c, 1) > 0)
        {
            send(connect_socket, &c, 1, 0);
        }
        close(cgi_output[0]);
        close(cgi_input[1]);
        //等待pid的进程，再执行主进程
        waitpid(pid,  &status,  0);
    }

}

//对请求头部按行进行处理
int get_line(int sock, char* buf, int size) {
    int i = 0;
    char c = '\0';
    int n;

    // 每一行最后的字符为'\n'
    while ((i < size - 1) && (c != '\n'))
    {
        // 一个一个字符的读取
        n = recv(sock, &c, 1, 0);
        // printf("%c \n", c);
        if(n > 0) {
            //每行中有多个字段，使用'\r'让其对齐
            if(c == '\r') {
                // MSG_PEEK : 只读缓存中的数据，而不取出来
                n = recv(sock, &c, 1, MSG_PEEK);
                if((n > 0) && (c == '\n')) {
                    // printf("%c \n", c);
                    recv(sock, &c, 1, 0);
                }
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i]  = '\0';

    return(i);    
}


void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}
//返回400 错误
void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}
//404
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}
//服务器问题
void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

// 图片的请求头部
void headers2(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: image/png\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

void headers3(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: video/mpeg4\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}


void cat(int connect_socket, FILE* resource) {
    char buf[1024];
    
    fgets(buf, sizeof(buf), resource);
    while(!feof(resource)) {
        send(connect_socket, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

//读取图片
void cat2(int client, FILE *resource)
{
    char buf[1024];
    char c;
    while ((fscanf(resource,"%c",&c))!=EOF)
    {
        send(client, &c, 1, 0);
    }
}

void server_file(int connect_socket, const char* filename) {
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];
    
    buf[0] = 'A'; buf[1] = '\0';
    // 读取请求信息，并抛弃它
    while((numchars > 0) && strcmp("\n", buf))
        numchars = get_line(connect_socket, buf, sizeof(buf));
    resource = fopen(filename, "r");

    //路由过程：发送响应头部信息，根据不同的文件类型返回不同的类型的响应包
    if(resource == NULL) {
        printf("open file fail : %s\n", filename);
        return;
    } else if(!strcmp(filename, "resource/test.mp4")){
         headers3(connect_socket, filename);
         cat2(connect_socket, resource);
    } else if(!strcasecmp(filename,  "resource/test.png")) {
        headers2(connect_socket, filename);
        printf("header2 send header ok!\n");
        cat2(connect_socket, resource);
    } else { // html文件 路由
        headers(connect_socket, filename);
        printf("header1 send header ok!\n");
        cat(connect_socket, resource);
    }
    
}

int main() {
    int server_socket, connect_socket;
    struct sockaddr_in server_addr, connect_addr;
    u_short prot = 233;
    int addr_len = sizeof(connect_addr);
    pthread_t newthread;

    // 创建socket
    if((server_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0 ) {
        perror("socket");
        return 0;
    }

    // 填写头部信息
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(prot);

    // server_socket 与 server_addr绑定
    if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
        perror("bind");
        return 0;
    }

    if(listen(server_socket, 5) < 0) {
        perror("listen");
        return 0;
    }

    while(1) {
        connect_socket = accept(server_socket, (struct sockaddr*)&connect_addr, (socklen_t *)&addr_len);
        if(connect_socket < 0) {
            perror("accept");
            return 0;
        } else {
            printf("\n%d connect ok!\n", connect_socket);
        }

        // accept_request(&connect_socket);        
        if(pthread_create(&newthread, NULL, accept_request,  &connect_socket) != 0) {
            perror("pthread_create");
        }
    }
    close(server_socket);
}