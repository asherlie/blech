#ifndef _PL_H
#define _PL_H

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

#define ANSI_RED   "\x1B[31m"
#define ANSI_NON   "\x1b[0m"
#define ANSI_GRE   "\x1b[32m"
#define ANSI_BLU   "\x1b[34m"
#define ANSI_MGNTA "\x1b[35m"

struct loc_addr_clnt_num{
      struct sockaddr_in l_a; 
      struct read_msg_arg* rma;
      int clnt_num, u_id;
      // [client name, mac]
      char** clnt_info;
      _Bool continuous;
};

struct glob_peer_list_entry{
      int u_id;
      // [client name, addr]
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
      pthread_mutex_t r_th_lck;
      int sz, cap;
      pthread_t* th;
};

struct fs_block{
      int u_fn, data_sz;
      char* data;
};

/* there are global file numbers - each time a file is uploaded, it is assigned a unique id
 * all users know when a new file is added
 * only those who have privelige know about the f_list (of u_id's)
 * it takes both to dl a file
 * each element needs u_fn and data segment
 */
struct filestor{
      struct fs_block* file_chunks;
      int sz, cap;
};

struct file_acc{
      // contains a list of u_id's
      int u_fn;// f_sz;
      char* fname;
      int* f_list;
};

struct filesys{
      struct filestor storage;
      int n_files, cap;
      struct file_acc* files;
};

struct peer_list{
      pthread_mutex_t pl_lock, sock_lock;
      struct glob_peer_list* gpl;
      struct loc_addr_clnt_num* l_a;
      // u_id is unique on network
      int sz, cap, local_sock, u_id;
      char* name;
      _Bool continuous, read_th_wait, sock_set;
      struct read_thread* rt;
      struct filesys file_system;
};

struct read_msg_arg{
      struct peer_list* pl;
      int index;
};

_Bool fs_add_stor(struct filesys* fs, int u_fn, char* data, int data_sz);
_Bool fs_add_acc(struct filesys* fs, int u_fn, char* fname, int* u_id_lst);
void fs_print(struct filesys* fs);
void fs_init(struct filesys* fs);
struct file_acc* find_file(struct filesys* fs, int u_fn);
struct fs_block* fs_get_stor(struct filesys* fs, int u_fn);
struct file_acc* fs_get_acc(struct filesys* fs, int u_fn);
void fs_free(struct filesys* fs);

void gpl_init(struct glob_peer_list* gpl);
void gple_add_route_entry(struct glob_peer_list_entry* gple, int rel_no);
_Bool gple_remove_route_entry(struct glob_peer_list_entry* gple, int rel_no);
struct glob_peer_list_entry* gpl_add(struct glob_peer_list* gpl, char* name, int u_id);
struct glob_peer_list_entry* glob_peer_route(struct peer_list* pl, int u_id, int el, _Bool* cont, int* gpl_ind);
void gpl_remove(struct glob_peer_list* gpl, int gpl_i, _Bool keep_name);
void gpl_free(struct glob_peer_list* gpl);

void pl_set_local_sock(struct peer_list* pl, in_addr_t* bind_addr, uint16_t port_num);
void pl_init(struct peer_list* pl);
void pl_add(struct peer_list* pl, struct sockaddr_in la, int clnt_num, char* name, int u_id);
int pl_remove(struct peer_list* pl, int peer_ind, char** gpl_i);
void pl_free(struct peer_list* pl);
void pl_print(struct peer_list* pl);
struct loc_addr_clnt_num* find_peer(struct peer_list* pl, int u_id);

int has_peer(struct peer_list* pl, char* name, int u_id, int* u_id_set, int* loc_num, int* glob_num);
_Bool in_glob_route(struct peer_list* pl, int pl_ind);
int* get_dir_p(struct peer_list* pl, int glob_u_id, int* n);

void rt_init(struct read_thread* rt);
pthread_t add_read_thread(struct peer_list* pl);
_Bool blech_init(struct peer_list* pl, char* sterm, int portnum);
void safe_exit(struct peer_list* pl);

int u_id_to_loc_id(struct peer_list* pl, int u_id);
void set_next_uid(int val);
int assign_uid();
#endif
