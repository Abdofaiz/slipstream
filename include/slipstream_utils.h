#ifndef SLIPSTREAM_UTILS_H
#define SLIPSTREAM_UTILS_H

#include "picoquic.h"

char* picoquic_connection_id_to_string(const picoquic_connection_id_t* cid);

void sockaddr_dummy(struct sockaddr_storage *addr_storage);

#endif //SLIPSTREAM_UTILS_H
