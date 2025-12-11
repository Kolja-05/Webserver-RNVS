#include <asm-generic/socket.h>
#include <bits/types/stack_t.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include "http_parser.h"

#define MAX_BODY 4096
#define MAX_NAME 256

typedef struct dynamic_content_t {
    bool used;
    char name[MAX_NAME];
    char content[MAX_BODY];
    size_t len;
} dynamic_content_t;

// finds the content in a dynamic_content_t table
// input: name of the content we are looking for,   pointer to content table,  length of the table
// returns: index of content in the content table on succsess, -1 if not found
int find_content (const char *name, dynamic_content_t *content_table, size_t len) {
    for (size_t i=0; i < len; i++) {
        if (content_table[i].used && strcmp(content_table[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}
//finds first free slot in a dynamic_content_t table
// input: pointer to the content table, lenght of the table
// returns: index of first free slot on success, -1 if no free slot found
int find_free_slot (dynamic_content_t * content_table, size_t len) {
    for (size_t i=0; i<len; i++) {
        if (!content_table[i].used) {
            return i;
        }
    }
    return -1;
}
// handles PUT requests
// inputs: client_socket, request, dynamic contetn of our webserver, the len of the content table, the buffer of the packet stream, the len of the buffer as a pointer
// returns: a string literal with the responscode e.g. "201 Created"
const char* handle_put(int client_fd, http_request_t *request, dynamic_content_t *dyn_cont, size_t dyn_cont_len, char *buf, int *buf_len_ptr) {
    if (strncmp(request->uri, "/dynamic/", 9) != 0) {
        return "403 Forbidden";
    }

    int buf_len = *buf_len_ptr;
    char *pos = strstr(buf, "\r\n\r\n");
    if (!pos) return "400 Bad Request"; // inclomplete header
    char *body_start = pos + 4;
    int body_in_buf = buf_len -(body_start - buf);

    char *filename = request->uri + 9;
    if (strlen(filename) == 0) {
        return  "400 Bad request";
    }

    const char* content_len_string = find_header_value_by_key(request->headers, "Content-Length");
    if (content_len_string == NULL) return  "400 Bad Request";
    int content_len = atoi(content_len_string);
    if (content_len <= 0) return "400 Bad Request";
    if (content_len > MAX_BODY) return "413 Request Entity Too Large";
    int idx = find_content(filename, dyn_cont, dyn_cont_len);
    bool created = false;
    if (idx == -1) {
        idx = find_free_slot(dyn_cont, dyn_cont_len);
        if (idx == -1) return "507 Insufficient Storage";
            created = true;
    }
    // copy body from buffer
    int bytes_received = body_in_buf;
    if (bytes_received > content_len) {
        bytes_received = content_len;
    }
    memcpy(dyn_cont[idx].content, body_start, bytes_received); // copy body from buffer
    while (bytes_received < content_len) { // copy missing bytes from stream
        int n = recv(client_fd, dyn_cont[idx].content + bytes_received, content_len - bytes_received, 0);
        if (n <= 0) {
            return "400 Bad Request";
        }
        bytes_received += n;
    }
    if (bytes_received < MAX_BODY) {
        dyn_cont[idx].content[bytes_received] = '\0';
    } else {
        dyn_cont[idx].content[MAX_BODY-1] = '\0';
    }
    dyn_cont[idx].len = bytes_received;
    strncpy(dyn_cont[idx].name, filename, MAX_NAME);
    dyn_cont[idx].used = true;

    // move buffer for next request
    int remaining = buf_len - (body_start - buf + bytes_received);
    if (remaining > 0) {
        memmove(buf, body_start + bytes_received, remaining);
    }
    *buf_len_ptr = remaining;
    if (created) {
        return "201 Created";
    }
    return "204 No Content";
}


const char* handle_delete(http_request_t *request, dynamic_content_t *dyn_cont, size_t dyn_cont_len) {
    if (strncmp(request->uri, "/dynamic/", 9) != 0) {
        return "403 Forbidden";
    }
    char *filename = request->uri + 9;
    if (strlen(filename) == 0) {
        return  "400 Bad request";
    }
    int idx = find_content(filename, dyn_cont, dyn_cont_len);
    if (idx == -1) {
        return "404 Not Found";
    }
    dyn_cont[idx].used = false;
    dyn_cont[idx].name[0] = '\0';
    dyn_cont[idx].content[0] = '\0';
    dyn_cont[idx].len = 0;
    return "204 No Content";
}

int main(int argc, char** argv) {
    if(argc != 3) { // check for right number of arguments
        perror("Error: invalid arguments, correct usage: ./webserver <host> <port>");
        return EXIT_FAILURE;
    }
    const char* host = argv[1];
    const char* port = argv[2];
    const int BACKLOG = 100; // how many pendign connections queue will hold

    //----------GET ADRESS INFORMATION ---------
    int status;
    struct addrinfo hints; // tells getaddrinfo what kind of address we are looking for (mor on Beej's guide 5.1)
    struct addrinfo *serverinfo; // will point to the resuts (linked list as discribed in 3.3 on Beej's guide)
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv4, for specific IPv4 use AF_INET, for IPv6 use AF_INET6
    hints.ai_socktype = SOCK_STREAM; //what kind of sockets we want
    hints.ai_flags = AI_PASSIVE; // assigns the address of local host to the socket structure

    if ((status = getaddrinfo(host, port, &hints, &serverinfo)) != 0) {
        fprintf (stderr, "getadder info failed with exitcode %s \n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }
    // serverinfo now points to a linked list of 1 or more struct addrinfo. We use the result to pass to other socket functions



    // TODO ..
    // loop for all serverinfos in the linked list and bind to the first we can
    struct addrinfo *p; // loop variable
    int yes = 1;
    int sock_fd; // socketdiscribtor that can be used in later systemcalls
    for (p = serverinfo; p != NULL; p = p->ai_next) {
        //----------------SOCKET----------------

        if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            fprintf(stderr, "socket failed with errno %d\n", errno);
            freeaddrinfo(serverinfo); //free before exiting
            continue;
        };

        // to avoid the error "Adress already in use"
        //
        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            fprintf(stderr, "setsockopt failed\n");
            close(sock_fd);
            exit(EXIT_FAILURE);
        } //TODO Errorhandling

        //---------------BIND-------------------
        //associate the socket with a port on our local machine

        if (bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
            fprintf(stderr, "bind failed with errno %d\n", errno);
            freeaddrinfo(p);
            continue;
        };
        break;
    }
    freeaddrinfo(serverinfo); //free the linked list
    

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
    }

    // -----------------LISTEN--------------
    //wait for someone to connect to us
    if (listen(sock_fd, BACKLOG) == -1) {
        fprintf(stderr, "error while listening\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }



    struct sockaddr_storage client_addr;
    int client_fd;
    const int buf_size = 8192;
    char buf[buf_size];
    int buf_len = 0;
    char *pos;

    const int MAX_DYN_CONTENT = 256;
    dynamic_content_t dyn_cont[MAX_DYN_CONTENT];
    memset(dyn_cont, 0, sizeof(dyn_cont)); // set everything in dyn_cont to 0, used = 0 (false), len = 0, name = 0 ('\0'), content = 0 ('\0')

    while(1) {
        socklen_t client_addr_len = sizeof(client_addr);
        //accept
        if((client_fd = accept(sock_fd, (struct sockaddr *) &client_addr, &client_addr_len)) == -1) {
            fprintf(stderr, "failed to accept connection, errno: %d\n", errno);
            continue;
        }


        while(1) {
            if (buf_len >= buf_size) {
                fprintf(stderr, "buffer overflow, closing client\n");
                break;
            }
            int bytes_recieved;
            if ((bytes_recieved = recv (client_fd, buf + buf_len, buf_size - buf_len, 0)) == -1) {
                fprintf(stderr, "error while receiving packets %s\n", strerror(errno));
                break;
            }
            if (bytes_recieved == 0) {
                break; // close connection
            }
            buf_len += bytes_recieved;
            buf[buf_len] = '\0';
            http_request_t request;;
            while ((pos = strstr(buf, "\r\n\r\n")) != NULL) {
                int request_len = pos - buf + 4;
                // TODO parse request
                request = parse_http_request(buf);

                // create reply
                char msg[1024];
                const char *body = ""; // foo, bar or baz, or empty if not found
                const char* responsecode; //400 Bad Request, 404 Not Found, 200 Ok, ...
                int content_len = 0;
                if (!request.valid) {
                    responsecode = "400 Bad Request";
                }
                else if (request.method == GET) {
                    //TODO check if content exists
                    if (strncmp(request.uri, "/", 1) != 0) {
                        responsecode = "400 Bad Request"; // URI must start with '/'
                    }
                    else if (strncmp(request.uri, "/static/", 8) == 0) {
                        char *filename = request.uri + 8; // skip /static/
                        if (strcmp(filename, "foo") == 0) {
                            body = "Foo";
                            content_len = strlen(body);
                            responsecode = "200 OK";
                        }
                        else if (strcmp(filename, "bar") == 0) {
                            body = "Bar";
                            content_len = strlen(body);
                            responsecode = "200 OK";
                        }
                        else if (strcmp(filename, "baz") == 0) {
                            body = "Baz";
                            content_len = strlen(body);
                            responsecode = "200 OK";
                        }
                        else {
                            responsecode = "404 Not Found";
                        }
                    }
                    else if (strncmp(request.uri, "/dynamic/", 9) == 0) {
                        // return dynamic conntent
                        char *filename = request.uri + 9; // skip dynamic
                        int idx = find_content(filename, dyn_cont, MAX_DYN_CONTENT); // TODO maybe MAX_DYN_CONTENT instead of sizeof...
                        if (idx != -1) {
                            body = dyn_cont[idx].content;
                            content_len = strlen(body);
                            responsecode = "200 Ok";
                        }
                        else {
                            responsecode = "404 Not Found";
                        }
                    }
                    else {
                        // invalid uri
                        responsecode = "404 Not Found";
                    }
                }
                else if (request.method == PUT) {
                    responsecode = handle_put(client_fd, &request, dyn_cont, MAX_DYN_CONTENT, buf, &buf_len);
                }
                else if (request.method == DELETE) {
                    responsecode = handle_delete(&request, dyn_cont, MAX_DYN_CONTENT);
                }
                else if (request.method == other) {
                    responsecode = "501 Not Implemented";
                }
                snprintf(msg, sizeof(msg),
                             "HTTP/1.1 %s\r\n"
                              "Content-Length: %d\r\n"
                              "\r\n",
                              responsecode, content_len);

                int len, bytes_sent;
                len = strlen(msg);
                if ((bytes_sent = send(client_fd, msg, len, 0)) == -1) {
                    fprintf(stderr, "send failed %s\n", strerror(errno));
                    break;
                }
                if (content_len > 0 && body != NULL) {
                    if ((bytes_sent = send(client_fd, body, content_len, 0)) == -1) {
                        fprintf(stderr, "send failed %s\n", strerror(errno));
                    }
                }
                int remaining = buf_len - request_len;
                if (remaining < 0) {
                    remaining = 0;
                }

                memmove(buf, buf + request_len, remaining);
                buf_len = remaining;
                if (buf_len < buf_size) {
                    buf[buf_len] = '\0';
                }
            }
        }
        // close the connection to the client
        close(client_fd);
    }
    
    // TODO send
    // TODO sendto and recv
    // TODO close and shutdown
    // TODO getpeername
    // TODO gethostname
}
