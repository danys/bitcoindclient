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
    printf("%s",screenMsg);
    exit(EXIT_FAILURE);
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

/* Find the index of the heade-body separator \r\n\r\n */
int getHeaderBodySeparatorIndex(char* input, int maxlen)
{
    int res = -1;
    for(int i=0;i<maxlen;i++)
    {
        if ((input[i]=='\r') && (i+1<maxlen) && (input[i+1]=='\n') && (i+2<maxlen) && (input[i+2]=='\r') && (i+3<maxlen) && (input[i+3]=='\n'))
            {
                res=i;
                break;
            }
    }
    return res;
}

/* Tries to extract the Content-Length size from a given input. Returns -1 if not found or if header-body separation line is not found */
int getContentLength(char* input, int maxlen)
{
    int contentLength=-1;
    int foundEmptyLine = getHeaderBodySeparatorIndex(input,maxlen);
    if (foundEmptyLine==-1) return -1;
    char identifier[] = "Content-Length: ";
    int identifierLen = strlen(identifier);
    int nMatches = 0;
    int lastMatchID = -1;
    for(int i=0;i<maxlen;i++)
    {
        if (input[i]==identifier[nMatches])
        {
            nMatches++;
            lastMatchID = i;
        }
        else
        {
            if (nMatches>0)
            {
                nMatches=0;
                lastMatchID=-1;
                if (input[i]==identifier[nMatches])
                {
                    nMatches++;
                    lastMatchID = i;
                }
            }
        }
        if (nMatches==identifierLen) break;
    }
    if (nMatches==identifierLen)
    {
        /* First find \r\n */
        int lastIndex=-1;
        for(int i=lastMatchID+2;i<maxlen;i++)
        {
            if ((input[i]=='\r') && (i+1<maxlen) && (input[i+1]=='\n'))
            {
                lastIndex=i-1;
                break;
            }
        }
        if (lastIndex!=-1)
        {
            /* Extract content length */
            char* lenStr = (char*)malloc(sizeof(char)*(lastIndex-lastMatchID+2));
            for(int i=0;i<lastIndex-lastMatchID+1;i++) lenStr[i]=input[i+lastMatchID+1];
            lenStr[lastIndex-lastMatchID+1]='\0';
            contentLength = atoi(lenStr);
            free(lenStr);
        }
    }
    return contentLength;
}

/* Extracts the body of the response (null-terminated response) */
char* receiveResponse(int socketFd)
{
    int bytes, sent, received, total;
    int responseLength = 4096;
    char* responseBody;
    char* response = (char*)malloc(sizeof(char)*responseLength);
    memset(response,0,sizeof(char)*responseLength); /* Initialize response buffer */
    total = responseLength-1;
    received = 0;
    int capturedLen;
    char* newBuffer;
    int foundContentSize = 0;
    do
    {
        bytes = read(socketFd,response+received,total-received);
        if (bytes < 0)
        {
            printErrorAndExit("ERROR reading message from socket");
            return NULL;
        }
        received+=bytes;
        if (foundContentSize==0) /* Don't perform this action if "Content-Length" has been found already */
        {
            capturedLen = getContentLength(response,received);
            if (capturedLen!=-1)
            {
                if (capturedLen>total-received)
                {
                    /* allocate larger array */
                    newBuffer = (char*)malloc(sizeof(char)*(capturedLen+received));
                    memcpy(newBuffer,response,received);
                    free(response);
                    response = newBuffer;
                    total = capturedLen+received-1;
                }
                foundContentSize=1;
            }
        }
        if (bytes == 0) break;
    } while (received < total);
    responseBody=NULL;
    if (received == total)
    {
            printErrorAndExit("ERROR storing complete response from socket");
            return NULL;
    }
    int index=getHeaderBodySeparatorIndex(response, total);
    if ((index==-1) || (foundContentSize==0)) return NULL;
    if (response[received-1]=='\n') capturedLen--;
    responseBody = (char*)malloc(sizeof(char)*(capturedLen+1));
    memcpy(responseBody,response+index+4,capturedLen);
    responseBody[capturedLen]='\0';
    free(response);
    return responseBody;
}

/* Build a JSON message to query a block hash */
char* constructGetBlockHashJSONMsg(int blockHeight)
{
    char* blockHeightStr = convertIntToStr(blockHeight);
    int len = strlen(blockHeightStr);
    char* preMsg = "{\"jsonrpc\":1.0,\"id\":\"req\",\"method\":\"getblockhash\",\"params\":[%s]}";
    char* msg = (char*)malloc((62+len+1)*sizeof(char));
    sprintf(msg,preMsg,blockHeightStr);
    return msg;
}

/* Build a JSON message to query a raw block header */
char* constructGetBlockJSONMsg(char* blockHash)
{
    int len = strlen(blockHash);
    char t[] = "false";
    char* preMsg = "{\"jsonrpc\":1.0,\"id\":\"req\",\"method\":\"getblock\",\"params\":[\"%s\",%s]}";
    char* msg = (char*)malloc((61+len+1+5)*sizeof(char));
    sprintf(msg,preMsg,blockHash,t);
    return msg;
}

/* Build a JSON message to query a raw transaction */
char* constructGetRawTransactionJSONMsg(char* txID)
{
    int len = strlen(txID);
    char* preMsg = "{\"jsonrpc\":1.0,\"id\":\"req\",\"method\":\"getrawtransaction\",\"params\":[\"%s\",%i]}";
    char* msg = (char*)malloc((70+len+1+1)*sizeof(char));
    sprintf(msg,preMsg,txID,0);
    return msg;
}

/* Extract the resulting string valine in the JSON response */
char* extractResultStringFromJSON(char* response)
{
    /* Get the response length */
    int len=0;
    while(response[len]!='\0') len++;
    /* First check the error message */
    int errorOK=0;
    for(int i=0;i<len-11;i++)
    {
        if ((response[i]=='"') && (response[i+1]=='e') && (response[i+2]=='r') && (response[i+3]=='r') && 
            (response[i+4]=='o') && (response[i+5]=='r') && (response[i+6]=='"') && (response[i+7]==':') &&
            (response[i+8]=='n') && (response[i+9]=='u') && (response[i+10]=='l') && (response[i+11]=='l'))
        {
            errorOK=1;
            break;
        }
    }
    if (errorOK==0) return NULL; /* the error field was not null => failure */
    /* Next get the result string */
    int resultIndex=-1;
    for(int i=0;i<len-8;i++)
    {
        if ((response[i]=='"') && (response[i+1]=='r') && (response[i+2]=='e') && (response[i+3]=='s') && 
            (response[i+4]=='u') && (response[i+5]=='l') && (response[i+6]=='t') && (response[i+7]=='"') &&
            (response[i+8]==':'))
        {
            resultIndex=i;
            break;
        }
    }
    if (resultIndex==-1) return NULL;
    resultIndex += 9; /* resultIndex points to beginning of result string => points to '"' char */
    if ((resultIndex>len-1) || response[resultIndex]!='"') return NULL;
    int resultStopIndex=-1;
    for(int i=resultIndex+1;i<len;i++)
    {
        if (response[i]=='"')
        {
            resultStopIndex=i;
            break;
        }
    }
    if (resultStopIndex==-1) return NULL; /* error finding end of string value */
    resultIndex++;
    resultStopIndex--;
    char* resultVar = (char*)malloc(sizeof(char)*(resultStopIndex-resultIndex+2));
    memcpy(resultVar,response+resultIndex,resultStopIndex-resultIndex+1);
    resultVar[resultStopIndex-resultIndex+1]='\0';
    free(response);
    return resultVar;
}

int main(int argc, char* argv[])
{
	/* Connection parameters and variable declarations */
	char* ip = "127.0.0.1";
    char* host = "localhost";
    char* username = "__cookie__";
    char* password = "mY3JZM2Vd1F4cFApPiM63cxSwSQC3umZrwva/4b+u84=";
    uint16_t port = 8332;
    struct sockaddr_in serverAddr;
    int socketFd;
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
    char* msg = constructGetBlockHashJSONMsg(0);
    char* req = buildRequest(msg,host,username,password);
    printf("Sending message : \n%s\n",req);
    sendMessage(socketFd,req);
    /* Receive the response's body */
    char* bodyStr = receiveResponse(socketFd);
    free(req); /* Clean up */
    /* close the socket */
    ret = close(socketFd);
	if (ret==-1) printErrorAndExit("Error closing socket!\n");
    printf("Closed socket successfully!\n");
    /* process response */
    char* responseValue = extractResultStringFromJSON(bodyStr);
    printf("Response: \n%s\n",responseValue);
    free(responseValue);
    return 0;
}