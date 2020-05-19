#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SERVER_PORT 12345
#define LISTENNQ 5
#define HTTP_HEADER_LAST_CHAR_NUM 4
#define MAXTHREAD (20)
#define BLOCK_LIST_SIZE 10
#define CACHE_LIST_SIZE 512
#define URL_LENGTH 50
#define ABS_URL_LENGTH 1024
#define ABS_URL_BUFF_LENGTH 1028    // ABS_URL_LENGTH + ".txt"
#define BUFF_LENTH 16384
#define SMALL_BUFF_LEN 1024
#define PORT_LENTH 100

struct sdPair {
    int sourcefd;
    int destinfd;
};

struct response {
    struct sdPair sd;
    char absURL[ABS_URL_BUFF_LENGTH];
};

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
int getCacheList(char list[CACHE_LIST_SIZE][ABS_URL_LENGTH]) {
    FILE* fp = fopen("Cache.txt", "r");

    if (fp == NULL) {
        // File does not exist
        return 0;
    }
    int i = 0;
    while (fgets(list[i], ABS_URL_LENGTH, fp)) {
        for (int j = (strlen(list[i]) - 1); j < ABS_URL_LENGTH; ++j) {
            list[i][j] = '\0';
        }
        ++i;
    }
    fclose(fp);
    
    for (int j = i; j < CACHE_LIST_SIZE; ++j) {
        for (int k = 0; k < ABS_URL_LENGTH; ++k) {
            list[j][k] = '\0';
        }
    }
    return i;
}

// Convert ABS URL into relative URL. Store the corresponding host and ABS URL
void modify(char* host, char* recv, char* request, char* absURL) {
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
    for (k = j; k < BUFF_LENTH; ++k) {
        if (recv[k] == '\n') {
            k += 7;
            break;
        }
    }

    // Find j which the relative URL begins
    int x;
    for (x = 0, j, k; j < BUFF_LENTH;) {
        if (recv[j] == recv[k]) {
            host[x] = recv[k];
            absURL[x++] = recv[k];

            ++j;
            ++k;
        } else {
            break;
        }
    }
    
    int y;
    for (y = x; y < SMALL_BUFF_LEN; ++y) {
        host[y] = '\0';
    }

    // Replace '/' with '%'
    for (y = j; recv[y] != ' '; ++x, ++y) {
        if (recv[y] == '/') {
            absURL[x] = '%';
        } else {
            absURL[x] = recv[y];
        }
    }

    for (y = x; y < ABS_URL_LENGTH; ++y) {
        absURL[y] = '\0';
    }

    // Remove "Proxy-" in "Proxy-Connection"
    char* filter = "Proxy-C";
    char first7Char[7] = {0};
    x = j;
    while (1) {

        // Get to the end of the line
        for (; k < BUFF_LENTH; ++k) {
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
        for (y = 0; y < strlen(filter); ++y, ++x) {
            first7Char[y] = recv[x];
        }

        if (strcmp(filter, first7Char) == 0) {
            // Line starts with "Proxy-C"
            k = --x;
        } else {
            x = ++k;
        }
    }

    for (j = k + 1; j < BUFF_LENTH; ++j, ++i) {
        request[i] = recv[j];
    }
}

// Get the host and port number
void getInfo(char* recv, char* host, char* port) {
    int i = 0;
    while (recv[i++] != ' ') {}

    int j = i;
    while (recv[j++] != ':') {}

    int k = j;
    while (recv[k++] != ' ') {}

    --k;
    int x = 0;
    for (int y = j; y < k;) {
        port[x++] = recv[y++];
    }

    for (; x < PORT_LENTH; ++x) {
        port[x] = '\0';
    }

    --j;
    x = 0;
    for (int y = i; y < j;) {
        host[x++] = recv[y++];
    }

    for (; x < SMALL_BUFF_LEN; ++x) {
        host[x] = '\0';
    }

}

// Connect to the specified host and port number
int connectHost(char* host, int* serverfd, char* port) {
    struct addrinfo hints;
    struct addrinfo *result, *current;

    memset(&hints, 0, (sizeof(hints)));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, port, &hints, &result) != 0) {
        printf("Error: getaddrinfo\n");
        return -1;
    }

    for (current = result; current != NULL; current = result->ai_next) {
        *serverfd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);

        if (*serverfd == -1) {
            continue;
        }

        if (connect(*serverfd, current->ai_addr, current->ai_addrlen) != -1) {
            break;
        }
        close(*serverfd);
    }

    if (current == NULL) {
        printf("Error: No address succeeded\n");
        return -2;
    }
    freeaddrinfo(result);
    return 0;
}

// Get the content length from header in msg
long getRemainContLen(char* msg, int* found) {
    long remain = 1;

    char* str = "Content-Length: ";
    int strlength = strlen(str);
    char buff[strlength];

    int i = 0, j = 0;
    while (msg[i] != '\0') {
        if (msg[i++] != '\n') {
            continue;
        }

        j = i;
        for (int k = 0; k < strlength; ++j, ++k) {
            buff[k] = msg[j];
        }
        buff[strlength] = '\0';
        
        if (strcmp(str, buff) == 0) {
            *found = 1;

            i = j;
            while (msg[++j] != '\r') {}
            int size = j - i + 1;
            char temp[size];

            memcpy(temp, &msg[i], (size - 1));
            temp[size] = '\0';
            remain = strtol(temp, &str, 10);

            printf("Content length: %ld\n", remain);

            while ((msg[++j] != '\n') || (msg[j + 1] != '\r')) {}
            j += 3;     // First byte of data
            i = j;

            while (msg[i++] != '\0') {
                --remain;
            }
            break;
        }
    }
    return remain;
}

// Keep forwarding bytes from source fd to destination fd
void* tunnel(void* args) {
    struct sdPair * sd = args;
    int sourcefd = sd->sourcefd;
    int destinfd = sd->destinfd;

    char buffer[BUFF_LENTH] = {0};
    int bytesRead = 0;

    while (1) {
        memset(&buffer, 0, sizeof(buffer));

        bytesRead = read(sourcefd, &buffer, sizeof(buffer));

        if (bytesRead <= 0) {
            pthread_exit(NULL);
        }
        write(destinfd, buffer, bytesRead);
    }
}

void* receive(void* args) {
    struct response * res = args;
    int clientfd = res->sd.destinfd;
    int serverfd = res->sd.sourcefd;
    char absURL[ABS_URL_LENGTH] = {0};
    strcpy(absURL, res->absURL);

    char buff[ABS_URL_BUFF_LENGTH] = {0};
    snprintf(buff, sizeof(buff), "%s.txt", absURL);

    FILE* fp = fopen(buff, "w");

    if (fp == NULL) {
        printf("Failed to create file\n");
        pthread_exit(NULL);
    }

    // Read server response
    char buffer[BUFF_LENTH] = {0};
    int bytesRead = 0;
    long remainContentLen = 1;
    int found = 0;
    while (1) {
        memset(&buffer, 0, sizeof(buffer));

        bytesRead = read(serverfd, buffer, (sizeof(buffer) - 1));

        printf("%d bytes read\n", bytesRead);       // REMOVEME

        if ((bytesRead <= 0) || (remainContentLen <= 0)) {
            break;
        }

        buffer[bytesRead] = '\0';
        
        if (found == 1) {
            remainContentLen -= bytesRead;
        }

        if (found == 0) {
            remainContentLen = getRemainContLen(buffer, &found);
        }

        fprintf(fp, buffer);
        fflush(fp);

        write(clientfd, buffer, bytesRead);
        printf("Remain %ld bytes to be read\n\n", remainContentLen);        // REMOVEME
    }

    fclose(fp);

    printf("Exit\n");
    pthread_exit(NULL);
}

/**
 * Send the HTTP request to host
 * Store the host response into cache and cache list
 */
int handleHTTP(char* host, char* absURL, char* request, int* clientfd) {
    // Create a file for host response and open a file for storing cache
    FILE* cache = fopen("Cache.txt", "a");

    if (cache == NULL) {
        printf("Error: Failed to create file\n");
        return -1;
    }

    int serverfd;
    if (connectHost(host, &serverfd, "80") != 0) {
        return -2;
    }

    char buffer[BUFF_LENTH] = {0};

    snprintf(buffer, sizeof(buffer), request);
    write(serverfd, buffer, strlen(buffer));

    struct sdPair forward;
    forward.sourcefd = *clientfd;
    forward.destinfd = serverfd;
    pthread_t clientToServer;

    struct response backward;
    backward.sd.sourcefd = serverfd;
    backward.sd.destinfd = *clientfd;
    strcpy(backward.absURL, absURL);
    pthread_t serverToClient;
    
    if (pthread_create(&clientToServer, NULL, tunnel, (void*) &forward) != 0) {
        printf("Failed to create forward http thread\n");
        return -3;
    }

    if (pthread_create(&serverToClient, NULL, receive, (void*) &backward) != 0) {
        printf("Failed to create backward http thread\n");
        pthread_join(clientToServer, NULL);
        return -4;
    }

    pthread_join(clientToServer, NULL);
    pthread_join(serverToClient, NULL);

    fprintf(cache, "%s\n", absURL);      // Comment this line to disable caching
    fclose(cache);

    close(*clientfd);
    close(serverfd);
    printf("HTTP connection closed: Request and response handled\n");

    return 0;
}

/**
 * Send the stored host response back to client
 * Close the connection afterwards
 */
void sendResponse(char* absURL, int* clientfd, int blocked) {
    if (blocked == 1) {
        char* notFound = "HTTP/1.1 404 Not Found\r\n\r\n";

        char buffer[SMALL_BUFF_LEN] = {0};
        
        // Prepare send buffer
        snprintf(buffer, sizeof(buffer), notFound);
    
        // Write buffer to the connection
        write(*clientfd, buffer, strlen(buffer));
    } else {
        char buffer[BUFF_LENTH] = {0};
        char fbuff[ABS_URL_BUFF_LENGTH];
        int bytesRead = 0;

        snprintf(fbuff, sizeof(fbuff), "%s.txt", absURL);
        FILE* fp = fopen(fbuff, "r");

        if (fp == NULL) {
            printf("Error: Failed to open file\n");
            return;
        }

        while (1) {
            bytesRead = fread(buffer, 1, (sizeof(buffer) - 1), fp);
            
            if (bytesRead <= 0) {
                break;
            }
            
            buffer[bytesRead] = '\0';

            write(*clientfd, buffer, bytesRead);
        }
    }
}

// Close the fd and exit the current thread
void terminate(int* clientfd, int* serverfd) {
    if (clientfd != NULL) {
        close(*clientfd);
    }
    if (serverfd != NULL) {
        close(*serverfd);
    }
    pthread_exit(NULL);
}

void* handle(void* args) {
    int clientfd = (int) args;

    char blockList[BLOCK_LIST_SIZE][URL_LENGTH];
    char cacheList[CACHE_LIST_SIZE][ABS_URL_LENGTH];

    int numBlockedItms = getBlockList(blockList);
    int numCachedItms = getCacheList(cacheList);

    char host[SMALL_BUFF_LEN] = {0};

    char last4chars[HTTP_HEADER_LAST_CHAR_NUM + 1] = {0};
    char* terminator = "\r\n\r\n";

    // Grab the HTTP/HTTPS request
    char buff = 0;
    char recv[BUFF_LENTH] = {0};
    int i = 0;
    int n;
    while (i < sizeof(recv)) {
        n = read(clientfd, &buff, sizeof(buff));

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

    FILE* history = fopen("history.txt", "a");
    if (history == NULL) {
        printf("Error: Failed to create file\n");
        terminate(&clientfd, NULL);
    }

    // recv contains the request
    if (recv[0] == 'G') {
        printf("HTTP request received\n");

        char request[BUFF_LENTH] = {0};
        char absURL[ABS_URL_LENGTH] = {0};

        modify(host, recv, request, absURL);

        fprintf(history, "**********%s: %s**********\n%s~~~~~~~~~~Modified:~~~~~~~~~~\n%s**********%s: %s**********\n\n", host, absURL, recv, request, host, absURL);
        fclose(history);

        for (i = 0; i < numBlockedItms; ++i) {
            if (strcmp(blockList[i], host) == 0) {
                sendResponse(NULL, &clientfd, 1);
                printf("HTTP connection closed: Request is blocked\n");
                terminate(&clientfd, NULL);
            }
        }

        for (i = 0; i < numCachedItms; ++i) {
            if (strcmp(cacheList[i], absURL) == 0) {
                sendResponse(absURL, &clientfd, 0);
                printf("HTTP connection closed: Request found in cache\n");
                terminate(&clientfd, NULL);
            }
        }

        if (handleHTTP(host, absURL, request, &clientfd) != 0) {
            terminate(&clientfd, NULL);
        }
        numCachedItms = getCacheList(cacheList);

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
        printf("HTTPS request received\n");

        int serverfd;
        char buffer[BUFF_LENTH] = {0};
        char port[PORT_LENTH] = {0};

        getInfo(recv, host, port);

        fprintf(history, "**********%s:%s**********\n%s**********%s:%s**********\n\n", host, port, recv, host, port);
        fclose(history);

        for (i = 0; i < numBlockedItms; ++i) {
            if (strcmp(blockList[i], host) == 0) {
                sendResponse(NULL, &clientfd, 1);
                printf("HTTPS connection closed: Request is blocked\n");
                terminate(&clientfd, NULL);
            }
        }

        if (connectHost(host, &serverfd, port) != 0) {
            terminate(&clientfd, NULL);
        }

        struct sdPair forward;
        forward.sourcefd = clientfd;
        forward.destinfd = serverfd;
        pthread_t clientToServer;

        struct sdPair backward;
        backward.sourcefd = serverfd;
        backward.destinfd = clientfd;
        pthread_t serverToClient;

        char* msg = "HTTP/1.1 200 Connection Established\r\n\r\n";
        snprintf(buffer, sizeof(buffer), msg);
        write(clientfd, buffer, strlen(buffer));    // Send confirmation back to client

        if (pthread_create(&clientToServer, NULL, tunnel, (void*) &forward) != 0) {
            printf("Failed to create forward tunneling thread\n");
            terminate(&clientfd, &serverfd);
        }

        if (pthread_create(&serverToClient, NULL, tunnel, (void*) &backward) != 0) {
            printf("Failed to create backward tunneling thread\n");
            pthread_join(clientToServer, NULL);
            terminate(&clientfd, &serverfd);
        }

        pthread_join(clientToServer, NULL);
        pthread_join(serverToClient, NULL);

        close(clientfd);
        close(serverfd);

        printf("HTTPS connection closed\n");
    }
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    int listenfd, clientfd;

    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(struct sockaddr_in);

    int threads_count = 0;
    pthread_t threads[MAXTHREAD];

    /* initialize server socket */
    listenfd = socket(AF_INET, SOCK_STREAM, 0); /* SOCK_STREAM : TCP */
    if (listenfd < 0) {
        printf("Error: init socket\n");
        return 0;
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
    }

    if (listen(listenfd, LISTENNQ) < 0) {
        printf("Error: listen\n");
        return 0;
    } else {
        printf("Proxy started\n");
    }

    /* keep processing incoming requests */
    while (1) {
        /* accept an incoming connection from the remote side */
        clientfd = accept(listenfd, (struct sockaddr*)&cliaddr, &len);
        if (clientfd < 0) {
            printf("Error: accept\n");
            return 0;
        }

        if (threads_count >= MAXTHREAD) {
            for (int i = 0; i < MAXTHREAD; ++i) {
                pthread_join(threads[i], NULL);
            }
        }

        if (pthread_create(&threads[threads_count++], NULL, handle, (void*) clientfd) != 0) {
            printf("Error when creating thread %d\n", threads_count);
            return 0;
        }
    }

    return 0;
}
