#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "slipstream_bypass.h"

// Example usage of firewall bypass and proxy support
int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Usage: %s <bypass_technique> <target_host> <target_port> [proxy_type] [proxy_host] [proxy_port]\n", argv[0]);
        printf("Bypass techniques: dns, http, https, icmp, fragmentation, steganography, mimicry, port_hopping, domain_fronting, cdn_bypass\n");
        printf("Proxy types: http, socks4, socks5, ssh, tor\n");
        printf("Examples:\n");
        printf("  %s dns example.com 53\n", argv[0]);
        printf("  %s http example.com 80 http proxy.example.com 8080\n", argv[0]);
        printf("  %s https example.com 443 socks5 tor-proxy.example.com 9050\n", argv[0]);
        return 1;
    }
    
    const char* bypass_technique = argv[1];
    const char* target_host = argv[2];
    uint16_t target_port = atoi(argv[3]);
    
    // Parse proxy configuration if provided
    slipstream_proxy_config_t proxy_config = {0};
    if (argc >= 6) {
        const char* proxy_type = argv[4];
        const char* proxy_host = argv[5];
        uint16_t proxy_port = atoi(argv[6]);
        
        if (strcmp(proxy_type, "http") == 0) {
            proxy_config.type = SLIPSTREAM_PROXY_HTTP;
        } else if (strcmp(proxy_type, "socks4") == 0) {
            proxy_config.type = SLIPSTREAM_PROXY_SOCKS4;
        } else if (strcmp(proxy_type, "socks5") == 0) {
            proxy_config.type = SLIPSTREAM_PROXY_SOCKS5;
        } else if (strcmp(proxy_type, "ssh") == 0) {
            proxy_config.type = SLIPSTREAM_PROXY_SSH;
        } else if (strcmp(proxy_type, "tor") == 0) {
            proxy_config.type = SLIPSTREAM_PROXY_TOR;
        } else {
            printf("Unknown proxy type: %s\n", proxy_type);
            return 1;
        }
        
        strncpy(proxy_config.hostname, proxy_host, sizeof(proxy_config.hostname) - 1);
        proxy_config.port = proxy_port;
        proxy_config.use_authentication = false;
        
        printf("Proxy configuration: %s %s:%d\n", 
               slipstream_proxy_type_name(proxy_config.type),
               proxy_config.hostname, proxy_config.port);
    }
    
    // Parse bypass technique
    slipstream_bypass_config_t bypass_config = {0};
    if (strcmp(bypass_technique, "dns") == 0) {
        bypass_config.technique = SLIPSTREAM_BYPASS_DNS_TUNNEL;
    } else if (strcmp(bypass_technique, "http") == 0) {
        bypass_config.technique = SLIPSTREAM_BYPASS_HTTP_TUNNEL;
    } else if (strcmp(bypass_technique, "https") == 0) {
        bypass_config.technique = SLIPSTREAM_BYPASS_HTTPS_TUNNEL;
    } else if (strcmp(bypass_technique, "icmp") == 0) {
        bypass_config.technique = SLIPSTREAM_BYPASS_ICMP_TUNNEL;
    } else if (strcmp(bypass_technique, "fragmentation") == 0) {
        bypass_config.technique = SLIPSTREAM_BYPASS_FRAGMENTATION;
    } else if (strcmp(bypass_technique, "steganography") == 0) {
        bypass_config.technique = SLIPSTREAM_BYPASS_STEGANOGRAPHY;
    } else if (strcmp(bypass_technique, "mimicry") == 0) {
        bypass_config.technique = SLIPSTREAM_BYPASS_PROTOCOL_MIMICRY;
    } else if (strcmp(bypass_technique, "port_hopping") == 0) {
        bypass_config.technique = SLIPSTREAM_BYPASS_PORT_HOPPING;
    } else if (strcmp(bypass_technique, "domain_fronting") == 0) {
        bypass_config.technique = SLIPSTREAM_BYPASS_DOMAIN_FRONTING;
    } else if (strcmp(bypass_technique, "cdn_bypass") == 0) {
        bypass_config.technique = SLIPSTREAM_BYPASS_CDN_BYPASS;
    } else {
        printf("Unknown bypass technique: %s\n", bypass_technique);
        return 1;
    }
    
    bypass_config.enabled = true;
    strncpy(bypass_config.custom_domain, target_host, sizeof(bypass_config.custom_domain) - 1);
    bypass_config.custom_port = target_port;
    
    printf("Bypass configuration: %s to %s:%d\n", 
           slipstream_bypass_technique_name(bypass_config.technique),
           target_host, target_port);
    
    // Initialize bypass manager
    slipstream_bypass_manager_t manager;
    if (slipstream_bypass_manager_init(&manager, &bypass_config, &proxy_config) != 0) {
        printf("Failed to initialize bypass manager\n");
        return 1;
    }
    
    // Setup bypass technique
    int ret = 0;
    switch (bypass_config.technique) {
        case SLIPSTREAM_BYPASS_DNS_TUNNEL:
            ret = slipstream_bypass_dns_tunnel(&manager, target_host, target_port);
            break;
        case SLIPSTREAM_BYPASS_HTTP_TUNNEL:
            ret = slipstream_bypass_http_tunnel(&manager, target_host, target_port);
            break;
        case SLIPSTREAM_BYPASS_HTTPS_TUNNEL:
            ret = slipstream_bypass_https_tunnel(&manager, target_host, target_port);
            break;
        case SLIPSTREAM_BYPASS_ICMP_TUNNEL:
            ret = slipstream_bypass_icmp_tunnel(&manager);
            break;
        case SLIPSTREAM_BYPASS_FRAGMENTATION:
            ret = slipstream_bypass_fragmentation(&manager, (const uint8_t*)"test data", 9);
            break;
        case SLIPSTREAM_BYPASS_STEGANOGRAPHY:
            ret = slipstream_bypass_steganography(&manager, (const uint8_t*)"hidden data", 11);
            break;
        case SLIPSTREAM_BYPASS_PROTOCOL_MIMICRY:
            ret = slipstream_bypass_protocol_mimicry(&manager, "http", (const uint8_t*)"mimic data", 10);
            break;
        case SLIPSTREAM_BYPASS_PORT_HOPPING:
            ret = slipstream_bypass_port_hopping(&manager, 8000, 9000);
            break;
        case SLIPSTREAM_BYPASS_DOMAIN_FRONTING:
            ret = slipstream_bypass_domain_fronting(&manager, "cdn.example.com", target_host);
            break;
        case SLIPSTREAM_BYPASS_CDN_BYPASS:
            ret = slipstream_bypass_cdn_bypass(&manager, "cloudflare", target_host);
            break;
        default:
            printf("Unsupported bypass technique\n");
            ret = -1;
            break;
    }
    
    if (ret != 0) {
        printf("Failed to setup bypass technique\n");
        slipstream_bypass_manager_cleanup(&manager);
        return 1;
    }
    
    // Test proxy connection if configured
    if (proxy_config.type != SLIPSTREAM_PROXY_NONE) {
        printf("Testing proxy connection...\n");
        ret = slipstream_proxy_connect(&manager, target_host, target_port);
        if (ret != 0) {
            printf("Failed to connect through proxy\n");
            slipstream_bypass_manager_cleanup(&manager);
            return 1;
        }
        
        // Test data transfer through proxy
        const char* test_data = "Hello, Proxy World!";
        size_t data_len = strlen(test_data);
        
        printf("Sending test data through proxy: %s\n", test_data);
        ret = slipstream_proxy_send(&manager, (const uint8_t*)test_data, data_len);
        if (ret < 0) {
            printf("Failed to send data through proxy\n");
        } else {
            printf("Sent %d bytes through proxy\n", ret);
        }
        
        // Try to receive response
        uint8_t response_buffer[1024];
        ret = slipstream_proxy_receive(&manager, response_buffer, sizeof(response_buffer) - 1);
        if (ret > 0) {
            response_buffer[ret] = '\0';
            printf("Received response through proxy: %s\n", (char*)response_buffer);
        } else if (ret < 0) {
            printf("Failed to receive response through proxy\n");
        }
        
        // Disconnect from proxy
        slipstream_proxy_disconnect(&manager);
    }
    
    // Test advanced bypass techniques
    printf("\nTesting advanced bypass techniques...\n");
    
    const char* test_data = "Advanced bypass test data";
    size_t data_len = strlen(test_data);
    
    // Test DPI evasion
    printf("Testing DPI evasion...\n");
    ret = slipstream_bypass_dpi_evasion(&manager, (const uint8_t*)test_data, data_len);
    if (ret == 0) {
        printf("DPI evasion test completed successfully\n");
    } else {
        printf("DPI evasion test failed\n");
    }
    
    // Test traffic obfuscation
    printf("Testing traffic obfuscation...\n");
    ret = slipstream_bypass_traffic_obfuscation(&manager, (const uint8_t*)test_data, data_len);
    if (ret == 0) {
        printf("Traffic obfuscation test completed successfully\n");
    } else {
        printf("Traffic obfuscation test failed\n");
    }
    
    // Test timing attack evasion
    printf("Testing timing attack evasion...\n");
    ret = slipstream_bypass_timing_attack(&manager, (const uint8_t*)test_data, data_len);
    if (ret == 0) {
        printf("Timing attack evasion test completed successfully\n");
    } else {
        printf("Timing attack evasion test failed\n");
    }
    
    // Test flow watermarking
    printf("Testing flow watermarking...\n");
    ret = slipstream_bypass_flow_watermarking(&manager, (const uint8_t*)test_data, data_len);
    if (ret == 0) {
        printf("Flow watermarking test completed successfully\n");
    } else {
        printf("Flow watermarking test failed\n");
    }
    
    // Cleanup
    slipstream_bypass_manager_cleanup(&manager);
    
    printf("\nBypass and proxy test completed successfully!\n");
    return 0;
}
