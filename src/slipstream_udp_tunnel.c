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

// UDP Tunnel Configuration
typedef struct {
    struct sockaddr_storage target_addr;
    int udp_socket;
    bool connected;
    pthread_mutex_t mutex;
} slipstream_udp_config_t;

// UDP Tunnel Context
typedef struct {
    slipstream_udp_config_t* config;
    int client_socket;
    uint64_t stream_id;
    volatile bool active;
    pthread_t worker_thread;
} slipstream_udp_context_t;

// Forward declarations
static void* udp_tunnel_worker(void* arg);
static int udp_connect_to_target(slipstream_udp_config_t* config);
static ssize_t udp_forward_data(int from_socket, int to_socket, struct sockaddr_storage* target_addr);

// UDP Protocol Implementation
static int udp_tunnel_init(void* config) {
    slipstream_udp_config_t* udp_config = (slipstream_udp_config_t*)config;
    if (udp_config == NULL) {
        return -1;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&udp_config->mutex, NULL) != 0) {
        perror("Failed to initialize UDP mutex");
        return -1;
    }
    
    udp_config->connected = false;
    udp_config->udp_socket = -1;
    
    return 0;
}

static void udp_tunnel_cleanup(void* config) {
    slipstream_udp_config_t* udp_config = (slipstream_udp_config_t*)config;
    if (udp_config == NULL) {
        return;
    }
    
    pthread_mutex_lock(&udp_config->mutex);
    
    if (udp_config->udp_socket >= 0) {
        close(udp_config->udp_socket);
        udp_config->udp_socket = -1;
    }
    
    udp_config->connected = false;
    
    pthread_mutex_unlock(&udp_config->mutex);
    pthread_mutex_destroy(&udp_config->mutex);
}

static int udp_tunnel_create_socket(void* config, struct sockaddr_storage* target_addr) {
    slipstream_udp_config_t* udp_config = (slipstream_udp_config_t*)config;
    if (udp_config == NULL || target_addr == NULL) {
        return -1;
    }
    
    pthread_mutex_lock(&udp_config->mutex);
    
    // Create UDP socket
    int sock = socket(target_addr->ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        perror("UDP socket creation failed");
        pthread_mutex_unlock(&udp_config->mutex);
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Set receive timeout
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Store target address
    memcpy(&udp_config->target_addr, target_addr, sizeof(struct sockaddr_storage));
    udp_config->udp_socket = sock;
    
    pthread_mutex_unlock(&udp_config->mutex);
    
    return sock;
}

static ssize_t udp_tunnel_handle_data(void* config, int socket_fd, uint8_t* buffer, size_t buffer_size) {
    slipstream_udp_config_t* udp_config = (slipstream_udp_config_t*)config;
    if (udp_config == NULL) {
        return -1;
    }
    
    struct sockaddr_storage peer_addr;
    socklen_t peer_len = sizeof(peer_addr);
    
    ssize_t bytes_received = recvfrom(socket_fd, buffer, buffer_size, 0, 
                                     (struct sockaddr*)&peer_addr, &peer_len);
    
    if (bytes_received > 0) {
        // Update target address if it changed
        pthread_mutex_lock(&udp_config->mutex);
        memcpy(&udp_config->target_addr, &peer_addr, sizeof(struct sockaddr_storage));
        pthread_mutex_unlock(&udp_config->mutex);
    }
    
    return bytes_received;
}

static ssize_t udp_tunnel_send_data(void* config, int socket_fd, const uint8_t* data, size_t data_size) {
    slipstream_udp_config_t* udp_config = (slipstream_udp_config_t*)config;
    if (udp_config == NULL) {
        return -1;
    }
    
    pthread_mutex_lock(&udp_config->mutex);
    
    ssize_t bytes_sent = sendto(socket_fd, data, data_size, 0,
                               (struct sockaddr*)&udp_config->target_addr,
                               sizeof(udp_config->target_addr));
    
    pthread_mutex_unlock(&udp_config->mutex);
    
    return bytes_sent;
}

static bool udp_tunnel_is_ready(void* config, int socket_fd) {
    // UDP is always "ready" - it's connectionless
    return true;
}

// UDP Tunnel Handler
static slipstream_protocol_handler_t udp_tunnel_handler = {
    .init = udp_tunnel_init,
    .cleanup = udp_tunnel_cleanup,
    .create_socket = udp_tunnel_create_socket,
    .handle_data = udp_tunnel_handle_data,
    .send_data = udp_tunnel_send_data,
    .is_ready = udp_tunnel_is_ready,
    .get_config = NULL
};

// UDP Tunnel Worker Thread
static void* udp_tunnel_worker(void* arg) {
    slipstream_udp_context_t* ctx = (slipstream_udp_context_t*)arg;
    if (ctx == NULL || ctx->config == NULL) {
        return NULL;
    }
    
    printf("[UDP Tunnel %lu] Starting UDP tunnel worker\n", ctx->stream_id);
    
    struct pollfd fds[2];
    fds[0].fd = ctx->client_socket;
    fds[0].events = POLLIN;
    fds[1].fd = ctx->config->udp_socket;
    fds[1].events = POLLIN;
    
    uint8_t buffer[4096];
    
    while (ctx->active) {
        int ret = poll(fds, 2, 1000); // 1 second timeout
        
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("UDP tunnel poll failed");
            break;
        }
        
        if (ret == 0) {
            // Timeout - continue
            continue;
        }
        
        // Handle data from client to UDP target
        if (fds[0].revents & POLLIN) {
            ssize_t bytes_read = recv(ctx->client_socket, buffer, sizeof(buffer), 0);
            if (bytes_read > 0) {
                ssize_t bytes_sent = udp_tunnel_send_data(ctx->config, ctx->config->udp_socket, buffer, bytes_read);
                if (bytes_sent < 0) {
                    perror("Failed to send UDP data");
                    break;
                }
                printf("[UDP Tunnel %lu] Forwarded %ld bytes from client to UDP\n", ctx->stream_id, bytes_sent);
            } else if (bytes_read == 0) {
                printf("[UDP Tunnel %lu] Client disconnected\n", ctx->stream_id);
                break;
            } else {
                perror("Failed to read from client");
                break;
            }
        }
        
        // Handle data from UDP target to client
        if (fds[1].revents & POLLIN) {
            ssize_t bytes_read = udp_tunnel_handle_data(ctx->config, ctx->config->udp_socket, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                ssize_t bytes_sent = send(ctx->client_socket, buffer, bytes_read, MSG_NOSIGNAL);
                if (bytes_sent < 0) {
                    perror("Failed to send data to client");
                    break;
                }
                printf("[UDP Tunnel %lu] Forwarded %ld bytes from UDP to client\n", ctx->stream_id, bytes_sent);
            } else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Failed to read from UDP socket");
                break;
            }
        }
    }
    
    printf("[UDP Tunnel %lu] UDP tunnel worker exiting\n", ctx->stream_id);
    ctx->active = false;
    return NULL;
}

// Public API Functions
int slipstream_udp_tunnel_create(slipstream_udp_context_t** ctx, 
                                struct sockaddr_storage* target_addr,
                                uint64_t stream_id) {
    if (ctx == NULL || target_addr == NULL) {
        return -1;
    }
    
    *ctx = malloc(sizeof(slipstream_udp_context_t));
    if (*ctx == NULL) {
        return -1;
    }
    
    (*ctx)->config = malloc(sizeof(slipstream_udp_config_t));
    if ((*ctx)->config == NULL) {
        free(*ctx);
        return -1;
    }
    
    // Initialize UDP configuration
    if (udp_tunnel_init((*ctx)->config) != 0) {
        free((*ctx)->config);
        free(*ctx);
        return -1;
    }
    
    // Create UDP socket
    int udp_sock = udp_tunnel_create_socket((*ctx)->config, target_addr);
    if (udp_sock < 0) {
        udp_tunnel_cleanup((*ctx)->config);
        free((*ctx)->config);
        free(*ctx);
        return -1;
    }
    
    (*ctx)->stream_id = stream_id;
    (*ctx)->active = true;
    (*ctx)->client_socket = -1; // Will be set when client connects
    
    return 0;
}

int slipstream_udp_tunnel_start(slipstream_udp_context_t* ctx, int client_socket) {
    if (ctx == NULL || client_socket < 0) {
        return -1;
    }
    
    ctx->client_socket = client_socket;
    
    // Start worker thread
    if (pthread_create(&ctx->worker_thread, NULL, udp_tunnel_worker, ctx) != 0) {
        perror("Failed to create UDP tunnel worker thread");
        return -1;
    }
    
    pthread_setname_np(ctx->worker_thread, "udp_tunnel");
    
    return 0;
}

void slipstream_udp_tunnel_destroy(slipstream_udp_context_t* ctx) {
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
        udp_tunnel_cleanup(ctx->config);
        free(ctx->config);
    }
    
    free(ctx);
}

// Get UDP tunnel handler
slipstream_protocol_handler_t* slipstream_get_udp_tunnel_handler(void) {
    return &udp_tunnel_handler;
}
