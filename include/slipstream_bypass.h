#ifndef SLIPSTREAM_BYPASS_H
#define SLIPSTREAM_BYPASS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Firewall bypass techniques
typedef enum {
    SLIPSTREAM_BYPASS_NONE = 0,
    SLIPSTREAM_BYPASS_DNS_TUNNEL = 1,
    SLIPSTREAM_BYPASS_HTTP_TUNNEL = 2,
    SLIPSTREAM_BYPASS_HTTPS_TUNNEL = 3,
    SLIPSTREAM_BYPASS_ICMP_TUNNEL = 4,
    SLIPSTREAM_BYPASS_FRAGMENTATION = 5,
    SLIPSTREAM_BYPASS_STEGANOGRAPHY = 6,
    SLIPSTREAM_BYPASS_PROTOCOL_MIMICRY = 7,
    SLIPSTREAM_BYPASS_PORT_HOPPING = 8,
    SLIPSTREAM_BYPASS_DOMAIN_FRONTING = 9,
    SLIPSTREAM_BYPASS_CDN_BYPASS = 10
} slipstream_bypass_technique_t;

// Proxy types
typedef enum {
    SLIPSTREAM_PROXY_NONE = 0,
    SLIPSTREAM_PROXY_HTTP = 1,
    SLIPSTREAM_PROXY_HTTPS = 2,
    SLIPSTREAM_PROXY_SOCKS4 = 3,
    SLIPSTREAM_PROXY_SOCKS5 = 4,
    SLIPSTREAM_PROXY_SOCKS5_AUTH = 5,
    SLIPSTREAM_PROXY_SSH = 6,
    SLIPSTREAM_PROXY_TOR = 7
} slipstream_proxy_type_t;

// Bypass configuration
typedef struct {
    slipstream_bypass_technique_t technique;
    bool enabled;
    char custom_domain[256];
    uint16_t custom_port;
    bool use_fragmentation;
    bool use_steganography;
    bool use_protocol_mimicry;
    char mimic_protocol[32];
    bool use_port_hopping;
    uint16_t port_range_start;
    uint16_t port_range_end;
    bool use_domain_fronting;
    char fronting_domain[256];
    bool use_cdn_bypass;
    char cdn_provider[64];
} slipstream_bypass_config_t;

// Proxy configuration
typedef struct {
    slipstream_proxy_type_t type;
    char hostname[256];
    uint16_t port;
    char username[128];
    char password[128];
    bool use_authentication;
    bool use_encryption;
    char ca_cert_path[512];
    char client_cert_path[512];
    char client_key_path[512];
} slipstream_proxy_config_t;

// Bypass manager
typedef struct {
    slipstream_bypass_config_t bypass_config;
    slipstream_proxy_config_t proxy_config;
    bool initialized;
    int bypass_socket;
    int proxy_socket;
    struct sockaddr_storage target_addr;
    struct sockaddr_storage proxy_addr;
} slipstream_bypass_manager_t;

// Function declarations
int slipstream_bypass_manager_init(slipstream_bypass_manager_t* manager, 
                                  const slipstream_bypass_config_t* bypass_config,
                                  const slipstream_proxy_config_t* proxy_config);
void slipstream_bypass_manager_cleanup(slipstream_bypass_manager_t* manager);

// Bypass techniques
int slipstream_bypass_dns_tunnel(slipstream_bypass_manager_t* manager, 
                                const char* domain, uint16_t port);
int slipstream_bypass_http_tunnel(slipstream_bypass_manager_t* manager, 
                                 const char* hostname, uint16_t port);
int slipstream_bypass_https_tunnel(slipstream_bypass_manager_t* manager, 
                                  const char* hostname, uint16_t port);
int slipstream_bypass_icmp_tunnel(slipstream_bypass_manager_t* manager);
int slipstream_bypass_fragmentation(slipstream_bypass_manager_t* manager, 
                                   const uint8_t* data, size_t data_len);
int slipstream_bypass_steganography(slipstream_bypass_manager_t* manager, 
                                   const uint8_t* data, size_t data_len);
int slipstream_bypass_protocol_mimicry(slipstream_bypass_manager_t* manager, 
                                      const char* protocol, const uint8_t* data, size_t data_len);
int slipstream_bypass_port_hopping(slipstream_bypass_manager_t* manager, 
                                  uint16_t start_port, uint16_t end_port);
int slipstream_bypass_domain_fronting(slipstream_bypass_manager_t* manager, 
                                     const char* fronting_domain, const char* real_domain);
int slipstream_bypass_cdn_bypass(slipstream_bypass_manager_t* manager, 
                                const char* cdn_provider, const char* target_domain);

// Proxy support
int slipstream_proxy_connect(slipstream_bypass_manager_t* manager, 
                            const char* target_host, uint16_t target_port);
int slipstream_proxy_send(slipstream_bypass_manager_t* manager, 
                         const uint8_t* data, size_t data_len);
int slipstream_proxy_receive(slipstream_bypass_manager_t* manager, 
                            uint8_t* buffer, size_t buffer_size);
int slipstream_proxy_disconnect(slipstream_bypass_manager_t* manager);

// HTTP Proxy
int slipstream_http_proxy_connect(slipstream_bypass_manager_t* manager, 
                                 const char* target_host, uint16_t target_port);
int slipstream_http_proxy_send(slipstream_bypass_manager_t* manager, 
                              const uint8_t* data, size_t data_len);
int slipstream_http_proxy_receive(slipstream_bypass_manager_t* manager, 
                                 uint8_t* buffer, size_t buffer_size);

// SOCKS Proxy
int slipstream_socks_proxy_connect(slipstream_bypass_manager_t* manager, 
                                  const char* target_host, uint16_t target_port);
int slipstream_socks_proxy_send(slipstream_bypass_manager_t* manager, 
                               const uint8_t* data, size_t data_len);
int slipstream_socks_proxy_receive(slipstream_bypass_manager_t* manager, 
                                  uint8_t* buffer, size_t buffer_size);

// SSH Proxy (SSH Tunnel)
int slipstream_ssh_proxy_connect(slipstream_bypass_manager_t* manager, 
                                const char* ssh_host, uint16_t ssh_port,
                                const char* target_host, uint16_t target_port);
int slipstream_ssh_proxy_send(slipstream_bypass_manager_t* manager, 
                             const uint8_t* data, size_t data_len);
int slipstream_ssh_proxy_receive(slipstream_bypass_manager_t* manager, 
                                uint8_t* buffer, size_t buffer_size);

// Tor Proxy
int slipstream_tor_proxy_connect(slipstream_bypass_manager_t* manager, 
                                const char* target_host, uint16_t target_port);
int slipstream_tor_proxy_send(slipstream_bypass_manager_t* manager, 
                             const uint8_t* data, size_t data_len);
int slipstream_tor_proxy_receive(slipstream_bypass_manager_t* manager, 
                                uint8_t* buffer, size_t buffer_size);

// Utility functions
const char* slipstream_bypass_technique_name(slipstream_bypass_technique_t technique);
const char* slipstream_proxy_type_name(slipstream_proxy_type_t type);
bool slipstream_bypass_technique_available(slipstream_bypass_technique_t technique);
bool slipstream_proxy_type_supported(slipstream_proxy_type_t type);

// Advanced bypass techniques
int slipstream_bypass_dpi_evasion(slipstream_bypass_manager_t* manager, 
                                 const uint8_t* data, size_t data_len);
int slipstream_bypass_traffic_obfuscation(slipstream_bypass_manager_t* manager, 
                                         const uint8_t* data, size_t data_len);
int slipstream_bypass_timing_attack(slipstream_bypass_manager_t* manager, 
                                   const uint8_t* data, size_t data_len);
int slipstream_bypass_flow_watermarking(slipstream_bypass_manager_t* manager, 
                                       const uint8_t* data, size_t data_len);

#ifdef __cplusplus
}
#endif

#endif // SLIPSTREAM_BYPASS_H
