#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

#include "blech.h"

#define ANSI_RED   "\x1B[31m"
#define ANSI_NON   "\x1b[0m"
#define ANSI_GRE   "\x1b[32m"
#define ANSI_BLU   "\x1b[34m"
#define ANSI_MGNTA "\x1b[35m"

struct glob_peer_list_entry{
      // [client name, mac]
      char** clnt_info;
      // dir_p is next peer in chain to dest peer
      int* dir_p;
      int dir_p_cap, n_dir_p;
};

struct glob_peer_list{
      int sz, cap;
      struct glob_peer_list_entry* gpl;
};

struct read_thread{
      int sz, cap;
      pthread_t* th;
};

struct peer_list{
      pthread_mutex_t pl_lock;
      struct glob_peer_list* gpl;
      struct loc_addr_clnt_num* l_a;
      int sz, cap, local_sock;
      char* local_mac;
      char* name;
      _Bool continuous;
      struct read_thread* rt;
      void* read_func;
};

struct read_msg_arg{
      struct peer_list* pl;
      int index;
};

void gpl_init(struct glob_peer_list* gpl);
void pl_init(struct peer_list* pl, uint16_t port_num);
void pl_add(struct peer_list* pl, struct sockaddr_in la, int clnt_num, char* name, char* mac);
int pl_remove(struct peer_list* pl, int peer_ind, char** gpl_i);
void pl_free(struct peer_list* pl);
void pl_print(struct peer_list* pl);
void gple_add_route_entry(struct glob_peer_list_entry* gple, int rel_no);
_Bool gple_remove_route_entry(struct glob_peer_list_entry* gple, int rel_no);
void gpl_init(struct glob_peer_list* gpl);
struct glob_peer_list_entry* gpl_add(struct glob_peer_list* gpl, char* name, char* mac);
void gpl_remove(struct glob_peer_list* gpl, int gpl_i);
void gpl_free(struct glob_peer_list* gpl);
int has_peer(struct peer_list* pl, char* mac);
struct glob_peer_list_entry* glob_peer_route(struct peer_list* pl, char* mac, int el, _Bool* cont);
void rt_init(struct read_thread* rt);
pthread_t add_read_thread(struct peer_list* pl, void *(*read_th_fnc) (void *));
