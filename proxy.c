#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAXLINE 1000
#define SERVER_PORT 12345
#define LISTENNQ 5
#define HTTP_HEADER_LAST_CHAR_NUM 4

int main(int argc, char** argv) {
    int listenfd, connfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(struct sockaddr_in);
    char recv[MAXLINE] = {0};
    char ip_str[INET_ADDRSTRLEN] = {0};
    char buff = 0;
    char last4chars[HTTP_HEADER_LAST_CHAR_NUM + 1] = {0};
    char* terminator = "\r\n\r\n";
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

        /* print http/https request */
        i = 0;
        while (i < sizeof(recv)) {
            n = read(connfd, &buff, sizeof(buff));

            if (n <= 0) {
                break;
            }

            recv[i++] = buff;

            /* retrive last 4 chars */
            for (j = 0; j < HTTP_HEADER_LAST_CHAR_NUM; ++j) {
                last4chars[j] = recv[i - HTTP_HEADER_LAST_CHAR_NUM + j];
            }

            /* HTTP header end with "\r\n\r\n" */
            if (strcmp(last4chars, terminator) == 0) {
                break;
            }
        }

        recv[i] = '\0';
        printf("%s", recv);

        /* close the connection */
        close(connfd);
    }

    return 0;
}
