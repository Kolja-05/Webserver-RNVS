
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <stdbool.h>

typedef enum http_method_t {
    GET,    // 0
    PUT,    // 1
    DELETE, // 2
    other,  // 3
}http_method_t;

typedef enum http_version_t {
    v1_0,           // 0
    v1_1,           // 1
    not_specified,  // 2
}http_version_t;

typedef struct header_list_t{
    char key[16];
    char value[256];
    struct header_list_t *next;
}header_list_t;

typedef struct {
    http_method_t method; // GET, PUT, DELETE, ...
    char uri[256]; //paht to resource
    http_version_t version; // 1.0 or 1.1
    bool valid; // True if ok Flase if not 
    header_list_t* headers;
} http_request_t;

/* --------- FUNCTION TO PARSE A HTPPREQEUST--------
*<HTTP method>  <URI> <HTTP version>
* example GET /index.html HTTP/1.1
*/
http_request_t parse_http_request (const char* packet) {
    http_request_t request; // this is the request we want to return in the end
    const char *end_of_line = strstr(packet, "\r\n"); // TODO we already did that in main, could give the end_of_packet as an argument...
    if(!end_of_line) request.valid = false; // if no end of packet was found, return invalid request

    // -------Parse the start line--------
    char start_line[512];
    size_t start_line_len = end_of_line - packet;
    if (start_line_len <= 0 || start_line_len >= sizeof(start_line)) {
        request.valid = false; // no start line or start line is to long
    }

    memcpy(start_line, packet, start_line_len);
    start_line[start_line_len] = '\0';  // startline is not a Null-Terminated String

    char method[16];
    char uri[256];
    char version[16];
    memset(method, 0, sizeof(method)); // TODO should be unnessecary, maybe delete
    memset(uri, 0, sizeof(uri));
    memset(version, 0, sizeof(version));
    int n = sscanf(start_line, "%15s %255s %15s", method, request.uri, version); // scan for Keywords and store the uri
    char *pos;
    if ((pos =strchr(version, '\r')) != NULL) {
        *pos = '\0';  // make sure version does not have \ŕ\n
    }

    request.valid = true; // if everything passed the request is valid
    if (n != 3) {
        request.valid = false; // start line does not match the pattern <method> <uri> <version>
    }
    if (strcmp(method, "GET") == 0) {
        request.method = GET;
    } else if (strcmp(method, "PUT") == 0) {
        request.method = PUT;
    } else if (strcmp(method, "DELETE") == 0) {
        request.method = DELETE;
    } else {
        request.method = other;
    }
    if (strcmp(version, "HTTP/1.0") == 0) {
        request.version = v1_0;
    }
    else if ( strcmp(version, "HTTP/1.1") == 0) {
        request.version = v1_1;
    }
    else {
        // invalid http_version
        request.version = not_specified;
        request.valid = false;
    }
    // --------------- Parse Headers ---------------


    const char* header_start = end_of_line + 2; // + \r\n
    const char *header_end;
    header_list_t *headers = NULL; // TODO what if an error accurs during the loop, after we already initialized the list, memory leak, we need to free the list
    header_list_t *prev = NULL;
    while ((header_end = strstr(header_start, "\r\n")) != NULL) {
        if (header_start == header_end) {
            break; //end of headers
        }
        size_t header_len = header_end - header_start;
        char header[256];
        if (header_len >= sizeof(header)) {
            request.valid = false; // header to big, TODO maybe copy part of header that fits into header
        }
        memcpy(header, header_start, header_len); // copy of the header line, we can edit
        header[header_len] = '\0'; // Header is now a Null-terminated String


        size_t key_len, value_len;

        char* colon = strchr(header, ':'); // Find the colon in the header;
        if (!colon || header == colon) {
            request.valid = false; // header does not match the pattern <key> : <value>
        }

        key_len = colon - header;

        if (*(colon + 1) != ' ') {
            request.valid = false;
        }
        char* value_start = colon + 1;
        while (*value_start == ' ') value_start++; // strip whitespace
        value_len = (header + header_len) - value_start;

        header_list_t *h = (header_list_t*) malloc(sizeof(header_list_t));
        if (key_len >= sizeof(h->key) || value_len >= sizeof(h->value)) {
            request.valid = false;
        }
        memcpy(h->key, header, key_len); // TODO what if key or value are bigger than the buffers
        h->key[key_len] = '\0';
        strncpy(h->value, value_start, value_len);
        h->value[value_len] = '\0';
        h->next = NULL;
        if (prev != NULL) {
            prev->next = h;
        } else {
            headers = h;
        }
        prev = h;
        header_start = header_end + 2; // \r\n
    }
    request.headers = headers;

    return request;
}


// Helper function to find a header in a header_list_t by a given key
// inputs: pointer to a header_list_t and the key
// return pointer to the header_list_t elemenent on success, NULL if such key does not exists in the list
const char* find_header_value_by_key (const header_list_t *headers, const char *key) {
    if (key == NULL) {
        return  NULL;
    }
    const header_list_t *cur = headers;
    while (cur != NULL && strcasecmp(cur->key, key) != 0) {
        cur = cur->next;
    }
    if (cur != NULL) {
        return cur->value;
    }
    return NULL;
}




