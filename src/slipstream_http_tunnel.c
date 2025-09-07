#include "slipstream_protocols.h"
#include "slipstream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <poll.h>

// HTTP Tunnel Configuration
typedef struct {
    struct sockaddr_storage target_addr;
    int http_socket;
    bool connected;
    bool is_https;
    char hostname[256];
    uint16_t port;
    pthread_mutex_t mutex;
} slipstream_http_config_t;

// HTTP Tunnel Context
typedef struct {
    slipstream_http_config_t* config;
    int client_socket;
    uint64_t stream_id;
    volatile bool active;
    pthread_t worker_thread;
} slipstream_http_context_t;

// HTTP Request/Response structures
typedef struct {
    char method[16];
    char path[512];
    char version[16];
    char headers[2048];
    char body[4096];
    size_t body_length;
} http_request_t;

typedef struct {
    char version[16];
    int status_code;
    char status_text[64];
    char headers[2048];
    char body[4096];
    size_t body_length;
} http_response_t;

// Forward declarations
static void* http_tunnel_worker(void* arg);
static int http_parse_request(const char* data, size_t data_len, http_request_t* req);
static int http_build_response(const http_request_t* req, http_response_t* resp);
static int http_connect_to_target(slipstream_http_config_t* config);
static ssize_t http_forward_request(slipstream_http_config_t* config, const http_request_t* req);
static ssize_t http_forward_response(int client_socket, const http_response_t* resp);

// HTTP Protocol Implementation
static int http_tunnel_init(void* config) {
    slipstream_http_config_t* http_config = (slipstream_http_config_t*)config;
    if (http_config == NULL) {
        return -1;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&http_config->mutex, NULL) != 0) {
        perror("Failed to initialize HTTP mutex");
        return -1;
    }
    
    http_config->connected = false;
    http_config->http_socket = -1;
    http_config->is_https = false;
    http_config->hostname[0] = '\0';
    http_config->port = 80;
    
    return 0;
}

static void http_tunnel_cleanup(void* config) {
    slipstream_http_config_t* http_config = (slipstream_http_config_t*)config;
    if (http_config == NULL) {
        return;
    }
    
    pthread_mutex_lock(&http_config->mutex);
    
    if (http_config->http_socket >= 0) {
        close(http_config->http_socket);
        http_config->http_socket = -1;
    }
    
    http_config->connected = false;
    
    pthread_mutex_unlock(&http_config->mutex);
    pthread_mutex_destroy(&http_config->mutex);
}

static int http_tunnel_create_socket(void* config, struct sockaddr_storage* target_addr) {
    slipstream_http_config_t* http_config = (slipstream_http_config_t*)config;
    if (http_config == NULL || target_addr == NULL) {
        return -1;
    }
    
    pthread_mutex_lock(&http_config->mutex);
    
    // Create TCP socket (HTTP uses TCP)
    int sock = socket(target_addr->ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        perror("HTTP socket creation failed");
        pthread_mutex_unlock(&http_config->mutex);
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Set receive timeout
    struct timeval timeout;
    timeout.tv_sec = 30;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Store target address
    memcpy(&http_config->target_addr, target_addr, sizeof(struct sockaddr_storage));
    http_config->http_socket = sock;
    
    pthread_mutex_unlock(&http_config->mutex);
    
    return sock;
}

static ssize_t http_tunnel_handle_data(void* config, int socket_fd, uint8_t* buffer, size_t buffer_size) {
    slipstream_http_config_t* http_config = (slipstream_http_config_t*)config;
    if (http_config == NULL) {
        return -1;
    }
    
    return recv(socket_fd, buffer, buffer_size, 0);
}

static ssize_t http_tunnel_send_data(void* config, int socket_fd, const uint8_t* data, size_t data_size) {
    slipstream_http_config_t* http_config = (slipstream_http_config_t*)config;
    if (http_config == NULL) {
        return -1;
    }
    
    return send(socket_fd, data, data_size, MSG_NOSIGNAL);
}

static bool http_tunnel_is_ready(void* config, int socket_fd) {
    slipstream_http_config_t* http_config = (slipstream_http_config_t*)config;
    if (http_config == NULL) {
        return false;
    }
    
    pthread_mutex_lock(&http_config->mutex);
    bool ready = http_config->connected;
    pthread_mutex_unlock(&http_config->mutex);
    
    return ready;
}

// HTTP Tunnel Handler
static slipstream_protocol_handler_t http_tunnel_handler = {
    .init = http_tunnel_init,
    .cleanup = http_tunnel_cleanup,
    .create_socket = http_tunnel_create_socket,
    .handle_data = http_tunnel_handle_data,
    .send_data = http_tunnel_send_data,
    .is_ready = http_tunnel_is_ready,
    .get_config = NULL
};

// HTTP Request Parsing
static int http_parse_request(const char* data, size_t data_len, http_request_t* req) {
    if (data == NULL || req == NULL) {
        return -1;
    }
    
    // Initialize request structure
    memset(req, 0, sizeof(http_request_t));
    
    // Find end of first line (request line)
    const char* line_end = strstr(data, "\r\n");
    if (line_end == NULL) {
        return -1;
    }
    
    // Parse request line: METHOD PATH VERSION
    char request_line[1024];
    size_t line_len = line_end - data;
    if (line_len >= sizeof(request_line)) {
        return -1;
    }
    
    strncpy(request_line, data, line_len);
    request_line[line_len] = '\0';
    
    // Parse method, path, and version
    if (sscanf(request_line, "%15s %511s %15s", req->method, req->path, req->version) != 3) {
        return -1;
    }
    
    // Find headers section
    const char* headers_start = line_end + 2;
    const char* headers_end = strstr(headers_start, "\r\n\r\n");
    if (headers_end == NULL) {
        return -1;
    }
    
    // Copy headers
    size_t headers_len = headers_end - headers_start;
    if (headers_len >= sizeof(req->headers)) {
        headers_len = sizeof(req->headers) - 1;
    }
    strncpy(req->headers, headers_start, headers_len);
    req->headers[headers_len] = '\0';
    
    // Copy body if present
    const char* body_start = headers_end + 4;
    size_t body_len = data_len - (body_start - data);
    if (body_len > 0 && body_len < sizeof(req->body)) {
        strncpy(req->body, body_start, body_len);
        req->body[body_len] = '\0';
        req->body_length = body_len;
    }
    
    return 0;
}

// HTTP Response Building
static int http_build_response(const http_request_t* req, http_response_t* resp) {
    if (req == NULL || resp == NULL) {
        return -1;
    }
    
    // Initialize response structure
    memset(resp, 0, sizeof(http_response_t));
    
    // Set version
    strncpy(resp->version, req->version, sizeof(resp->version) - 1);
    
    // Set status (simplified - always 200 OK for now)
    resp->status_code = 200;
    strcpy(resp->status_text, "OK");
    
    // Build headers
    snprintf(resp->headers, sizeof(resp->headers),
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "Server: Slipstream-HTTP-Tunnel/1.0\r\n",
             resp->body_length);
    
    // Simple response body
    strcpy(resp->body, "<html><body><h1>Slipstream HTTP Tunnel</h1><p>Request processed successfully</p></body></html>");
    resp->body_length = strlen(resp->body);
    
    return 0;
}

// HTTP Tunnel Worker Thread
static void* http_tunnel_worker(void* arg) {
    slipstream_http_context_t* ctx = (slipstream_http_context_t*)arg;
    if (ctx == NULL || ctx->config == NULL) {
        return NULL;
    }
    
    printf("[HTTP Tunnel %lu] Starting HTTP tunnel worker\n", ctx->stream_id);
    
    uint8_t buffer[8192];
    http_request_t request;
    http_response_t response;
    
    while (ctx->active) {
        // Read HTTP request from client
        ssize_t bytes_read = recv(ctx->client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf("[HTTP Tunnel %lu] Client disconnected\n", ctx->stream_id);
            } else {
                perror("Failed to read HTTP request");
            }
            break;
        }
        
        buffer[bytes_read] = '\0';
        printf("[HTTP Tunnel %lu] Received %ld bytes HTTP request\n", ctx->stream_id, bytes_read);
        
        // Parse HTTP request
        if (http_parse_request((const char*)buffer, bytes_read, &request) != 0) {
            printf("[HTTP Tunnel %lu] Failed to parse HTTP request\n", ctx->stream_id);
            continue;
        }
        
        printf("[HTTP Tunnel %lu] Parsed request: %s %s %s\n", 
               ctx->stream_id, request.method, request.path, request.version);
        
        // Connect to target if not connected
        pthread_mutex_lock(&ctx->config->mutex);
        if (!ctx->config->connected) {
            if (http_connect_to_target(ctx->config) != 0) {
                printf("[HTTP Tunnel %lu] Failed to connect to target\n", ctx->stream_id);
                pthread_mutex_unlock(&ctx->config->mutex);
                continue;
            }
        }
        pthread_mutex_unlock(&ctx->config->mutex);
        
        // Forward request to target
        ssize_t bytes_sent = http_forward_request(ctx->config, &request);
        if (bytes_sent < 0) {
            printf("[HTTP Tunnel %lu] Failed to forward request\n", ctx->stream_id);
            continue;
        }
        
        // Read response from target
        ssize_t response_bytes = recv(ctx->config->http_socket, buffer, sizeof(buffer) - 1, 0);
        if (response_bytes <= 0) {
            printf("[HTTP Tunnel %lu] Failed to read response from target\n", ctx->stream_id);
            continue;
        }
        
        buffer[response_bytes] = '\0';
        printf("[HTTP Tunnel %lu] Received %ld bytes HTTP response\n", ctx->stream_id, response_bytes);
        
        // Forward response to client
        ssize_t client_bytes_sent = send(ctx->client_socket, buffer, response_bytes, MSG_NOSIGNAL);
        if (client_bytes_sent < 0) {
            perror("Failed to send HTTP response to client");
            break;
        }
        
        printf("[HTTP Tunnel %lu] Forwarded %ld bytes to client\n", ctx->stream_id, client_bytes_sent);
    }
    
    printf("[HTTP Tunnel %lu] HTTP tunnel worker exiting\n", ctx->stream_id);
    ctx->active = false;
    return NULL;
}

// Connect to HTTP target
static int http_connect_to_target(slipstream_http_config_t* config) {
    if (config == NULL) {
        return -1;
    }
    
    if (config->http_socket < 0) {
        return -1;
    }
    
    // Connect to target
    if (connect(config->http_socket, (struct sockaddr*)&config->target_addr, 
                sizeof(config->target_addr)) < 0) {
        perror("Failed to connect to HTTP target");
        return -1;
    }
    
    config->connected = true;
    printf("Connected to HTTP target\n");
    
    return 0;
}

// Forward HTTP request to target
static ssize_t http_forward_request(slipstream_http_config_t* config, const http_request_t* req) {
    if (config == NULL || req == NULL) {
        return -1;
    }
    
    // Build HTTP request string
    char http_request[8192];
    snprintf(http_request, sizeof(http_request),
             "%s %s %s\r\n"
             "%s"
             "\r\n"
             "%s",
             req->method, req->path, req->version,
             req->headers,
             req->body);
    
    size_t request_len = strlen(http_request);
    
    // Send request to target
    ssize_t bytes_sent = send(config->http_socket, http_request, request_len, MSG_NOSIGNAL);
    if (bytes_sent < 0) {
        perror("Failed to send HTTP request to target");
        return -1;
    }
    
    return bytes_sent;
}

// Forward HTTP response to client
static ssize_t http_forward_response(int client_socket, const http_response_t* resp) {
    if (resp == NULL) {
        return -1;
    }
    
    // Build HTTP response string
    char http_response[8192];
    snprintf(http_response, sizeof(http_response),
             "%s %d %s\r\n"
             "%s"
             "\r\n"
             "%s",
             resp->version, resp->status_code, resp->status_text,
             resp->headers,
             resp->body);
    
    size_t response_len = strlen(http_response);
    
    // Send response to client
    ssize_t bytes_sent = send(client_socket, http_response, response_len, MSG_NOSIGNAL);
    if (bytes_sent < 0) {
        perror("Failed to send HTTP response to client");
        return -1;
    }
    
    return bytes_sent;
}

// Public API Functions
int slipstream_http_tunnel_create(slipstream_http_context_t** ctx, 
                                 struct sockaddr_storage* target_addr,
                                 uint64_t stream_id,
                                 bool is_https) {
    if (ctx == NULL || target_addr == NULL) {
        return -1;
    }
    
    *ctx = malloc(sizeof(slipstream_http_context_t));
    if (*ctx == NULL) {
        return -1;
    }
    
    (*ctx)->config = malloc(sizeof(slipstream_http_config_t));
    if ((*ctx)->config == NULL) {
        free(*ctx);
        return -1;
    }
    
    // Initialize HTTP configuration
    if (http_tunnel_init((*ctx)->config) != 0) {
        free((*ctx)->config);
        free(*ctx);
        return -1;
    }
    
    // Create HTTP socket
    int http_sock = http_tunnel_create_socket((*ctx)->config, target_addr);
    if (http_sock < 0) {
        http_tunnel_cleanup((*ctx)->config);
        free((*ctx)->config);
        free(*ctx);
        return -1;
    }
    
    (*ctx)->stream_id = stream_id;
    (*ctx)->active = true;
    (*ctx)->client_socket = -1; // Will be set when client connects
    (*ctx)->config->is_https = is_https;
    
    return 0;
}

int slipstream_http_tunnel_start(slipstream_http_context_t* ctx, int client_socket) {
    if (ctx == NULL || client_socket < 0) {
        return -1;
    }
    
    ctx->client_socket = client_socket;
    
    // Start worker thread
    if (pthread_create(&ctx->worker_thread, NULL, http_tunnel_worker, ctx) != 0) {
        perror("Failed to create HTTP tunnel worker thread");
        return -1;
    }
    
    pthread_setname_np(ctx->worker_thread, "http_tunnel");
    
    return 0;
}

void slipstream_http_tunnel_destroy(slipstream_http_context_t* ctx) {
    if (ctx == NULL) {
        return;
    }
    
    ctx->active = false;
    
    // Wait for worker thread to finish
    if (ctx->worker_thread) {
        pthread_join(ctx->worker_thread, NULL);
    }
    
    // Cleanup
    if (ctx->config) {
        http_tunnel_cleanup(ctx->config);
        free(ctx->config);
    }
    
    free(ctx);
}

// Get HTTP tunnel handler
slipstream_protocol_handler_t* slipstream_get_http_tunnel_handler(void) {
    return &http_tunnel_handler;
}
