#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <pthread.h>
#include <bluetooth/rfcomm.h>

#include "blech.h"
// TODO: this should be sorted to allow for binary search
// for easiest shortest path node calculation
struct peer_list{
      struct loc_addr_clnt_num* l_a;
      int cap;
      int sz, local_sock;
      _Bool continuous, lock;
};

void pl_init(struct peer_list* pl);
void pl_add(struct peer_list* pl, struct sockaddr_rc la, int clnt_num, char* name, char* mac);
void pl_print(struct peer_list* pl);
