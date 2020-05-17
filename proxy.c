#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXLINE 1024
#define SERVER_PORT 12345
#define LISTENNQ 5
#define HTTP_HEADER_LAST_CHAR_NUM 4
#define MAXTHREAD (5)
#define BLOCK_LIST_SIZE 10
#define CACHE_LIST_SIZE 10
#define URL_LENGTH 50

// The file should be provided from Windows, each line is terminated with \r\n
int getBlockList(char list[BLOCK_LIST_SIZE][URL_LENGTH]) {
    FILE* fp = fopen("Block List.txt", "r");

    int i = 0;
    while (fgets(list[i], URL_LENGTH, fp)) {
        for (int j = (strlen(list[i]) - 2); j < URL_LENGTH; ++j) {
            list[i][j] = '\0';
        }
        ++i;
    }
    fclose(fp);

    for (int j = i; j < BLOCK_LIST_SIZE; ++j) {
        for (int k = 0; k < URL_LENGTH; ++k) {
            list[j][k] = '\0';
        }
    }
    return --i;
}

// The file should be within linux system, each line is terminated with \n
int getCacheList(char list[CACHE_LIST_SIZE][URL_LENGTH]) {
    FILE* fp = fopen("Cache.txt", "r");

    if (fp == NULL) {
        // File does not exist
        return 0;
    }
    int i = 0;
    while (fgets(list[i], URL_LENGTH, fp)) {
        for (int j = (strlen(list[i]) - 1); j < URL_LENGTH; ++j) {
            list[i][j] = '\0';
        }
        ++i;
    }
    fclose(fp);
    
    for (int j = i; j < CACHE_LIST_SIZE; ++j) {
        for (int k = 0; k < URL_LENGTH; ++k) {
            list[j][k] = '\0';
        }
    }
    return --i;
}

void modify(char* host, char* recv, char* request) {
    /**
     * i always point at the location of the next char to copy in request[]
     * j is always used to point at the char to be copied in recv[]
     */
    int i = 0;
    int j, k;

    // Copy "Get "
    for (j = 0; j < 4; ++j, ++i) {
        request[i] = recv[j];
    }
    // Index of the first char right after "http://"
    j = 11;

    // k == index of the first char right after "Host: "
    for (k = j; k < MAXLINE; ++k) {
        if (recv[k] == '\n') {
            k += 7;
            break;
        }
    }

    // Find j which the relative URL begins
    int x;
    for (x = 0, j, k; j < MAXLINE;) {
        if (recv[j] == recv[k]) {
            host[x++] = recv[k];

            ++j;
            ++k;
        } else {
            break;
        }
    }
    host[x] = '\0';

    // Remove "Proxy-" in "Proxy-Connection"
    char* filter = "Proxy-C";
    char first7Char[7] = {0};
    x = j;
    while (1) {

        // Get to the end of the line
        for (k; k < MAXLINE; ++k) {
            if (recv[k] == '\n') {
                break;
            }
        }

        // Copy until pass k
        for (j = x; j <= k; ++j, ++i) {
            request[i] = recv[j];
        }

        // Check if end of header
        if (recv[k + 1] == '\r') {
            break;
        }

        // Check if "Proxy-C"
        x = k + 1;
        for (int y = 0; y < strlen(filter); ++y, ++x) {
            first7Char[y] = recv[x];
        }

        if (strcmp(filter, first7Char) == 0) {
            // Line starts with "Proxy-C"
            k = --x;
        } else {
            x = ++k;
        }
    }

    for (j = k + 1; j < MAXLINE; ++j, ++i) {
        request[i] = recv[j];
    }
    request[k + 3] = '\0';
}

void sentRequest(char* host, char* request, char* response) {
    int sockfd, status;
    char writeline[MAXLINE] = {0};
    
    struct addrinfo hints;
    struct addrinfo *result, *current;

    memset(&hints, 0, (sizeof(hints)));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;    // ?

    status = getaddrinfo(host, "80", &hints, &result);
    if (status != 0) {
        printf("Error: getaddrinfo\n");
        return;
    }

    for (current = result; current != NULL; current = result->ai_next) {
        sockfd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);

        if (sockfd == -1) {
            continue;
        }

        if (connect(sockfd, current->ai_addr, current->ai_addrlen) != -1) {
            printf("************\nHost socket successfully connected\n");     // REMOVEME
            break;
        }
        close(sockfd);
    }

    if (current == NULL) {
        printf("Error: Could not connect\n");
        return;
    }

    freeaddrinfo(result);

    // Construct the request
    snprintf(writeline, sizeof(writeline) - 1, request);
    // Send the request
    write(sockfd, writeline, strlen(writeline));

    printf("Request sent to host\n");   // REMOVEME

    // Read the response
    int i;
    while (1) {
        i = read(sockfd, response, (sizeof(response) - 1));

        if (i <= 0) {
            break;
        } else {
            response[i] = '\0';
        }
    }
    
    printf("Response read from host\n");   // REMOVEME
}

/**
 * Send the stored host response back to client
 * Close the connection afterwards
 */
void sendResponse(char* host, int* connfd, int blocked) {
    char buffer[MAXLINE] = {0};

    if (blocked == 1) {
        char notFound[] = "HTTP/1.1 404 Not Found\r\n\r\n";
        
        // Prepare send buffer
        snprintf(buffer, (sizeof(buffer) - 1), notFound, *connfd);
    
        // Write buffer to the connection
        write(*connfd, buffer, strlen(buffer));
    } else {
        char fbuff[URL_LENGTH];
        size_t bytesRead = 0;

        snprintf(fbuff, sizeof(fbuff), "%s.html", host);
        FILE* fp = fopen(fbuff, "r");

        if (fp == NULL) {
            printf("Error: Failed to open file\n");
        }

        memset(&buffer, 0, sizeof(buffer));

        while (1) {
            bytesRead = fread(buffer, 1, (sizeof(buffer) - 1), fp);
            
            if (bytesRead <= 0) {
                break;
            }
            
            buffer[bytesRead] = '\0';

            write(*connfd, buffer, bytesRead);
        }
    }

    printf("Response sent back to client\n");  // REMOVEME

    close(*connfd);
    printf("Client connection closed\n");  // REMOVEME
}

int main(int argc, char** argv) {
    int listenfd, connfd;

    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(struct sockaddr_in);

    int threads_count = 0;
    pthread_t threads[MAXTHREAD];

    char blockList[BLOCK_LIST_SIZE][URL_LENGTH];
    char cacheList[CACHE_LIST_SIZE][URL_LENGTH];

    int numBlockedItms = getBlockList(blockList);
    int numCachedItms = getCacheList(cacheList);

    /* initialize server socket */
    listenfd = socket(AF_INET, SOCK_STREAM, 0); /* SOCK_STREAM : TCP */
    if (listenfd < 0) {
        printf("Error: init socket\n");
        return 0;
    } else {
        printf("Proxy server incoming socket initialized\n");    // REMOVEME
    }

    /* initialize server address (IP:port) */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;  /* IP address: 0.0.0.0 */
    servaddr.sin_port = htons(SERVER_PORT); /* port number */

    /* bind the socket to the server address */
    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr)) < 0) {
        printf("Error: bind\n");
        return 0;
    } else {
        printf("Proxy server socket bound\n");    // REMOVEME
    }

    if (listen(listenfd, LISTENNQ) < 0) {
        printf("Error: listen\n");
        return 0;
    } else {
        printf("Proxy server is now listening\n");    // REMOVEME
    }

    /* keep processing incoming requests */
    while (1) {
        /* accept an incoming connection from the remote side */
        connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &len);
        if (connfd < 0) {
            printf("Error: accept\n");
            return 0;
        } else {
            printf("Client connection accepted\n");   // REMOVEME
        }

        char buff = 0;
        char recv[MAXLINE] = {0};
        char request[MAXLINE] = {0};

        char host[MAXLINE] = {0};

        int cont = 0;

        char last4chars[HTTP_HEADER_LAST_CHAR_NUM + 1] = {0};
        char* terminator = "\r\n\r\n";

        // Grab the HTTP/HTTPS request
        int i = 0;
        int n;
        while (i < sizeof(recv)) {
            n = read(connfd, &buff, sizeof(buff));

            if (n <= 0) {
                break;
            }

            recv[i++] = buff;

            int j;
            for (j = 0; j < HTTP_HEADER_LAST_CHAR_NUM; ++j) {
                last4chars[j] = recv[i - HTTP_HEADER_LAST_CHAR_NUM + j];
            }

            /* HTTP header end with "\r\n\r\n" */
            if (strcmp(last4chars, terminator) == 0) {
                break;
            }
        }
        recv[i] = '\0';

        // recv contains the request
        if (recv[0] == 'G') {
            /**
             * HTTP request
             */
            
            printf("Client HTTP request received\n");  // REMOVEME

            modify(host, recv, request);

            FILE* history = fopen("history.txt", "a");
            if (history == NULL) {
                printf("Error: Failed to create file\n");
                return 0;
            }
            fprintf(history, "**********%s**********\n%s~~~~~~~~~~\n%s********************\n", host, recv, request);
            fclose(history);

            for (i = 0; i < numBlockedItms; ++i) {
                if (strcmp(blockList[i], host) == 0) {
                    // Send 404 back to client
                    printf("Requested site is blocked\n");

                    sendResponse(host, &connfd, 1);
                    cont = 1;
                    break;
                }
            }
            // printf("%s\n*****\n%s\n*****\n%s", host, recv, request);
            for (i = 0; i < numCachedItms; ++i) {
                if (strcmp(cacheList[i], host) == 0) {
                    printf("Requested site is found in cache\n");

                    sendResponse(host, &connfd, 0);
                    cont = 1;
                    break;
                }
            }

            if (cont == 1) {
                continue;
            }

            if (sentRequest(host, request) != 0) {
                // Encountered error while sending request
                continue;
            }
            
            numCachedItms = getCacheList(cacheList);

            sendResponse(host, &connfd, 0);

        } else if (recv[0] == 'C') {
            /**
             * HTTPS request
             * 
             * CONNECT example.copm:443 HTTP/1.1\r\n
             * Host: sample.com\r\n
             * Connection: keep-alive\r\n
             * 
             * Check P.10 for info
             */
            // printf("Client HTTPS request received\n");  // REMOVEME

            close(connfd);
        }
    }

    return 0;
}
