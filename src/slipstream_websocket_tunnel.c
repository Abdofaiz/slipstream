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
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

// WebSocket Configuration
typedef struct {
    struct sockaddr_storage target_addr;
    int ws_socket;
    bool connected;
    bool handshake_completed;
    char hostname[256];
    uint16_t port;
    char websocket_key[64];
    pthread_mutex_t mutex;
} slipstream_websocket_config_t;

// WebSocket Tunnel Context
typedef struct {
    slipstream_websocket_config_t* config;
    int client_socket;
    uint64_t stream_id;
    volatile bool active;
    pthread_t worker_thread;
} slipstream_websocket_context_t;

// WebSocket Frame Structure
typedef struct {
    bool fin;
    uint8_t opcode;
    bool mask;
    uint64_t payload_length;
    uint8_t masking_key[4];
    uint8_t* payload;
} websocket_frame_t;

// Forward declarations
static void* websocket_tunnel_worker(void* arg);
static int websocket_handshake(slipstream_websocket_context_t* ctx);
static int websocket_parse_frame(const uint8_t* data, size_t data_len, websocket_frame_t* frame);
static int websocket_build_frame(const websocket_frame_t* frame, uint8_t* buffer, size_t buffer_size);
static int websocket_send_frame(int socket, const websocket_frame_t* frame);
static int websocket_receive_frame(int socket, websocket_frame_t* frame);
static void websocket_cleanup_frame(websocket_frame_t* frame);
static char* base64_encode(const uint8_t* data, size_t data_len);

// WebSocket Protocol Implementation
static int websocket_tunnel_init(void* config) {
    slipstream_websocket_config_t* ws_config = (slipstream_websocket_config_t*)config;
    if (ws_config == NULL) {
        return -1;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&ws_config->mutex, NULL) != 0) {
        perror("Failed to initialize WebSocket mutex");
        return -1;
    }
    
    ws_config->connected = false;
    ws_config->handshake_completed = false;
    ws_config->ws_socket = -1;
    ws_config->hostname[0] = '\0';
    ws_config->port = 80;
    ws_config->websocket_key[0] = '\0';
    
    return 0;
}

static void websocket_tunnel_cleanup(void* config) {
    slipstream_websocket_config_t* ws_config = (slipstream_websocket_config_t*)config;
    if (ws_config == NULL) {
        return;
    }
    
    pthread_mutex_lock(&ws_config->mutex);
    
    if (ws_config->ws_socket >= 0) {
        close(ws_config->ws_socket);
        ws_config->ws_socket = -1;
    }
    
    ws_config->connected = false;
    ws_config->handshake_completed = false;
    
    pthread_mutex_unlock(&ws_config->mutex);
    pthread_mutex_destroy(&ws_config->mutex);
}

static int websocket_tunnel_create_socket(void* config, struct sockaddr_storage* target_addr) {
    slipstream_websocket_config_t* ws_config = (slipstream_websocket_config_t*)config;
    if (ws_config == NULL || target_addr == NULL) {
        return -1;
    }
    
    pthread_mutex_lock(&ws_config->mutex);
    
    // Create TCP socket (WebSocket uses TCP)
    int sock = socket(target_addr->ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        perror("WebSocket socket creation failed");
        pthread_mutex_unlock(&ws_config->mutex);
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
    memcpy(&ws_config->target_addr, target_addr, sizeof(struct sockaddr_storage));
    ws_config->ws_socket = sock;
    
    pthread_mutex_unlock(&ws_config->mutex);
    
    return sock;
}

static ssize_t websocket_tunnel_handle_data(void* config, int socket_fd, uint8_t* buffer, size_t buffer_size) {
    slipstream_websocket_config_t* ws_config = (slipstream_websocket_config_t*)config;
    if (ws_config == NULL) {
        return -1;
    }
    
    return recv(socket_fd, buffer, buffer_size, 0);
}

static ssize_t websocket_tunnel_send_data(void* config, int socket_fd, const uint8_t* data, size_t data_size) {
    slipstream_websocket_config_t* ws_config = (slipstream_websocket_config_t*)config;
    if (ws_config == NULL) {
        return -1;
    }
    
    return send(socket_fd, data, data_size, MSG_NOSIGNAL);
}

static bool websocket_tunnel_is_ready(void* config, int socket_fd) {
    slipstream_websocket_config_t* ws_config = (slipstream_websocket_config_t*)config;
    if (ws_config == NULL) {
        return false;
    }
    
    pthread_mutex_lock(&ws_config->mutex);
    bool ready = ws_config->connected && ws_config->handshake_completed;
    pthread_mutex_unlock(&ws_config->mutex);
    
    return ready;
}

// WebSocket Tunnel Handler
static slipstream_protocol_handler_t websocket_tunnel_handler = {
    .init = websocket_tunnel_init,
    .cleanup = websocket_tunnel_cleanup,
    .create_socket = websocket_tunnel_create_socket,
    .handle_data = websocket_tunnel_handle_data,
    .send_data = websocket_tunnel_send_data,
    .is_ready = websocket_tunnel_is_ready,
    .get_config = NULL
};

// WebSocket Handshake
static int websocket_handshake(slipstream_websocket_context_t* ctx) {
    if (ctx == NULL || ctx->config == NULL) {
        return -1;
    }
    
    printf("[WebSocket Tunnel %lu] Starting WebSocket handshake\n", ctx->stream_id);
    
    // Connect to WebSocket server
    pthread_mutex_lock(&ctx->config->mutex);
    if (connect(ctx->config->ws_socket, (struct sockaddr*)&ctx->config->target_addr, 
                sizeof(ctx->config->target_addr)) < 0) {
        perror("Failed to connect to WebSocket server");
        pthread_mutex_unlock(&ctx->config->mutex);
        return -1;
    }
    ctx->config->connected = true;
    pthread_mutex_unlock(&ctx->config->mutex);
    
    // Generate WebSocket key
    const char* key_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 16; i++) {
        ctx->config->websocket_key[i] = key_chars[rand() % 64];
    }
    ctx->config->websocket_key[16] = '\0';
    
    // Build WebSocket handshake request
    char handshake_request[1024];
    snprintf(handshake_request, sizeof(handshake_request),
             "GET / HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Key: %s\r\n"
             "Sec-WebSocket-Version: 13\r\n"
             "\r\n",
             ctx->config->hostname, ctx->config->port, ctx->config->websocket_key);
    
    // Send handshake request
    ssize_t bytes_sent = send(ctx->config->ws_socket, handshake_request, strlen(handshake_request), 0);
    if (bytes_sent < 0) {
        perror("Failed to send WebSocket handshake");
        return -1;
    }
    
    // Read handshake response
    char handshake_response[2048];
    ssize_t bytes_received = recv(ctx->config->ws_socket, handshake_response, sizeof(handshake_response) - 1, 0);
    if (bytes_received <= 0) {
        perror("Failed to receive WebSocket handshake response");
        return -1;
    }
    
    handshake_response[bytes_received] = '\0';
    
    // Check if handshake was successful
    if (strstr(handshake_response, "101 Switching Protocols") == NULL) {
        printf("[WebSocket Tunnel %lu] WebSocket handshake failed\n", ctx->stream_id);
        return -1;
    }
    
    pthread_mutex_lock(&ctx->config->mutex);
    ctx->config->handshake_completed = true;
    pthread_mutex_unlock(&ctx->config->mutex);
    
    printf("[WebSocket Tunnel %lu] WebSocket handshake completed\n", ctx->stream_id);
    return 0;
}

// WebSocket Frame Parsing
static int websocket_parse_frame(const uint8_t* data, size_t data_len, websocket_frame_t* frame) {
    if (data == NULL || frame == NULL || data_len < 2) {
        return -1;
    }
    
    // Initialize frame structure
    memset(frame, 0, sizeof(websocket_frame_t));
    
    // Parse first byte
    frame->fin = (data[0] & 0x80) != 0;
    frame->opcode = data[0] & 0x0F;
    
    // Parse second byte
    frame->mask = (data[1] & 0x80) != 0;
    uint8_t payload_len = data[1] & 0x7F;
    
    size_t header_len = 2;
    
    // Parse payload length
    if (payload_len == 126) {
        if (data_len < 4) return -1;
        frame->payload_length = (data[2] << 8) | data[3];
        header_len = 4;
    } else if (payload_len == 127) {
        if (data_len < 10) return -1;
        frame->payload_length = 0;
        for (int i = 0; i < 8; i++) {
            frame->payload_length = (frame->payload_length << 8) | data[2 + i];
        }
        header_len = 10;
    } else {
        frame->payload_length = payload_len;
    }
    
    // Parse masking key
    if (frame->mask) {
        if (data_len < header_len + 4) return -1;
        memcpy(frame->masking_key, &data[header_len], 4);
        header_len += 4;
    }
    
    // Parse payload
    if (data_len < header_len + frame->payload_length) return -1;
    
    if (frame->payload_length > 0) {
        frame->payload = malloc(frame->payload_length);
        if (frame->payload == NULL) return -1;
        
        memcpy(frame->payload, &data[header_len], frame->payload_length);
        
        // Unmask payload if masked
        if (frame->mask) {
            for (size_t i = 0; i < frame->payload_length; i++) {
                frame->payload[i] ^= frame->masking_key[i % 4];
            }
        }
    }
    
    return 0;
}

// WebSocket Frame Building
static int websocket_build_frame(const websocket_frame_t* frame, uint8_t* buffer, size_t buffer_size) {
    if (frame == NULL || buffer == NULL) {
        return -1;
    }
    
    size_t header_len = 2;
    size_t total_len = header_len + frame->payload_length;
    
    if (frame->payload_length > 125) {
        header_len += 2;
        total_len += 2;
    }
    if (frame->payload_length > 65535) {
        header_len += 6;
        total_len += 6;
    }
    if (frame->mask) {
        header_len += 4;
        total_len += 4;
    }
    
    if (buffer_size < total_len) {
        return -1;
    }
    
    // Build frame header
    buffer[0] = (frame->fin ? 0x80 : 0x00) | (frame->opcode & 0x0F);
    
    if (frame->payload_length < 126) {
        buffer[1] = (frame->mask ? 0x80 : 0x00) | frame->payload_length;
    } else if (frame->payload_length < 65536) {
        buffer[1] = (frame->mask ? 0x80 : 0x00) | 126;
        buffer[2] = (frame->payload_length >> 8) & 0xFF;
        buffer[3] = frame->payload_length & 0xFF;
    } else {
        buffer[1] = (frame->mask ? 0x80 : 0x00) | 127;
        for (int i = 0; i < 8; i++) {
            buffer[2 + i] = (frame->payload_length >> (56 - i * 8)) & 0xFF;
        }
    }
    
    // Add masking key if needed
    size_t payload_offset = header_len;
    if (frame->mask) {
        memcpy(&buffer[payload_offset - 4], frame->masking_key, 4);
    }
    
    // Add payload
    if (frame->payload_length > 0 && frame->payload != NULL) {
        memcpy(&buffer[payload_offset], frame->payload, frame->payload_length);
        
        // Mask payload if needed
        if (frame->mask) {
            for (size_t i = 0; i < frame->payload_length; i++) {
                buffer[payload_offset + i] ^= frame->masking_key[i % 4];
            }
        }
    }
    
    return total_len;
}

// WebSocket Tunnel Worker Thread
static void* websocket_tunnel_worker(void* arg) {
    slipstream_websocket_context_t* ctx = (slipstream_websocket_context_t*)arg;
    if (ctx == NULL || ctx->config == NULL) {
        return NULL;
    }
    
    printf("[WebSocket Tunnel %lu] Starting WebSocket tunnel worker\n", ctx->stream_id);
    
    // Perform WebSocket handshake
    if (websocket_handshake(ctx) != 0) {
        printf("[WebSocket Tunnel %lu] WebSocket handshake failed\n", ctx->stream_id);
        ctx->active = false;
        return NULL;
    }
    
    struct pollfd fds[2];
    fds[0].fd = ctx->client_socket;
    fds[0].events = POLLIN;
    fds[1].fd = ctx->config->ws_socket;
    fds[1].events = POLLIN;
    
    uint8_t buffer[8192];
    websocket_frame_t frame;
    
    while (ctx->active) {
        int ret = poll(fds, 2, 1000); // 1 second timeout
        
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("WebSocket tunnel poll failed");
            break;
        }
        
        if (ret == 0) {
            // Timeout - continue
            continue;
        }
        
        // Handle data from client to WebSocket server
        if (fds[0].revents & POLLIN) {
            ssize_t bytes_read = recv(ctx->client_socket, buffer, sizeof(buffer), 0);
            if (bytes_read > 0) {
                // Create WebSocket frame
                websocket_frame_t client_frame = {
                    .fin = true,
                    .opcode = 1, // Text frame
                    .mask = false,
                    .payload_length = bytes_read,
                    .payload = buffer
                };
                
                // Send frame to WebSocket server
                uint8_t frame_buffer[8192];
                int frame_len = websocket_build_frame(&client_frame, frame_buffer, sizeof(frame_buffer));
                if (frame_len > 0) {
                    ssize_t bytes_sent = send(ctx->config->ws_socket, frame_buffer, frame_len, 0);
                    if (bytes_sent < 0) {
                        perror("Failed to send WebSocket frame");
                        break;
                    }
                    printf("[WebSocket Tunnel %lu] Forwarded %ld bytes from client to WebSocket\n", ctx->stream_id, bytes_sent);
                }
            } else if (bytes_read == 0) {
                printf("[WebSocket Tunnel %lu] Client disconnected\n", ctx->stream_id);
                break;
            } else {
                perror("Failed to read from client");
                break;
            }
        }
        
        // Handle data from WebSocket server to client
        if (fds[1].revents & POLLIN) {
            ssize_t bytes_read = recv(ctx->config->ws_socket, buffer, sizeof(buffer), 0);
            if (bytes_read > 0) {
                // Parse WebSocket frame
                if (websocket_parse_frame(buffer, bytes_read, &frame) == 0) {
                    if (frame.payload_length > 0 && frame.payload != NULL) {
                        ssize_t bytes_sent = send(ctx->client_socket, frame.payload, frame.payload_length, MSG_NOSIGNAL);
                        if (bytes_sent < 0) {
                            perror("Failed to send data to client");
                            websocket_cleanup_frame(&frame);
                            break;
                        }
                        printf("[WebSocket Tunnel %lu] Forwarded %ld bytes from WebSocket to client\n", ctx->stream_id, bytes_sent);
                    }
                    websocket_cleanup_frame(&frame);
                }
            } else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Failed to read from WebSocket socket");
                break;
            }
        }
    }
    
    printf("[WebSocket Tunnel %lu] WebSocket tunnel worker exiting\n", ctx->stream_id);
    ctx->active = false;
    return NULL;
}

// Cleanup WebSocket frame
static void websocket_cleanup_frame(websocket_frame_t* frame) {
    if (frame == NULL) {
        return;
    }
    
    if (frame->payload != NULL) {
        free(frame->payload);
        frame->payload = NULL;
    }
}

// Public API Functions
int slipstream_websocket_tunnel_create(slipstream_websocket_context_t** ctx, 
                                      struct sockaddr_storage* target_addr,
                                      uint64_t stream_id,
                                      const char* hostname,
                                      uint16_t port) {
    if (ctx == NULL || target_addr == NULL) {
        return -1;
    }
    
    *ctx = malloc(sizeof(slipstream_websocket_context_t));
    if (*ctx == NULL) {
        return -1;
    }
    
    (*ctx)->config = malloc(sizeof(slipstream_websocket_config_t));
    if ((*ctx)->config == NULL) {
        free(*ctx);
        return -1;
    }
    
    // Initialize WebSocket configuration
    if (websocket_tunnel_init((*ctx)->config) != 0) {
        free((*ctx)->config);
        free(*ctx);
        return -1;
    }
    
    // Create WebSocket socket
    int ws_sock = websocket_tunnel_create_socket((*ctx)->config, target_addr);
    if (ws_sock < 0) {
        websocket_tunnel_cleanup((*ctx)->config);
        free((*ctx)->config);
        free(*ctx);
        return -1;
    }
    
    (*ctx)->stream_id = stream_id;
    (*ctx)->active = true;
    (*ctx)->client_socket = -1; // Will be set when client connects
    
    // Set hostname and port
    if (hostname != NULL) {
        strncpy((*ctx)->config->hostname, hostname, sizeof((*ctx)->config->hostname) - 1);
        (*ctx)->config->hostname[sizeof((*ctx)->config->hostname) - 1] = '\0';
    }
    (*ctx)->config->port = port;
    
    return 0;
}

int slipstream_websocket_tunnel_start(slipstream_websocket_context_t* ctx, int client_socket) {
    if (ctx == NULL || client_socket < 0) {
        return -1;
    }
    
    ctx->client_socket = client_socket;
    
    // Start worker thread
    if (pthread_create(&ctx->worker_thread, NULL, websocket_tunnel_worker, ctx) != 0) {
        perror("Failed to create WebSocket tunnel worker thread");
        return -1;
    }
    
    pthread_setname_np(ctx->worker_thread, "websocket_tunnel");
    
    return 0;
}

void slipstream_websocket_tunnel_destroy(slipstream_websocket_context_t* ctx) {
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
        websocket_tunnel_cleanup(ctx->config);
        free(ctx->config);
    }
    
    free(ctx);
}

// Get WebSocket tunnel handler
slipstream_protocol_handler_t* slipstream_get_websocket_tunnel_handler(void) {
    return &websocket_tunnel_handler;
}
