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

#define ANSI_RED "\x1B[31m"
#define ANSI_NON "\x1b[0m"
#define ANSI_GRE "\x1b[32m"
#define ANSI_BLU "\x1b[34m"


struct glob_peer_list_entry{
      // [client name, mac]
      char** clnt_info;
      // peer_list->l_a[route[0]] is first step of path
      // route[n-1] will have the clnt_info in their peer_list
      // NOTE: each digit of route is relative to the peer list of the digit before it
      // dir_p is next peer in chain to dest peer
      int dir_p;
      int* route;
      int route_s, route_c;
};

struct glob_peer_list{
      int sz, cap;
      struct glob_peer_list_entry* gpl;
};

// TODO: this should be sorted to allow for binary search
// for easiest shortest path node calculation
struct peer_list{
      struct glob_peer_list* gpl;
      struct loc_addr_clnt_num* l_a;
      int sz, cap, local_sock;
      char* local_mac;
      _Bool continuous;
};

void gpl_init(struct glob_peer_list* gpl);
void pl_init(struct peer_list* pl);
void pl_add(struct peer_list* pl, struct sockaddr_rc la, int clnt_num, char* name, char* mac);
void pl_print(struct peer_list* pl);
void gple_add_route_entry(struct glob_peer_list_entry* gple, int rel_no);
void gpl_init(struct glob_peer_list* gpl);
struct glob_peer_list_entry* gpl_add(struct glob_peer_list* gpl, char* name, char* mac);
int has_peer(struct peer_list* pl, char* mac);
int next_in_line(struct peer_list* pl, char* mac);
