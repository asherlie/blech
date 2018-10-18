#ifndef _SND_H
#define _SND_H
#include "peer_list.h"

#define MSG_SND   0
#define MSG_PASS  1
#define PEER_PASS 2
#define PEER_EXIT 3
#define MSG_BLAST 4
#define FROM_OTHR 5

_Bool abs_snd_msg(struct loc_addr_clnt_num* la, int n, int msg_type, int sender_sz, int msg_sz, int recp, char* sender, char* msg, int u_msg_no, int adtnl_int);
_Bool init_prop_msg(struct peer_list* pl, _Bool skip_lst, int msg_type, char* msg, int msg_sz, int op_int);
int snd_txt_to_peers(struct peer_list* pl, char* msg, int msg_sz);
//_Bool snd_pm(struct peer_list* pl, char* msg, int recp);
_Bool prop_msg(struct loc_addr_clnt_num* la, int peer_no, struct peer_list* pl, 
               int msg_type, int msg_sz, char* buf, int recp, char* sndr, int adtnl_int);
_Bool read_messages(int s, int* recp, char** name, char** msg, int* adtnl_int, int msg_sz_cap);
_Bool read_msg_msg_pass(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int peer_no);
_Bool read_msg_peer_exit(struct peer_list* pl, int* recp, char* sndr_name, int* sndr_u_id, int peer_no);
_Bool read_msg_msg_blast(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int peer_no);
_Bool read_msg_peer_pass(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int* new_u_id, int peer_no);
_Bool read_msg_msg_snd(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int peer_no);
void read_messages_pth(struct read_msg_arg* rma);
#endif