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

// Protocol configurations
static const slipstream_protocol_config_t protocol_configs[] = {
    {
        .type = SLIPSTREAM_PROTOCOL_TCP,
        .name = "TCP",
        .default_port = 80,
        .requires_connection = true,
        .supports_multiplexing = true,
        .supports_reliability = true
    },
    {
        .type = SLIPSTREAM_PROTOCOL_UDP,
        .name = "UDP",
        .default_port = 53,
        .requires_connection = false,
        .supports_multiplexing = false,
        .supports_reliability = false
    },
    {
        .type = SLIPSTREAM_PROTOCOL_HTTP,
        .name = "HTTP",
        .default_port = 80,
        .requires_connection = true,
        .supports_multiplexing = true,
        .supports_reliability = true
    },
    {
        .type = SLIPSTREAM_PROTOCOL_HTTPS,
        .name = "HTTPS",
        .default_port = 443,
        .requires_connection = true,
        .supports_multiplexing = true,
        .supports_reliability = true
    },
    {
        .type = SLIPSTREAM_PROTOCOL_WEBSOCKET,
        .name = "WebSocket",
        .default_port = 80,
        .requires_connection = true,
        .supports_multiplexing = true,
        .supports_reliability = true
    },
    {
        .type = SLIPSTREAM_PROTOCOL_ICMP,
        .name = "ICMP",
        .default_port = 0,
        .requires_connection = false,
        .supports_multiplexing = false,
        .supports_reliability = false
    }
};

// TCP Protocol Handler
static int tcp_create_socket(void* config, struct sockaddr_storage* target_addr) {
    int sock = socket(target_addr->ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        perror("TCP socket creation failed");
        return -1;
    }
    
    // Set socket options for TCP
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    return sock;
}

static ssize_t tcp_handle_data(void* config, int socket_fd, uint8_t* buffer, size_t buffer_size) {
    return recv(socket_fd, buffer, buffer_size, 0);
}

static ssize_t tcp_send_data(void* config, int socket_fd, const uint8_t* data, size_t data_size) {
    return send(socket_fd, data, data_size, MSG_NOSIGNAL);
}

static bool tcp_is_ready(void* config, int socket_fd) {
    // TCP is ready when connected
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);
    return error == 0;
}

static slipstream_protocol_handler_t tcp_handler = {
    .init = NULL,
    .cleanup = NULL,
    .create_socket = tcp_create_socket,
    .handle_data = tcp_handle_data,
    .send_data = tcp_send_data,
    .is_ready = tcp_is_ready,
    .get_config = NULL
};

// UDP Protocol Handler
static int udp_create_socket(void* config, struct sockaddr_storage* target_addr) {
    int sock = socket(target_addr->ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        perror("UDP socket creation failed");
        return -1;
    }
    
    // Set socket options for UDP
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    return sock;
}

static ssize_t udp_handle_data(void* config, int socket_fd, uint8_t* buffer, size_t buffer_size) {
    struct sockaddr_storage peer_addr;
    socklen_t peer_len = sizeof(peer_addr);
    return recvfrom(socket_fd, buffer, buffer_size, 0, 
                   (struct sockaddr*)&peer_addr, &peer_len);
}

static ssize_t udp_send_data(void* config, int socket_fd, const uint8_t* data, size_t data_size) {
    // For UDP, we need the target address from config
    // This is a simplified version - in practice, you'd store the target address
    return send(socket_fd, data, data_size, 0);
}

static bool udp_is_ready(void* config, int socket_fd) {
    // UDP is always "ready" - it's connectionless
    return true;
}

static slipstream_protocol_handler_t udp_handler = {
    .init = NULL,
    .cleanup = NULL,
    .create_socket = udp_create_socket,
    .handle_data = udp_handle_data,
    .send_data = udp_send_data,
    .is_ready = udp_is_ready,
    .get_config = NULL
};

// HTTP Protocol Handler (simplified)
static int http_create_socket(void* config, struct sockaddr_storage* target_addr) {
    // HTTP uses TCP underneath
    return tcp_create_socket(config, target_addr);
}

static ssize_t http_handle_data(void* config, int socket_fd, uint8_t* buffer, size_t buffer_size) {
    // HTTP data handling - could include HTTP header parsing
    return tcp_handle_data(config, socket_fd, buffer, buffer_size);
}

static ssize_t http_send_data(void* config, int socket_fd, const uint8_t* data, size_t data_size) {
    // HTTP data sending - could include HTTP header construction
    return tcp_send_data(config, socket_fd, data, data_size);
}

static bool http_is_ready(void* config, int socket_fd) {
    return tcp_is_ready(config, socket_fd);
}

static slipstream_protocol_handler_t http_handler = {
    .init = NULL,
    .cleanup = NULL,
    .create_socket = http_create_socket,
    .handle_data = http_handle_data,
    .send_data = http_send_data,
    .is_ready = http_is_ready,
    .get_config = NULL
};

// WebSocket Protocol Handler (simplified)
static int websocket_create_socket(void* config, struct sockaddr_storage* target_addr) {
    // WebSocket uses TCP underneath
    return tcp_create_socket(config, target_addr);
}

static ssize_t websocket_handle_data(void* config, int socket_fd, uint8_t* buffer, size_t buffer_size) {
    // WebSocket data handling - would include WebSocket frame parsing
    return tcp_handle_data(config, socket_fd, buffer, buffer_size);
}

static ssize_t websocket_send_data(void* config, int socket_fd, const uint8_t* data, size_t data_size) {
    // WebSocket data sending - would include WebSocket frame construction
    return tcp_send_data(config, socket_fd, data, data_size);
}

static bool websocket_is_ready(void* config, int socket_fd) {
    return tcp_is_ready(config, socket_fd);
}

static slipstream_protocol_handler_t websocket_handler = {
    .init = NULL,
    .cleanup = NULL,
    .create_socket = websocket_create_socket,
    .handle_data = websocket_handle_data,
    .send_data = websocket_send_data,
    .is_ready = websocket_is_ready,
    .get_config = NULL
};

// Protocol Manager Implementation
int slipstream_protocol_manager_init(slipstream_protocol_manager_t* manager, slipstream_protocol_type_t protocol) {
    if (manager == NULL) {
        return -1;
    }
    
    manager->active_protocol = protocol;
    manager->initialized = false;
    manager->protocol_config = NULL;
    
    // Select appropriate handler based on protocol type
    switch (protocol) {
        case SLIPSTREAM_PROTOCOL_TCP:
            manager->handler = &tcp_handler;
            break;
        case SLIPSTREAM_PROTOCOL_UDP:
            manager->handler = &udp_handler;
            break;
        case SLIPSTREAM_PROTOCOL_HTTP:
        case SLIPSTREAM_PROTOCOL_HTTPS:
            manager->handler = &http_handler;
            break;
        case SLIPSTREAM_PROTOCOL_WEBSOCKET:
            manager->handler = &websocket_handler;
            break;
        default:
            return -1;
    }
    
    // Initialize protocol if handler provides init function
    if (manager->handler->init != NULL) {
        int ret = manager->handler->init(manager->protocol_config);
        if (ret != 0) {
            return ret;
        }
    }
    
    manager->initialized = true;
    return 0;
}

void slipstream_protocol_manager_cleanup(slipstream_protocol_manager_t* manager) {
    if (manager == NULL || !manager->initialized) {
        return;
    }
    
    // Cleanup protocol if handler provides cleanup function
    if (manager->handler->cleanup != NULL) {
        manager->handler->cleanup(manager->protocol_config);
    }
    
    manager->initialized = false;
    manager->handler = NULL;
    manager->protocol_config = NULL;
}

// Utility Functions
const char* slipstream_protocol_get_name(slipstream_protocol_type_t type) {
    for (size_t i = 0; i < sizeof(protocol_configs) / sizeof(protocol_configs[0]); i++) {
        if (protocol_configs[i].type == type) {
            return protocol_configs[i].name;
        }
    }
    return "UNKNOWN";
}

uint16_t slipstream_protocol_get_default_port(slipstream_protocol_type_t type) {
    for (size_t i = 0; i < sizeof(protocol_configs) / sizeof(protocol_configs[0]); i++) {
        if (protocol_configs[i].type == type) {
            return protocol_configs[i].default_port;
        }
    }
    return 0;
}

bool slipstream_protocol_supports_multiplexing(slipstream_protocol_type_t type) {
    for (size_t i = 0; i < sizeof(protocol_configs) / sizeof(protocol_configs[0]); i++) {
        if (protocol_configs[i].type == type) {
            return protocol_configs[i].supports_multiplexing;
        }
    }
    return false;
}

// Protocol-specific initialization functions
int slipstream_protocol_tcp_init(void* config) {
    // TCP initialization - currently no special setup needed
    return 0;
}

int slipstream_protocol_udp_init(void* config) {
    // UDP initialization - currently no special setup needed
    return 0;
}

int slipstream_protocol_http_init(void* config) {
    // HTTP initialization - could set up HTTP-specific configurations
    return 0;
}

int slipstream_protocol_websocket_init(void* config) {
    // WebSocket initialization - could set up WebSocket-specific configurations
    return 0;
}
