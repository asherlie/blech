#ifndef _NET_H
#define _NET_H
#include "peer_list.h"

int net_connect(char* host, int* sock, uint16_t port_num);
void accept_connections(struct peer_list* pl);
#endif
