#include "slipstream_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char* picoquic_connection_id_to_string(const picoquic_connection_id_t* cid) {
    // Each byte needs 2 hex characters + null terminator
    char* str = malloc((cid->id_len * 2 + 1) * sizeof(char));
    if (str == NULL) {
        return NULL;
    }

    // Convert each byte to hex
    for (int i = 0; i < cid->id_len; i++) {
        sprintf(&str[i * 2], "%02x", cid->id[i]);
    }
    str[cid->id_len * 2] = '\0';

    return str;
}

// Function to create a dummy sockaddr_storage with hardcoded IPv4 and port
void sockaddr_dummy(struct sockaddr_storage *addr_storage) {
    // Clear the entire sockaddr_storage to avoid residual data
    memset(addr_storage, 0, sizeof(struct sockaddr_storage));

    // Cast sockaddr_storage to sockaddr_in for IPv4
    struct sockaddr_in *addr4 = (struct sockaddr_in *)addr_storage;

    // Set address family to AF_INET (IPv4)
    addr4->sin_family = AF_INET;

    // Use a hardcoded IPv4 address: 192.0.2.1 (TEST-NET-1 for testing)
    inet_pton(AF_INET, "192.0.2.1", &addr4->sin_addr);

    // Set a hardcoded port: 12345
    addr4->sin_port = htons(12345);

#ifdef __APPLE__ // For BSD systems, set sin_len
    addr4->sin_len = sizeof(struct sockaddr_in);
#endif
}
