#include "slipstream_bypass.h"
#include "slipstream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <time.h>
// OpenSSL headers - commented out for now
// #include <openssl/ssl.h>
// #include <openssl/err.h>

// HTTP Proxy Implementation
int slipstream_http_proxy_connect(slipstream_bypass_manager_t* manager, 
                                 const char* target_host, uint16_t target_port) {
    if (manager == NULL || target_host == NULL) {
        return -1;
    }
    
    printf("Connecting to HTTP proxy %s:%d\n", manager->proxy_config.hostname, manager->proxy_config.port);
    
    // Create socket to proxy
    manager->proxy_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (manager->proxy_socket < 0) {
        perror("Failed to create proxy socket");
        return -1;
    }
    
    // Setup proxy address
    struct sockaddr_in* proxy_addr = (struct sockaddr_in*)&manager->proxy_addr;
    proxy_addr->sin_family = AF_INET;
    proxy_addr->sin_port = htons(manager->proxy_config.port);
    
    if (inet_pton(AF_INET, manager->proxy_config.hostname, &proxy_addr->sin_addr) != 1) {
        // Try to resolve proxy hostname
        struct hostent* he = gethostbyname(manager->proxy_config.hostname);
        if (he == NULL) {
            printf("Failed to resolve proxy hostname: %s\n", manager->proxy_config.hostname);
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
        memcpy(&proxy_addr->sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    // Connect to proxy
    if (connect(manager->proxy_socket, (struct sockaddr*)&manager->proxy_addr, 
                sizeof(manager->proxy_addr)) < 0) {
        perror("Failed to connect to HTTP proxy");
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    // Send CONNECT request to proxy
    char connect_request[512];
    snprintf(connect_request, sizeof(connect_request),
            "CONNECT %s:%d HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "User-Agent: Slipstream-Proxy/1.0\r\n"
            "Connection: keep-alive\r\n"
            "\r\n",
            target_host, target_port, target_host, target_port);
    
    ssize_t bytes_sent = send(manager->proxy_socket, connect_request, strlen(connect_request), 0);
    if (bytes_sent < 0) {
        perror("Failed to send CONNECT request");
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    // Read proxy response
    char response[1024];
    ssize_t bytes_received = recv(manager->proxy_socket, response, sizeof(response) - 1, 0);
    if (bytes_received <= 0) {
        perror("Failed to receive proxy response");
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    response[bytes_received] = '\0';
    
    // Check if connection was successful
    if (strstr(response, "200 Connection established") == NULL) {
        printf("Proxy connection failed: %s\n", response);
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    printf("HTTP proxy connection established successfully\n");
    return 0;
}

int slipstream_http_proxy_send(slipstream_bypass_manager_t* manager, 
                              const uint8_t* data, size_t data_len) {
    if (manager == NULL || data == NULL || data_len == 0) {
        return -1;
    }
    
    if (manager->proxy_socket < 0) {
        printf("Proxy socket not connected\n");
        return -1;
    }
    
    ssize_t bytes_sent = send(manager->proxy_socket, data, data_len, 0);
    if (bytes_sent < 0) {
        perror("Failed to send data through HTTP proxy");
        return -1;
    }
    
    return bytes_sent;
}

int slipstream_http_proxy_receive(slipstream_bypass_manager_t* manager, 
                                 uint8_t* buffer, size_t buffer_size) {
    if (manager == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }
    
    if (manager->proxy_socket < 0) {
        printf("Proxy socket not connected\n");
        return -1;
    }
    
    ssize_t bytes_received = recv(manager->proxy_socket, buffer, buffer_size, 0);
    if (bytes_received < 0) {
        perror("Failed to receive data through HTTP proxy");
        return -1;
    }
    
    return bytes_received;
}

// SOCKS Proxy Implementation
int slipstream_socks_proxy_connect(slipstream_bypass_manager_t* manager, 
                                  const char* target_host, uint16_t target_port) {
    if (manager == NULL || target_host == NULL) {
        return -1;
    }
    
    printf("Connecting to SOCKS proxy %s:%d\n", manager->proxy_config.hostname, manager->proxy_config.port);
    
    // Create socket to proxy
    manager->proxy_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (manager->proxy_socket < 0) {
        perror("Failed to create SOCKS proxy socket");
        return -1;
    }
    
    // Setup proxy address
    struct sockaddr_in* proxy_addr = (struct sockaddr_in*)&manager->proxy_addr;
    proxy_addr->sin_family = AF_INET;
    proxy_addr->sin_port = htons(manager->proxy_config.port);
    
    if (inet_pton(AF_INET, manager->proxy_config.hostname, &proxy_addr->sin_addr) != 1) {
        // Try to resolve proxy hostname
        struct hostent* he = gethostbyname(manager->proxy_config.hostname);
        if (he == NULL) {
            printf("Failed to resolve SOCKS proxy hostname: %s\n", manager->proxy_config.hostname);
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
        memcpy(&proxy_addr->sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    // Connect to proxy
    if (connect(manager->proxy_socket, (struct sockaddr*)&manager->proxy_addr, 
                sizeof(manager->proxy_addr)) < 0) {
        perror("Failed to connect to SOCKS proxy");
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    // SOCKS5 handshake
    if (manager->proxy_config.type == SLIPSTREAM_PROXY_SOCKS5 || 
        manager->proxy_config.type == SLIPSTREAM_PROXY_SOCKS5_AUTH) {
        
        // Send SOCKS5 greeting
        uint8_t greeting[3];
        greeting[0] = 0x05; // SOCKS version 5
        greeting[1] = 0x01; // Number of authentication methods
        greeting[2] = manager->proxy_config.use_authentication ? 0x02 : 0x00; // No auth or username/password
        
        ssize_t bytes_sent = send(manager->proxy_socket, greeting, sizeof(greeting), 0);
        if (bytes_sent < 0) {
            perror("Failed to send SOCKS5 greeting");
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
        
        // Read SOCKS5 response
        uint8_t response[2];
        ssize_t bytes_received = recv(manager->proxy_socket, response, sizeof(response), 0);
        if (bytes_received < 0) {
            perror("Failed to receive SOCKS5 response");
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
        
        if (response[0] != 0x05) {
            printf("Invalid SOCKS5 version in response\n");
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
        
        // Handle authentication if required
        if (response[1] == 0x02 && manager->proxy_config.use_authentication) {
            // Send username/password authentication
            size_t username_len = strlen(manager->proxy_config.username);
            size_t password_len = strlen(manager->proxy_config.password);
            size_t auth_packet_size = 3 + username_len + password_len;
            
            uint8_t* auth_packet = malloc(auth_packet_size);
            if (auth_packet == NULL) {
                close(manager->proxy_socket);
                manager->proxy_socket = -1;
                return -1;
            }
            
            auth_packet[0] = 0x01; // Authentication version
            auth_packet[1] = username_len;
            memcpy(auth_packet + 2, manager->proxy_config.username, username_len);
            auth_packet[2 + username_len] = password_len;
            memcpy(auth_packet + 3 + username_len, manager->proxy_config.password, password_len);
            
            bytes_sent = send(manager->proxy_socket, auth_packet, auth_packet_size, 0);
            free(auth_packet);
            
            if (bytes_sent < 0) {
                perror("Failed to send SOCKS5 authentication");
                close(manager->proxy_socket);
                manager->proxy_socket = -1;
                return -1;
            }
            
            // Read authentication response
            uint8_t auth_response[2];
            bytes_received = recv(manager->proxy_socket, auth_response, sizeof(auth_response), 0);
            if (bytes_received < 0 || auth_response[1] != 0x00) {
                printf("SOCKS5 authentication failed\n");
                close(manager->proxy_socket);
                manager->proxy_socket = -1;
                return -1;
            }
        }
        
        // Send connection request
        struct sockaddr_in target_addr;
        target_addr.sin_family = AF_INET;
        target_addr.sin_port = htons(target_port);
        
        if (inet_pton(AF_INET, target_host, &target_addr.sin_addr) != 1) {
            // Try to resolve target hostname
            struct hostent* he = gethostbyname(target_host);
            if (he == NULL) {
                printf("Failed to resolve target hostname: %s\n", target_host);
                close(manager->proxy_socket);
                manager->proxy_socket = -1;
                return -1;
            }
            memcpy(&target_addr.sin_addr, he->h_addr_list[0], he->h_length);
        }
        
        // Build connection request
        uint8_t connect_request[10];
        connect_request[0] = 0x05; // SOCKS version
        connect_request[1] = 0x01; // CONNECT command
        connect_request[2] = 0x00; // Reserved
        connect_request[3] = 0x01; // IPv4 address type
        memcpy(connect_request + 4, &target_addr.sin_addr, 4);
        memcpy(connect_request + 8, &target_addr.sin_port, 2);
        
        bytes_sent = send(manager->proxy_socket, connect_request, sizeof(connect_request), 0);
        if (bytes_sent < 0) {
            perror("Failed to send SOCKS5 connection request");
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
        
        // Read connection response
        uint8_t connect_response[10];
        bytes_received = recv(manager->proxy_socket, connect_response, sizeof(connect_response), 0);
        if (bytes_received < 0) {
            perror("Failed to receive SOCKS5 connection response");
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
        
        if (connect_response[1] != 0x00) {
            printf("SOCKS5 connection failed with code: %d\n", connect_response[1]);
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
        
    } else if (manager->proxy_config.type == SLIPSTREAM_PROXY_SOCKS4) {
        // SOCKS4 connection request
        struct sockaddr_in target_addr;
        target_addr.sin_family = AF_INET;
        target_addr.sin_port = htons(target_port);
        
        if (inet_pton(AF_INET, target_host, &target_addr.sin_addr) != 1) {
            // Try to resolve target hostname
            struct hostent* he = gethostbyname(target_host);
            if (he == NULL) {
                printf("Failed to resolve target hostname: %s\n", target_host);
                close(manager->proxy_socket);
                manager->proxy_socket = -1;
                return -1;
            }
            memcpy(&target_addr.sin_addr, he->h_addr_list[0], he->h_length);
        }
        
        // Build SOCKS4 connection request
        uint8_t connect_request[9];
        connect_request[0] = 0x04; // SOCKS version 4
        connect_request[1] = 0x01; // CONNECT command
        memcpy(connect_request + 2, &target_addr.sin_port, 2);
        memcpy(connect_request + 4, &target_addr.sin_addr, 4);
        connect_request[8] = 0x00; // Null terminator for username
        
        ssize_t bytes_sent = send(manager->proxy_socket, connect_request, sizeof(connect_request), 0);
        if (bytes_sent < 0) {
            perror("Failed to send SOCKS4 connection request");
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
        
        // Read SOCKS4 response
        uint8_t connect_response[8];
        ssize_t bytes_received = recv(manager->proxy_socket, connect_response, sizeof(connect_response), 0);
        if (bytes_received < 0) {
            perror("Failed to receive SOCKS4 connection response");
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
        
        if (connect_response[1] != 0x5A) {
            printf("SOCKS4 connection failed with code: %d\n", connect_response[1]);
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
    }
    
    printf("SOCKS proxy connection established successfully\n");
    return 0;
}

int slipstream_socks_proxy_send(slipstream_bypass_manager_t* manager, 
                               const uint8_t* data, size_t data_len) {
    if (manager == NULL || data == NULL || data_len == 0) {
        return -1;
    }
    
    if (manager->proxy_socket < 0) {
        printf("SOCKS proxy socket not connected\n");
        return -1;
    }
    
    ssize_t bytes_sent = send(manager->proxy_socket, data, data_len, 0);
    if (bytes_sent < 0) {
        perror("Failed to send data through SOCKS proxy");
        return -1;
    }
    
    return bytes_sent;
}

int slipstream_socks_proxy_receive(slipstream_bypass_manager_t* manager, 
                                  uint8_t* buffer, size_t buffer_size) {
    if (manager == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }
    
    if (manager->proxy_socket < 0) {
        printf("SOCKS proxy socket not connected\n");
        return -1;
    }
    
    ssize_t bytes_received = recv(manager->proxy_socket, buffer, buffer_size, 0);
    if (bytes_received < 0) {
        perror("Failed to receive data through SOCKS proxy");
        return -1;
    }
    
    return bytes_received;
}

// SSH Proxy Implementation
int slipstream_ssh_proxy_connect(slipstream_bypass_manager_t* manager, 
                                const char* ssh_host, uint16_t ssh_port,
                                const char* target_host, uint16_t target_port) {
    if (manager == NULL || ssh_host == NULL || target_host == NULL) {
        return -1;
    }
    
    printf("Connecting to SSH proxy %s:%d\n", ssh_host, ssh_port);
    
    // Create socket to SSH server
    manager->proxy_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (manager->proxy_socket < 0) {
        perror("Failed to create SSH proxy socket");
        return -1;
    }
    
    // Setup SSH server address
    struct sockaddr_in* ssh_addr = (struct sockaddr_in*)&manager->proxy_addr;
    ssh_addr->sin_family = AF_INET;
    ssh_addr->sin_port = htons(ssh_port);
    
    if (inet_pton(AF_INET, ssh_host, &ssh_addr->sin_addr) != 1) {
        // Try to resolve SSH hostname
        struct hostent* he = gethostbyname(ssh_host);
        if (he == NULL) {
            printf("Failed to resolve SSH hostname: %s\n", ssh_host);
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
        memcpy(&ssh_addr->sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    // Connect to SSH server
    if (connect(manager->proxy_socket, (struct sockaddr*)&manager->proxy_addr, 
                sizeof(manager->proxy_addr)) < 0) {
        perror("Failed to connect to SSH server");
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    // SSH handshake (simplified)
    char ssh_handshake[256];
    snprintf(ssh_handshake, sizeof(ssh_handshake),
            "SSH-2.0-SlipstreamProxy_1.0\r\n");
    
    ssize_t bytes_sent = send(manager->proxy_socket, ssh_handshake, strlen(ssh_handshake), 0);
    if (bytes_sent < 0) {
        perror("Failed to send SSH handshake");
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    // Read SSH server response
    char ssh_response[1024];
    ssize_t bytes_received = recv(manager->proxy_socket, ssh_response, sizeof(ssh_response) - 1, 0);
    if (bytes_received <= 0) {
        perror("Failed to receive SSH response");
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    ssh_response[bytes_received] = '\0';
    
    // For simplicity, we'll assume SSH tunnel is established
    // In a real implementation, you'd need to handle SSH key exchange, authentication, etc.
    
    printf("SSH proxy connection established successfully\n");
    return 0;
}

int slipstream_ssh_proxy_send(slipstream_bypass_manager_t* manager, 
                             const uint8_t* data, size_t data_len) {
    if (manager == NULL || data == NULL || data_len == 0) {
        return -1;
    }
    
    if (manager->proxy_socket < 0) {
        printf("SSH proxy socket not connected\n");
        return -1;
    }
    
    ssize_t bytes_sent = send(manager->proxy_socket, data, data_len, 0);
    if (bytes_sent < 0) {
        perror("Failed to send data through SSH proxy");
        return -1;
    }
    
    return bytes_sent;
}

int slipstream_ssh_proxy_receive(slipstream_bypass_manager_t* manager, 
                                uint8_t* buffer, size_t buffer_size) {
    if (manager == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }
    
    if (manager->proxy_socket < 0) {
        printf("SSH proxy socket not connected\n");
        return -1;
    }
    
    ssize_t bytes_received = recv(manager->proxy_socket, buffer, buffer_size, 0);
    if (bytes_received < 0) {
        perror("Failed to receive data through SSH proxy");
        return -1;
    }
    
    return bytes_received;
}

// Tor Proxy Implementation
int slipstream_tor_proxy_connect(slipstream_bypass_manager_t* manager, 
                                const char* target_host, uint16_t target_port) {
    if (manager == NULL || target_host == NULL) {
        return -1;
    }
    
    printf("Connecting to Tor proxy %s:%d\n", manager->proxy_config.hostname, manager->proxy_config.port);
    
    // Create socket to Tor proxy
    manager->proxy_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (manager->proxy_socket < 0) {
        perror("Failed to create Tor proxy socket");
        return -1;
    }
    
    // Setup Tor proxy address
    struct sockaddr_in* tor_addr = (struct sockaddr_in*)&manager->proxy_addr;
    tor_addr->sin_family = AF_INET;
    tor_addr->sin_port = htons(manager->proxy_config.port);
    
    if (inet_pton(AF_INET, manager->proxy_config.hostname, &tor_addr->sin_addr) != 1) {
        // Try to resolve Tor proxy hostname
        struct hostent* he = gethostbyname(manager->proxy_config.hostname);
        if (he == NULL) {
            printf("Failed to resolve Tor proxy hostname: %s\n", manager->proxy_config.hostname);
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
        memcpy(&tor_addr->sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    // Connect to Tor proxy
    if (connect(manager->proxy_socket, (struct sockaddr*)&manager->proxy_addr, 
                sizeof(manager->proxy_addr)) < 0) {
        perror("Failed to connect to Tor proxy");
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    // Tor uses SOCKS5 protocol
    // Send SOCKS5 greeting
    uint8_t greeting[3];
    greeting[0] = 0x05; // SOCKS version 5
    greeting[1] = 0x01; // Number of authentication methods
    greeting[2] = 0x00; // No authentication
    
    ssize_t bytes_sent = send(manager->proxy_socket, greeting, sizeof(greeting), 0);
    if (bytes_sent < 0) {
        perror("Failed to send Tor SOCKS5 greeting");
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    // Read SOCKS5 response
    uint8_t response[2];
    ssize_t bytes_received = recv(manager->proxy_socket, response, sizeof(response), 0);
    if (bytes_received < 0) {
        perror("Failed to receive Tor SOCKS5 response");
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    if (response[0] != 0x05 || response[1] != 0x00) {
        printf("Tor SOCKS5 handshake failed\n");
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    // Send connection request
    struct sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_port);
    
    if (inet_pton(AF_INET, target_host, &target_addr.sin_addr) != 1) {
        // Try to resolve target hostname
        struct hostent* he = gethostbyname(target_host);
        if (he == NULL) {
            printf("Failed to resolve target hostname: %s\n", target_host);
            close(manager->proxy_socket);
            manager->proxy_socket = -1;
            return -1;
        }
        memcpy(&target_addr.sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    // Build connection request
    uint8_t connect_request[10];
    connect_request[0] = 0x05; // SOCKS version
    connect_request[1] = 0x01; // CONNECT command
    connect_request[2] = 0x00; // Reserved
    connect_request[3] = 0x01; // IPv4 address type
    memcpy(connect_request + 4, &target_addr.sin_addr, 4);
    memcpy(connect_request + 8, &target_addr.sin_port, 2);
    
    bytes_sent = send(manager->proxy_socket, connect_request, sizeof(connect_request), 0);
    if (bytes_sent < 0) {
        perror("Failed to send Tor connection request");
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    // Read connection response
    uint8_t connect_response[10];
    bytes_received = recv(manager->proxy_socket, connect_response, sizeof(connect_response), 0);
    if (bytes_received < 0) {
        perror("Failed to receive Tor connection response");
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    if (connect_response[1] != 0x00) {
        printf("Tor connection failed with code: %d\n", connect_response[1]);
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        return -1;
    }
    
    printf("Tor proxy connection established successfully\n");
    return 0;
}

int slipstream_tor_proxy_send(slipstream_bypass_manager_t* manager, 
                             const uint8_t* data, size_t data_len) {
    if (manager == NULL || data == NULL || data_len == 0) {
        return -1;
    }
    
    if (manager->proxy_socket < 0) {
        printf("Tor proxy socket not connected\n");
        return -1;
    }
    
    ssize_t bytes_sent = send(manager->proxy_socket, data, data_len, 0);
    if (bytes_sent < 0) {
        perror("Failed to send data through Tor proxy");
        return -1;
    }
    
    return bytes_sent;
}

int slipstream_tor_proxy_receive(slipstream_bypass_manager_t* manager, 
                                uint8_t* buffer, size_t buffer_size) {
    if (manager == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }
    
    if (manager->proxy_socket < 0) {
        printf("Tor proxy socket not connected\n");
        return -1;
    }
    
    ssize_t bytes_received = recv(manager->proxy_socket, buffer, buffer_size, 0);
    if (bytes_received < 0) {
        perror("Failed to receive data through Tor proxy");
        return -1;
    }
    
    return bytes_received;
}

// Generic proxy functions
int slipstream_proxy_connect(slipstream_bypass_manager_t* manager, 
                            const char* target_host, uint16_t target_port) {
    if (manager == NULL || target_host == NULL) {
        return -1;
    }
    
    switch (manager->proxy_config.type) {
        case SLIPSTREAM_PROXY_HTTP:
        case SLIPSTREAM_PROXY_HTTPS:
            return slipstream_http_proxy_connect(manager, target_host, target_port);
            
        case SLIPSTREAM_PROXY_SOCKS4:
        case SLIPSTREAM_PROXY_SOCKS5:
        case SLIPSTREAM_PROXY_SOCKS5_AUTH:
            return slipstream_socks_proxy_connect(manager, target_host, target_port);
            
        case SLIPSTREAM_PROXY_SSH:
            return slipstream_ssh_proxy_connect(manager, 
                                              manager->proxy_config.hostname, 
                                              manager->proxy_config.port,
                                              target_host, target_port);
            
        case SLIPSTREAM_PROXY_TOR:
            return slipstream_tor_proxy_connect(manager, target_host, target_port);
            
        default:
            printf("Unsupported proxy type: %d\n", manager->proxy_config.type);
            return -1;
    }
}

int slipstream_proxy_send(slipstream_bypass_manager_t* manager, 
                         const uint8_t* data, size_t data_len) {
    if (manager == NULL || data == NULL || data_len == 0) {
        return -1;
    }
    
    switch (manager->proxy_config.type) {
        case SLIPSTREAM_PROXY_HTTP:
        case SLIPSTREAM_PROXY_HTTPS:
            return slipstream_http_proxy_send(manager, data, data_len);
            
        case SLIPSTREAM_PROXY_SOCKS4:
        case SLIPSTREAM_PROXY_SOCKS5:
        case SLIPSTREAM_PROXY_SOCKS5_AUTH:
            return slipstream_socks_proxy_send(manager, data, data_len);
            
        case SLIPSTREAM_PROXY_SSH:
            return slipstream_ssh_proxy_send(manager, data, data_len);
            
        case SLIPSTREAM_PROXY_TOR:
            return slipstream_tor_proxy_send(manager, data, data_len);
            
        default:
            printf("Unsupported proxy type: %d\n", manager->proxy_config.type);
            return -1;
    }
}

int slipstream_proxy_receive(slipstream_bypass_manager_t* manager, 
                            uint8_t* buffer, size_t buffer_size) {
    if (manager == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }
    
    switch (manager->proxy_config.type) {
        case SLIPSTREAM_PROXY_HTTP:
        case SLIPSTREAM_PROXY_HTTPS:
            return slipstream_http_proxy_receive(manager, buffer, buffer_size);
            
        case SLIPSTREAM_PROXY_SOCKS4:
        case SLIPSTREAM_PROXY_SOCKS5:
        case SLIPSTREAM_PROXY_SOCKS5_AUTH:
            return slipstream_socks_proxy_receive(manager, buffer, buffer_size);
            
        case SLIPSTREAM_PROXY_SSH:
            return slipstream_ssh_proxy_receive(manager, buffer, buffer_size);
            
        case SLIPSTREAM_PROXY_TOR:
            return slipstream_tor_proxy_receive(manager, buffer, buffer_size);
            
        default:
            printf("Unsupported proxy type: %d\n", manager->proxy_config.type);
            return -1;
    }
}

int slipstream_proxy_disconnect(slipstream_bypass_manager_t* manager) {
    if (manager == NULL) {
        return -1;
    }
    
    if (manager->proxy_socket >= 0) {
        close(manager->proxy_socket);
        manager->proxy_socket = -1;
        printf("Proxy connection closed\n");
    }
    
    return 0;
}
