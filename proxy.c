#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAXLINE 1000
#define SERVER_PORT 12345
#define LISTENNQ 5
#define HTTP_HEADER_LAST_CHAR_NUM 4
#define MAXTHREAD (5)

// Handle the client request
void* process(void* args) {
    char buff = 0;
    char recv[MAXLINE] = {0};
    char request[MAXLINE] = {0};

    char last4chars[HTTP_HEADER_LAST_CHAR_NUM + 1] = {0};
    char* terminator = "\r\n\r\n";

    // Grab the HTTP/HTTPS request
    int connfd = (int)args;

    int i = 0;
    while (i < sizeof(recv)) {
        int n = read(connfd, &buff, sizeof(buff));

        if (n <= 0) {
            break;
        }

        recv[i++] = buff;

        /* retrive last 4 chars */
        for (int j = 0; j < HTTP_HEADER_LAST_CHAR_NUM; ++j) {
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
         * 
         * GET http://sample.com/ HTTP/1.1\r\n
         * Host: sample.com\r\n
         * Connection: keep-alive\r\n
         */

        /**
         * i always point at the location of the next char to copy in request[]
         * j is always used to point at the char to be copied in recv[]
         */
        i = 0;
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
        for (j, k; j < MAXLINE;) {
            if (recv[j] == recv[k]) {
                ++j;
                ++k;
            } else {
                break;
            }
        }

        for (j; j < MAXLINE; ++j) {
            request[i] = recv[j];
        }

        // request[] now contains the modified proxy message

    } else {
        /**
         * HTTPS request
         * 
         * CONNECT example.copm:443 HTTP/1.1\r\n
         * Host: sample.com\r\n
         * Connection: keep-alive\r\n
         * 
         * Check P.10 for info
         */
    }

    /* close the connection */
    close(connfd);

    // TODO Terminate this thread
}

int main(int argc, char** argv) {
    int listenfd, connfd;

    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(struct sockaddr_in);

    char ip_str[INET_ADDRSTRLEN] = {0};

    int threads_count = 0;
    pthread_t threads[MAXTHREAD];

    int n, i, j;

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
    }

    /* keep processing incoming requests */
    while (1) {
        /* accept an incoming connection from the remote side */
        connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &len);
        if (connfd < 0) {
            printf("Error: accept\n");
            return 0;
        }

        if (pthread_create(&threads[threads_count], NULL, process, (void*)connfd) != 0) {
            printf("Error when creating thread %d\n", threads_count);
            return 0;
        }

        // TODO: Change to thread pool?
        if (++threads_count >= MAXTHREAD) {
            break;
        }
    }

    for (int i = 0; i < MAXTHREAD; ++i) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
