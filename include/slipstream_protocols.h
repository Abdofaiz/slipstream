#ifndef SLIPSTREAM_PROTOCOLS_H
#define SLIPSTREAM_PROTOCOLS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Protocol types supported by slipstream
typedef enum {
    SLIPSTREAM_PROTOCOL_TCP = 0,
    SLIPSTREAM_PROTOCOL_UDP = 1,
    SLIPSTREAM_PROTOCOL_HTTP = 2,
    SLIPSTREAM_PROTOCOL_HTTPS = 3,
    SLIPSTREAM_PROTOCOL_WEBSOCKET = 4,
    SLIPSTREAM_PROTOCOL_ICMP = 5,
    SLIPSTREAM_PROTOCOL_CUSTOM = 99
} slipstream_protocol_type_t;

// Protocol configuration structure
typedef struct {
    slipstream_protocol_type_t type;
    char name[32];
    uint16_t default_port;
    bool requires_connection;
    bool supports_multiplexing;
    bool supports_reliability;
} slipstream_protocol_config_t;

// Protocol handler function pointers
typedef struct {
    // Initialize protocol handler
    int (*init)(void* config);
    
    // Cleanup protocol handler
    void (*cleanup)(void* config);
    
    // Create connection/socket for this protocol
    int (*create_socket)(void* config, struct sockaddr_storage* target_addr);
    
    // Handle incoming data
    ssize_t (*handle_data)(void* config, int socket_fd, uint8_t* buffer, size_t buffer_size);
    
    // Send data through protocol
    ssize_t (*send_data)(void* config, int socket_fd, const uint8_t* data, size_t data_size);
    
    // Check if connection is ready
    bool (*is_ready)(void* config, int socket_fd);
    
    // Get protocol-specific configuration
    void* (*get_config)(slipstream_protocol_type_t type);
} slipstream_protocol_handler_t;

// Protocol manager structure
typedef struct {
    slipstream_protocol_type_t active_protocol;
    slipstream_protocol_handler_t* handler;
    void* protocol_config;
    bool initialized;
} slipstream_protocol_manager_t;

// Function declarations
int slipstream_protocol_manager_init(slipstream_protocol_manager_t* manager, slipstream_protocol_type_t protocol);
void slipstream_protocol_manager_cleanup(slipstream_protocol_manager_t* manager);

// Protocol-specific functions
int slipstream_protocol_tcp_init(void* config);
int slipstream_protocol_udp_init(void* config);
int slipstream_protocol_http_init(void* config);
int slipstream_protocol_websocket_init(void* config);

// Utility functions
const char* slipstream_protocol_get_name(slipstream_protocol_type_t type);
uint16_t slipstream_protocol_get_default_port(slipstream_protocol_type_t type);
bool slipstream_protocol_supports_multiplexing(slipstream_protocol_type_t type);

#ifdef __cplusplus
}
#endif

#endif // SLIPSTREAM_PROTOCOLS_H
