#include "snd.h"
/*#include "net.h"*/

extern int msg_no;

// if *_sz == 0, entry will not be sent
// u_msg_no - a unique message identifier
// adtnl_int will be sent if it's >= 0
_Bool abs_snd_msg(struct loc_addr_clnt_num* la, int n, int msg_type, int sender_sz, int msg_sz, int recp, char* sender, char* msg, int u_msg_no, int adtnl_int){
      #ifdef DEBUG
      printf("in abs_snd_msg. type: %i, rcp: %i, sndr: %s, msg: %s\n", msg_type, recp, sender, msg);
      #endif
      _Bool ret = 1;
      for(int i = 0; i < n; ++i){
            ret = (send(la[i].clnt_num, &msg_type, 4, 0L) == 4);
            ret = (send(la[i].clnt_num, &u_msg_no, 4, 0L) == 4) && ret;
            if(recp >= 0)ret = (send(la[i].clnt_num, &recp, 4, 0L) == 4) && ret;
            if(sender_sz)ret = (send(la[i].clnt_num, sender, 30, 0L) == 30) && ret;
            if(msg_sz)ret = (send(la[i].clnt_num, msg, msg_sz, 0L) == msg_sz) && ret;
            if(adtnl_int >= 0)ret = (send(la[i].clnt_num, &adtnl_int, 4, 0L) == 4) && ret;
            #ifdef DEBUG
            if(adtnl_int >= 0)printf("sending additional int in a %i message\n", msg_type);
            #endif
      }
      return ret;
}

// prop_msg works by sending a message to each global peer and each local no global peer
// if(skip_lst), last local peer will be skipped, helpful for peer passes
// TODO: change the way messages are sent
// op_int will be sent if >= 0
_Bool init_prop_msg(struct peer_list* pl, _Bool skip_lst, int msg_type, char* msg, int msg_sz, int op_int){
      _Bool ret = 1;
      #ifdef DEBUG
      printf("init_prop_msg: iterating through %i global peers and %i local\n", pl->gpl->sz, pl->sz-skip_lst);
      #endif
      for(int i = 0; i < pl->gpl->sz; ++i){
            ret = ret && abs_snd_msg(&pl->l_a[pl->gpl->gpl[i].dir_p[0]], 1, msg_type, 30, msg_sz, pl->gpl->gpl[i].u_id, pl->name, msg, msg_no++, op_int);
      }
      for(int i = 0; i < pl->sz-skip_lst; ++i){
            if(!in_glob_route(pl, i))ret = ret && abs_snd_msg(&pl->l_a[i], 1, msg_type, 30, msg_sz, pl->l_a[i].u_id, pl->name, msg, msg_no++, op_int);
      }
      return ret;
}

int snd_txt_to_peers(struct peer_list* pl, char* msg, int msg_sz){
      pthread_mutex_lock(&pl->pl_lock);
      _Bool ret = init_prop_msg(pl, 0, MSG_BLAST, msg, msg_sz, -1);
      pthread_mutex_unlock(&pl->pl_lock);
      return ret;
}

// recp is a u_id
_Bool snd_pm(struct peer_list* pl, char* msg, int msg_sz, int recp){
      /*returns 3 if peer is me, 1 if local peer, 2 if global, 0 else*/
      int loc_addr = -1;
      int peer_type = has_peer(pl, NULL, recp, NULL, &loc_addr, NULL);
      if(peer_type == 1)
            abs_snd_msg(&pl->l_a[loc_addr], 1, MSG_SND, 30, msg_sz, recp, pl->name, msg, msg_no++, -1);
      else if(peer_type == 2)
            abs_snd_msg(&pl->l_a[loc_addr], 1, MSG_PASS, 30, msg_sz, recp, pl->name, msg, msg_no++, -1);
            /*init_prop_msg(pl, 0, MSG_PASS, msg, msg_sz, -1);*/
      return peer_type;
}

// name of sender
// peer_no refers to the rma->index of the caller/peer number of the sender
// alt_msg_type is substituted, if it exists, when a recp is a local peer
_Bool prop_msg(struct loc_addr_clnt_num* la, int peer_no, struct peer_list* pl, int msg_type,
               int alt_msg_type, int msg_sz, char* buf, int recp, char* sndr, int adtnl_int){
      /*if(cur_msg_no == pre_msg_no || (msg_type == PEER_PASS && new_u_id == rma->pl->u_id))continue;*/
      // la_r implies MSG_PASS or PEER_PASS
      struct glob_peer_list_entry* route = NULL;
      if(la){
            abs_snd_msg(la, 1, (alt_msg_type >= 0) ? alt_msg_type : msg_type, 30, msg_sz, la->u_id, sndr, buf, msg_no++, adtnl_int);
      }
      else if((route = glob_peer_route(pl, recp, peer_no, NULL, NULL))){
            abs_snd_msg(&pl->l_a[route->dir_p[0]], 1, msg_type, 30, msg_sz, recp, sndr, buf, msg_no++, adtnl_int);
      }
      return 1;
}

// if msg size is unspecified or msg_type != PEER_PASS, msg_sz_cap can be safely set to 0
_Bool read_messages(int s, int* recp, char** name, char** msg, int* adtnl_int, int msg_sz_cap){
      if(recp)read(s, recp, 4);
      if(name)read(s, *name, 30);
      if(msg)read(s, *msg, (msg_sz_cap) ? msg_sz_cap : 1024);
      if(adtnl_int)read(s, adtnl_int, 1024);
      return 1;
}

// these wrappers handle propogation
// peer_no refers to the rma->index of the caller/peer number of the sender
_Bool read_msg_msg_pass(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int peer_no){
      read_messages(pl->l_a[peer_no].clnt_num, recp, &sndr_name, &msg, NULL, 0);
      struct loc_addr_clnt_num* la_r = find_peer(pl, *recp);
      return prop_msg(la_r, peer_no, pl, MSG_PASS, MSG_SND, 1024, msg, *recp, sndr_name, -1);
}

// PEER_EXIT uses optional int field to send sndr info rather than sndr_name - nvm
_Bool read_msg_peer_exit(struct peer_list* pl, int* recp, char* sndr_name, int* sndr_u_id, int peer_no){
      read_messages(pl->l_a[peer_no].clnt_num, recp, &sndr_name, NULL, sndr_u_id, peer_no);
      struct loc_addr_clnt_num* la_r = find_peer(pl, *recp);
      return prop_msg(la_r, peer_no, pl, PEER_EXIT, -1, 0, NULL, *recp, sndr_name, *sndr_u_id);
}

_Bool read_msg_msg_blast(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int peer_no){
      read_messages(pl->l_a[peer_no].clnt_num, recp, &sndr_name, &msg, NULL, 0);
      struct loc_addr_clnt_num* la_r = find_peer(pl, *recp);
      return prop_msg(la_r, peer_no, pl, MSG_BLAST, -1, 1024, msg, *recp, sndr_name, -1);
}

// peer pass uses all fields
_Bool read_msg_peer_pass(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int* new_u_id, int peer_no){
      read_messages(pl->l_a[peer_no].clnt_num, recp, &sndr_name, &msg, new_u_id, 30);
      struct loc_addr_clnt_num* la_r = find_peer(pl, *recp);
      return prop_msg(la_r, peer_no, pl, PEER_PASS, -1, 30, msg, *recp, sndr_name, *new_u_id);
}

_Bool read_msg_msg_snd(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int peer_no){
      return read_messages(pl->l_a[peer_no].clnt_num, recp, &sndr_name, &msg, NULL, 0);
}

void read_messages_pth(struct read_msg_arg* rma){
      char buf[1024] = {0};
      int recp = -1;
      char name[30] = {0};
      int msg_type = -1, cur_msg_no = -1, pre_msg_no = -1;
      int new_u_id;
      /*while(rma->pl->read_th_wait)usleep(10000);*/
      while(rma->pl->l_a[rma->index].continuous){
            #ifdef DEBUG
            int n_reads = 0;
            #endif
            read(rma->pl->l_a[rma->index].clnt_num, &msg_type, 4);
            read(rma->pl->l_a[rma->index].clnt_num, &cur_msg_no, 4);
            // TODO: handle this
            if(pre_msg_no == cur_msg_no)puts("uhoh");
            #ifdef DEBUG
            puts("msg type and message number have been read - awaiting more messages");
            n_reads += 2;
            #endif
            // recp refers to the intended recipient of the message's u_id
            int peer_ind = -1;
            switch(msg_type){
                  case MSG_SND:
                        read_msg_msg_snd(rma->pl, &recp, name, buf, rma->index);
                        printf("%s%s%s: %s\n", (has_peer(rma->pl, name, -1, NULL, NULL, NULL) == 1) ? ANSI_BLU : ANSI_GRE, name, ANSI_NON, buf);
                        break;
                  case MSG_PASS:
                        read_msg_msg_pass(rma->pl, &recp, name, buf, rma->index);
                        break;
                  case PEER_PASS:
                        /*pthread_mutex_lock(&rma->pl->sock_lock);*/
                        read_msg_peer_pass(rma->pl, &recp, name, buf, &new_u_id, rma->index);
                        /*pthread_mutex_unlock(&rma->pl->sock_lock);*/
                        _Bool has_route;
                        struct glob_peer_list_entry* route = glob_peer_route(rma->pl, recp, rma->index, &has_route, NULL);
                        if(route && has_route)continue;
                        #ifdef DEBUG
                        if(!route)puts("new user found");
                        else if(!has_route)puts("new route to existing user found");
                        #endif
                        // if we have the global peer already but this PEER_PASS is coming from a different local peer
                        // we'll want to record this new possible route in gpl->dir_p
                        if(!route)printf("new [%sglb%s] peer: %s has joined %s~the network~%s\n", ANSI_GRE, ANSI_NON, buf, ANSI_RED, ANSI_NON);
                        pthread_mutex_lock(&rma->pl->pl_lock);
                        if(route)gple_add_route_entry(route, rma->index);
                        // recp here refers not to the new user as it should, but to me or an existing peer
                        /*else gple_add_route_entry(gpl_add(rma->pl->gpl, strdup(name), recp), rma->index);*/
                        else gple_add_route_entry(gpl_add(rma->pl->gpl, strdup(buf), new_u_id), rma->index);
                        pthread_mutex_unlock(&rma->pl->pl_lock);
                        break;
                  case MSG_BLAST:
                        read_msg_msg_blast(rma->pl, &recp, name, buf, rma->index);
                        printf("%s%s%s: %s\n", (has_peer(rma->pl, name, -1, NULL, NULL, NULL) == 1) ? ANSI_BLU : ANSI_GRE, name, ANSI_NON, buf);
                        break;
                  case PEER_EXIT:
                        read_msg_peer_exit(rma->pl, &recp, name, &peer_ind, rma->index);
                        // peer_ind = u_id
                        /*printf("user %s has disconnected\n", rma->pl->l_a[rma->index].clnt_info[0]);*/
                        printf("user %s has disconnected\n", name);
                        /*printf("route to peer %s has been lost\n", name);*/
                        char* lost_route[rma->pl->gpl->sz];
                        #ifdef DEBUG
                        puts("attempting to remove peer list entry");
                        #endif
                        int gpl_i = -1;
                        int ploc = has_peer(rma->pl, NULL, peer_ind, NULL, NULL, &gpl_i);
                        /*returns 3 if peer is me, 1 if local peer, 2 if global, 0 else*/
                        // global peers need to be sent peer exits and everyone receiving one must check if any routes are lost
                        int lost = 0;
                        // TODO: rma->index should not be removed if not local
                        // TODO: is it a safe assumption that if we're removing a global peer it's rma->index?
                        // TODO: handle loss of route from global peer disconnection
                        if(ploc == 1)lost = pl_remove(rma->pl, rma->index, lost_route);
                        else if(ploc == 2)gpl_remove(rma->pl->gpl, gpl_i, 1);
                        #ifdef DEBUG
                        puts("SUCCESS");
                        #endif
                        for(int i = 0; i < lost; ++i)
                              printf("route to global peer %s has been lost\n", lost_route[i]);
                              /*printf("route to peer %s has been lost\n", lost_route[i]);*/
                        return;
            }
            memset(buf, 0, sizeof(buf));
            memset(name, 0, 30);
            pre_msg_no = cur_msg_no;
      }
}
