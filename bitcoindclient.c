#include <stdio.h> /* Input and output functions */
#include <stdlib.h> /* exit(), atoi() functions */
#include <arpa/inet.h> /* int_pton() function */
#include <sys/socket.h> /* socket() and connect() functions */
#include <unistd.h> /* read(), write(), close() functions */
#include <string.h> /* sizeof() function*/
#include "base64.c" /* base64Encode function */

/* Sends a message given a socket file descriptor and a message to send */
void sendMessage(int socketFd, char* msg)
{
    int total = strlen(msg);
    int sent = 0;
    int bytes;
    do
    {
        bytes = write(socketFd,msg+sent,total-sent);
        if (bytes < 0)
        {
            exit(EXIT_FAILURE);
            printf("ERROR writing message to socket");
        }   
        if (bytes == 0) break;
        sent+=bytes;
    } while (sent < total);
}

/* Prints an error message and terminates the program */
void printErrorAndExit(char* screenMsg)
{
    exit(EXIT_FAILURE);
    printf("%s",screenMsg);
}

/* Converts a given integer to its string representation */
char* convertIntToStr(int x)
{
    if (x<=0)
    {
        char* res = (char*)malloc(sizeof(char)*2);
        res[0]='0';
        res[1]='\0';
        return res;
    }
    int nDigits = 0;
    int t = x;
    while(t>0)
    {
        nDigits++;
        t /= 10;
    }
    char* res = (char*)malloc(sizeof(char)*(nDigits+1));
    int index=0;
    while(x>0)
    {
        t = x%10;
        res[nDigits-1-index]='0'+t;
        index++;
        x /= 10;
    }
    res[nDigits]='\0';
    return res;
}

/* Build a HTTP POST request suitable for bitcoind */
char* buildRequest(char* body, char* host,char* username, char* password)
{
    int bodyLen = strlen(body);
    char* bodyLenStr = convertIntToStr(bodyLen);
    int bodyLenStrSize = strlen(bodyLenStr);
    char* initialLine = "POST / HTTP/1.1\r\n";
    char* preHostLine = "Host: %s\r\n";
    char* hostLine = (char*)malloc(sizeof(char)*(9+strlen(host)));
    sprintf(hostLine,preHostLine,host);
    char* connectionLine = "Connection: close\r\n";
    char* preAuthorizationLine = "Authorization: Basic %s\r\n";
    char* credentials = (char*)malloc(sizeof(char)*(strlen(username)+strlen(password)+2));
    sprintf(credentials,"%s:%s",username,password);
    char* basicAuthBase64Cred = base64Encode(credentials);
    char* authorizationLine = (char*)malloc(sizeof(char)*(24+strlen(basicAuthBase64Cred)));
    sprintf(authorizationLine,preAuthorizationLine,basicAuthBase64Cred);
    char* contentTypeLine = "Content-Type: text/plain\r\n";
    char* preContentLengthLine = "Content-Length: %s\r\n";
    char* contentLengthLine = (char*)malloc(sizeof(char)*(19+bodyLenStrSize));
    sprintf(contentLengthLine,preContentLengthLine,bodyLenStr);
    char* res = (char*)malloc(sizeof(char)*(strlen(initialLine)+strlen(hostLine)+strlen(connectionLine)+strlen(authorizationLine)+strlen(contentTypeLine)+strlen(contentLengthLine)+1));
    char* preRes = "%s%s%s%s%s%s\r\n%s";
    sprintf(res,preRes,initialLine,hostLine,connectionLine,authorizationLine,contentTypeLine,contentLengthLine,body);
    free(bodyLenStr);
    free(hostLine);
    free(credentials);
    free(basicAuthBase64Cred);
    free(authorizationLine);
    free(contentLengthLine);
    return res;
}

int getContentLength(char* input, int maxlen)
{
    int contentLength=-1;
    char identifier[] = "Content-Length: ";
    int identifierLen = strlen(identifier);
    int nMatches = 0;
    int lastMatchID = -1;
    for(int i=0;i<maxlen;i++)
    {
        if (input[i]==identifier[nMatches])
        {
            if (nMatches==0)
            {
                nMatches++;
                lastMatchID = i;
            }
            else
            {
                if (lastMatchID!=i-1)/* partial match before*/
                {
                    nMatches=0;
                    lastMatchID=-1;
                }
                else
                {
                    nMatches++;
                    if (nMatches==identifierLen) break;
                    lastMatchID = i;
                }
            }
        }
    }
    if (nMatches==identifierLen)
    {
        /* First find \r\n */
        int lastIndex=-1;
        for(int i=lastMatchID+2;i<maxlen;i++)
        {
            if ((input[i]=='\r') && (i+1<maxlen) && (input[i+1]=='\n')) lastIndex=i-1;
        }
        if (lastIndex!=-1)
        {
            /* Extract content length */
            char* lenStr = (char*)malloc(sizeof(char)*(lastIndex-lastMatchID+1));
            for(int i=0;i<lastIndex-lastMatchID;i++) lenStr[i]=input[i+lastMatchID+1];
            contentLength = atoi(lenStr);
            free(lenStr);
        }
    }
    return contentLength;
}

int main(int argc, char* argv[])
{
	/* Connection parameters and variable declarations */
	char* ip = "127.0.0.1";
    char* host = "localhost";
    char* username = "__cookie__";
    char* password = "LPZGnJ6rpaxOZj0pGO8oSL9WLIBdfhRVDL0R0trxJTI=";
    uint16_t port = 8332;
    int responseLength = 4096;
    struct sockaddr_in serverAddr;
    int socketFd, bytes, sent, received, total;
    char* response = (char*)malloc(sizeof(char)*responseLength);
    int ret;
    /* Creating a socket */
    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd==-1) printErrorAndExit("Error creating socket!\n");
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    ret = inet_pton(AF_INET, ip, &serverAddr.sin_addr);
    if (ret!=1) printErrorAndExit("Unable to convert given IP address!\n");
    /* Connect to server */
 	ret = connect(socketFd, (struct sockaddr *)&serverAddr, sizeof serverAddr);
 	if (ret==-1) printErrorAndExit("Can't connect to remote server!\n");
    printf("Successfully connected to remote server!\n");
    /* Send a request */
    char* msg = "{\"jsonrpc\":1.0,\"id\":\"req\",\"method\":\"getinfo\",\"params\":[]}";
    char* req = buildRequest(msg,host,username,password);
    printf("Sending message : %s\n",req);
    sendMessage(socketFd,req);
    /* Receive the response */
    memset(response,0,sizeof(char)*responseLength); /* Initialize response buffer */
    total = responseLength-1;
    received = 0;
    int capturedLen;
    char* newBuffer;
    do
    {
        bytes = read(socketFd,response+received,total-received);
        if (bytes < 0) printErrorAndExit("ERROR reading message from socket");
        received+=bytes;
        capturedLen = getContentLength(response,received); /* TODO wait until \r\n\r\n received */
        if ((capturedLen!=-1) && (capturedLen>total-received))
        {
            /* allocate larger array */
            newBuffer = (char*)malloc(sizeof(char)*(capturedLen+received));
            memcpy(newBuffer,response,received);
            free(response);
            response = newBuffer;
            total = capturedLen+received-1;
        }
        if (bytes == 0) break;
    } while (received < total);
    if (received == total) printErrorAndExit("ERROR storing complete response from socket");
    free(req); /* Clean up */
    /* close the socket */
    ret = close(socketFd);
	if (ret==-1) printErrorAndExit("Error closing socket!\n");
    printf("Closed socket successfully!\n");
    /* process response */
    printf("Response:\n%s\n",response);
    free(response);
    return 0;
}