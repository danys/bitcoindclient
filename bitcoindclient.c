#include <stdio.h>
#include <stdlib.h> /* exit() function */
#include <arpa/inet.h> /* int_pton() function */
#include <sys/socket.h> /* socket() and connect() functions */
#include <unistd.h> /* read(), write(), close() functions */
#include <string.h> /* sizeof() function*/

int main(int argc, char* argv[])
{
	/* Connection parameters and variable declarations */
	char* ip = "127.0.0.1";
    uint16_t port = 8332;
    char *baseMessage = "POST %s HTTP/1.0\r\n\r\n";
    struct sockaddr_in serverAddr;
    
    int socketFd, bytes, sent, received, total;
    char message[1024],response[4096];

    int ret;
    /* Creating a socket */
    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd==-1)
    {
      printf("Error creating socket!\n");
      exit(EXIT_FAILURE);
    }
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    ret = inet_pton(AF_INET, ip, &serverAddr.sin_addr);
    if (ret!=1)
    {
    	printf("Unable to convert given IP address!\n");
     	exit(EXIT_FAILURE);
    }
    /* Connect to server */
 	ret = connect(socketFd, (struct sockaddr *)&serverAddr, sizeof serverAddr);
 	if (ret==-1)
    {
    	printf("Can't connect to remote server!\n");
     	exit(EXIT_FAILURE);
    }
    printf("Successfully connected to remote server!\n");
    /* Send a request */
    char* msg = "{\"jsonrpc\":1.0,\"id\":\"req\",\"method\":\"getinfo\",\"params\":[]}";
    char* finalMsg = (char*)malloc((strlen(baseMessage)+strlen(msg)+1)*sizeof(char));
    sprintf(finalMsg,baseMessage,msg);
    printf("Sending message : %s\n",finalMsg);
    total = strlen(finalMsg);
    sent = 0;
    do {
        bytes = write(socketFd,message+sent,total-sent);
        if (bytes < 0)
            error("ERROR writing message to socket");
        if (bytes == 0)
            break;
        sent+=bytes;
    } while (sent < total);

    /* receive the response */
    memset(response,0,sizeof(response));
    total = sizeof(response)-1;
    received = 0;
    do {
        bytes = read(socketFd,response+received,total-received);
        if (bytes < 0)
            error("ERROR reading response from socket");
        if (bytes == 0)
            break;
        received+=bytes;
    } while (received < total);

    if (received == total)
        error("ERROR storing complete response from socket");
    free(finalMsg); /* Clean up */
    /* close the socket */
    ret = close(socketFd);
	if (ret==-1)
    {
    	printf("Error closing socket!\n");
     	exit(EXIT_FAILURE);
    }
    printf("Closed socket successfully!\n");
    /* process response */
    //printf("Response:\n%s\n",response);

    return 0;
}