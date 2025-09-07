#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include "slipstream_bypass.h"
#include "slipstream_protocols.h"

// Global variables for cleanup
static volatile bool running = true;
static slipstream_bypass_manager_t* global_manager = NULL;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down gracefully...\n", sig);
    running = false;
    if (global_manager) {
        slipstream_bypass_manager_cleanup(global_manager);
    }
    exit(0);
}

// UDP Network Scanner
int scan_udp_ports(const char* target_host, uint16_t start_port, uint16_t end_port) {
    printf("Scanning UDP ports %d-%d on %s...\n", start_port, end_port, target_host);
    
    int open_ports = 0;
    struct sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(53); // Default to DNS port for testing
    
    if (inet_pton(AF_INET, target_host, &target_addr.sin_addr) != 1) {
        printf("Invalid target host: %s\n", target_host);
        return -1;
    }
    
    for (uint16_t port = start_port; port <= end_port && running; port++) {
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock < 0) {
            continue;
        }
        
        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        
        target_addr.sin_port = htons(port);
        
        // Send test packet
        const char* test_data = "UDP_PORT_TEST";
        ssize_t sent = sendto(sock, test_data, strlen(test_data), 0, 
                             (struct sockaddr*)&target_addr, sizeof(target_addr));
        
        if (sent > 0) {
            // Try to receive response
            char response[1024];
            struct sockaddr_in response_addr;
            socklen_t response_len = sizeof(response_addr);
            
            ssize_t received = recvfrom(sock, response, sizeof(response) - 1, 0,
                                       (struct sockaddr*)&response_addr, &response_len);
            
            if (received > 0) {
                response[received] = '\0';
                printf("Port %d: OPEN - Response: %s\n", port, response);
                open_ports++;
            } else {
                // Port might be open but not responding (common for UDP)
                printf("Port %d: OPEN (no response)\n", port);
                open_ports++;
            }
        }
        
        close(sock);
        
        // Small delay to avoid overwhelming the network
        usleep(1000); // 1ms delay
    }
    
    printf("Scan completed. Found %d open UDP ports.\n", open_ports);
    return open_ports;
}

// UDP Traffic Generator with Bypass Techniques
int generate_udp_traffic_with_bypass(slipstream_bypass_manager_t* manager, 
                                    const char* target_host, uint16_t target_port,
                                    const char* data, size_t data_len) {
    if (manager == NULL || target_host == NULL || data == NULL) {
        return -1;
    }
    
    printf("Generating UDP traffic with bypass techniques to %s:%d\n", target_host, target_port);
    
    // Setup target address
    struct sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_port);
    
    if (inet_pton(AF_INET, target_host, &target_addr.sin_addr) != 1) {
        printf("Invalid target host: %s\n", target_host);
        return -1;
    }
    
    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        perror("Failed to create UDP socket");
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Apply different bypass techniques
    printf("Applying bypass techniques...\n");
    
    // 1. Fragmentation bypass
    printf("1. Applying fragmentation bypass...\n");
    slipstream_bypass_fragmentation(manager, (const uint8_t*)data, data_len);
    
    // 2. Steganography bypass
    printf("2. Applying steganography bypass...\n");
    slipstream_bypass_steganography(manager, (const uint8_t*)data, data_len);
    
    // 3. Protocol mimicry (DNS)
    printf("3. Applying DNS protocol mimicry...\n");
    slipstream_bypass_protocol_mimicry(manager, "dns", (const uint8_t*)data, data_len);
    
    // 4. DPI evasion
    printf("4. Applying DPI evasion...\n");
    slipstream_bypass_dpi_evasion(manager, (const uint8_t*)data, data_len);
    
    // 5. Traffic obfuscation
    printf("5. Applying traffic obfuscation...\n");
    slipstream_bypass_traffic_obfuscation(manager, (const uint8_t*)data, data_len);
    
    // 6. Timing attack evasion
    printf("6. Applying timing attack evasion...\n");
    slipstream_bypass_timing_attack(manager, (const uint8_t*)data, data_len);
    
    // 7. Flow watermarking
    printf("7. Applying flow watermarking...\n");
    slipstream_bypass_flow_watermarking(manager, (const uint8_t*)data, data_len);
    
    close(sock);
    printf("UDP traffic generation with bypass techniques completed.\n");
    return 0;
}

// Port Hopping with UDP
int udp_port_hopping(slipstream_bypass_manager_t* manager, 
                     const char* target_host, uint16_t start_port, uint16_t end_port,
                     const char* data, size_t data_len) {
    if (manager == NULL || target_host == NULL || data == NULL) {
        return -1;
    }
    
    printf("Starting UDP port hopping from %d to %d on %s\n", start_port, end_port, target_host);
    
    // Setup target address
    struct sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    
    if (inet_pton(AF_INET, target_host, &target_addr.sin_addr) != 1) {
        printf("Invalid target host: %s\n", target_host);
        return -1;
    }
    
    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        perror("Failed to create UDP socket");
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    srand(time(NULL));
    int packets_sent = 0;
    
    while (running && packets_sent < 100) { // Limit to 100 packets for demo
        // Generate random port in range
        uint16_t random_port = start_port + (rand() % (end_port - start_port + 1));
        target_addr.sin_port = htons(random_port);
        
        // Send data to random port
        ssize_t sent = sendto(sock, data, data_len, 0, 
                             (struct sockaddr*)&target_addr, sizeof(target_addr));
        
        if (sent > 0) {
            printf("Sent %ld bytes to port %d\n", sent, random_port);
            packets_sent++;
        }
        
        // Random delay between packets
        usleep(rand() % 100000); // 0-100ms delay
    }
    
    close(sock);
    printf("UDP port hopping completed. Sent %d packets.\n", packets_sent);
    return packets_sent;
}

// UDP Tunnel with Multiple Techniques
int udp_tunnel_with_techniques(slipstream_bypass_manager_t* manager, 
                              const char* target_host, uint16_t target_port) {
    if (manager == NULL || target_host == NULL) {
        return -1;
    }
    
    printf("Setting up UDP tunnel with multiple bypass techniques to %s:%d\n", 
           target_host, target_port);
    
    // Setup bypass configuration
    slipstream_bypass_config_t bypass_config = {
        .technique = SLIPSTREAM_BYPASS_UDP,
        .enabled = true,
        .use_fragmentation = true,
        .use_steganography = true,
        .use_protocol_mimicry = true,
        .use_port_hopping = true,
        .port_range_start = 1,
        .port_range_end = 65535
    };
    
    // Initialize bypass manager
    if (slipstream_bypass_manager_init(manager, &bypass_config, NULL) != 0) {
        printf("Failed to initialize bypass manager\n");
        return -1;
    }
    
    // Setup UDP tunnel
    if (slipstream_bypass_dns_tunnel(manager, target_host, target_port) != 0) {
        printf("Failed to setup UDP tunnel\n");
        return -1;
    }
    
    printf("UDP tunnel with bypass techniques established successfully.\n");
    return 0;
}

// Main function
int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <target_host> <target_port> [start_port] [end_port]\n", argv[0]);
        printf("Examples:\n");
        printf("  %s 8.8.8.8 53\n", argv[0]);
        printf("  %s 127.0.0.1 53 1 65535\n", argv[0]);
        printf("  %s example.com 80 1000 2000\n", argv[0]);
        return 1;
    }
    
    const char* target_host = argv[1];
    uint16_t target_port = atoi(argv[2]);
    uint16_t start_port = 1;
    uint16_t end_port = 65535;
    
    if (argc >= 5) {
        start_port = atoi(argv[3]);
        end_port = atoi(argv[4]);
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("=== Slipstream UDP Network Bypass Demo ===\n");
    printf("Target: %s:%d\n", target_host, target_port);
    printf("Port Range: %d-%d\n", start_port, end_port);
    printf("Press Ctrl+C to stop\n\n");
    
    // Initialize bypass manager
    slipstream_bypass_manager_t manager;
    global_manager = &manager;
    
    // 1. Scan UDP ports
    printf("=== Step 1: UDP Port Scanning ===\n");
    int open_ports = scan_udp_ports(target_host, start_port, end_port);
    if (open_ports < 0) {
        printf("Port scanning failed\n");
        return 1;
    }
    
    // 2. Setup UDP tunnel with bypass techniques
    printf("\n=== Step 2: UDP Tunnel Setup ===\n");
    if (udp_tunnel_with_techniques(&manager, target_host, target_port) != 0) {
        printf("Failed to setup UDP tunnel\n");
        return 1;
    }
    
    // 3. Generate UDP traffic with bypass techniques
    printf("\n=== Step 3: UDP Traffic Generation ===\n");
    const char* test_data = "This is a test message for UDP bypass techniques";
    size_t data_len = strlen(test_data);
    
    if (generate_udp_traffic_with_bypass(&manager, target_host, target_port, 
                                        test_data, data_len) != 0) {
        printf("Failed to generate UDP traffic\n");
        return 1;
    }
    
    // 4. UDP Port Hopping
    printf("\n=== Step 4: UDP Port Hopping ===\n");
    if (udp_port_hopping(&manager, target_host, start_port, end_port, 
                        test_data, data_len) != 0) {
        printf("Failed to perform UDP port hopping\n");
        return 1;
    }
    
    // 5. Continuous UDP traffic with different techniques
    printf("\n=== Step 5: Continuous UDP Traffic ===\n");
    printf("Sending continuous UDP traffic with bypass techniques...\n");
    
    int packet_count = 0;
    while (running && packet_count < 50) { // Limit to 50 packets for demo
        // Alternate between different bypass techniques
        switch (packet_count % 7) {
            case 0:
                slipstream_bypass_fragmentation(&manager, (const uint8_t*)test_data, data_len);
                printf("Packet %d: Fragmentation bypass\n", packet_count + 1);
                break;
            case 1:
                slipstream_bypass_steganography(&manager, (const uint8_t*)test_data, data_len);
                printf("Packet %d: Steganography bypass\n", packet_count + 1);
                break;
            case 2:
                slipstream_bypass_protocol_mimicry(&manager, "dns", (const uint8_t*)test_data, data_len);
                printf("Packet %d: DNS mimicry bypass\n", packet_count + 1);
                break;
            case 3:
                slipstream_bypass_dpi_evasion(&manager, (const uint8_t*)test_data, data_len);
                printf("Packet %d: DPI evasion\n", packet_count + 1);
                break;
            case 4:
                slipstream_bypass_traffic_obfuscation(&manager, (const uint8_t*)test_data, data_len);
                printf("Packet %d: Traffic obfuscation\n", packet_count + 1);
                break;
            case 5:
                slipstream_bypass_timing_attack(&manager, (const uint8_t*)test_data, data_len);
                printf("Packet %d: Timing attack evasion\n", packet_count + 1);
                break;
            case 6:
                slipstream_bypass_flow_watermarking(&manager, (const uint8_t*)test_data, data_len);
                printf("Packet %d: Flow watermarking\n", packet_count + 1);
                break;
        }
        
        packet_count++;
        usleep(500000); // 500ms delay between packets
    }
    
    // Cleanup
    printf("\n=== Cleanup ===\n");
    slipstream_bypass_manager_cleanup(&manager);
    
    printf("UDP network bypass demo completed successfully!\n");
    printf("Total packets sent: %d\n", packet_count);
    printf("Open ports found: %d\n", open_ports);
    
    return 0;
}
