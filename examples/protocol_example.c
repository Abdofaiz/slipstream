#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "slipstream_protocols.h"

// Example usage of the new protocol system
int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <protocol> <target_ip> [port]\n", argv[0]);
        printf("Protocols: tcp, udp, http, websocket\n");
        return 1;
    }
    
    const char* protocol_name = argv[1];
    const char* target_ip = argv[2];
    uint16_t port = 80; // Default port
    
    if (argc > 3) {
        port = atoi(argv[3]);
    }
    
    // Determine protocol type
    slipstream_protocol_type_t protocol_type;
    if (strcmp(protocol_name, "tcp") == 0) {
        protocol_type = SLIPSTREAM_PROTOCOL_TCP;
    } else if (strcmp(protocol_name, "udp") == 0) {
        protocol_type = SLIPSTREAM_PROTOCOL_UDP;
    } else if (strcmp(protocol_name, "http") == 0) {
        protocol_type = SLIPSTREAM_PROTOCOL_HTTP;
    } else if (strcmp(protocol_name, "websocket") == 0) {
        protocol_type = SLIPSTREAM_PROTOCOL_WEBSOCKET;
    } else {
        printf("Unknown protocol: %s\n", protocol_name);
        return 1;
    }
    
    printf("Initializing %s tunnel to %s:%d\n", 
           slipstream_protocol_get_name(protocol_type), target_ip, port);
    
    // Initialize protocol manager
    slipstream_protocol_manager_t manager;
    if (slipstream_protocol_manager_init(&manager, protocol_type) != 0) {
        printf("Failed to initialize protocol manager\n");
        return 1;
    }
    
    // Setup target address
    struct sockaddr_storage target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    
    // Try IPv4 first
    struct sockaddr_in* addr_in = (struct sockaddr_in*)&target_addr;
    addr_in->sin_family = AF_INET;
    addr_in->sin_port = htons(port);
    
    if (inet_pton(AF_INET, target_ip, &addr_in->sin_addr) != 1) {
        // Try IPv6
        struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)&target_addr;
        addr_in6->sin6_family = AF_INET6;
        addr_in6->sin6_port = htons(port);
        
        if (inet_pton(AF_INET6, target_ip, &addr_in6->sin6_addr) != 1) {
            printf("Invalid IP address: %s\n", target_ip);
            slipstream_protocol_manager_cleanup(&manager);
            return 1;
        }
    }
    
    // Create socket using protocol handler
    int socket_fd = manager.handler->create_socket(manager.protocol_config, &target_addr);
    if (socket_fd < 0) {
        printf("Failed to create socket\n");
        slipstream_protocol_manager_cleanup(&manager);
        return 1;
    }
    
    printf("Socket created successfully (fd: %d)\n", socket_fd);
    
    // For connection-oriented protocols, attempt to connect
    if (slipstream_protocol_supports_multiplexing(protocol_type)) {
        printf("Attempting to connect...\n");
        
        if (connect(socket_fd, (struct sockaddr*)&target_addr, sizeof(target_addr)) < 0) {
            perror("Connection failed");
            close(socket_fd);
            slipstream_protocol_manager_cleanup(&manager);
            return 1;
        }
        
        printf("Connected successfully!\n");
    }
    
    // Test data transfer
    const char* test_data = "Hello, Slipstream Protocol System!";
    size_t data_len = strlen(test_data);
    
    printf("Sending test data: %s\n", test_data);
    
    ssize_t bytes_sent = manager.handler->send_data(manager.protocol_config, socket_fd, 
                                                   (const uint8_t*)test_data, data_len);
    if (bytes_sent < 0) {
        perror("Failed to send data");
    } else {
        printf("Sent %ld bytes\n", bytes_sent);
    }
    
    // Try to receive response (for some protocols)
    if (protocol_type == SLIPSTREAM_PROTOCOL_TCP || protocol_type == SLIPSTREAM_PROTOCOL_HTTP) {
        uint8_t response_buffer[1024];
        ssize_t bytes_received = manager.handler->handle_data(manager.protocol_config, socket_fd, 
                                                             response_buffer, sizeof(response_buffer) - 1);
        if (bytes_received > 0) {
            response_buffer[bytes_received] = '\0';
            printf("Received response: %s\n", (char*)response_buffer);
        } else if (bytes_received < 0) {
            perror("Failed to receive data");
        }
    }
    
    // Cleanup
    close(socket_fd);
    slipstream_protocol_manager_cleanup(&manager);
    
    printf("Protocol test completed successfully!\n");
    return 0;
}
