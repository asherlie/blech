#include "snd.h"

extern int msg_no;
int next_ufn = 0;

// if *_sz == 0, entry will not be sent
// u_msg_no - a unique message identifier
// adtnl_int will be sent if it's >= 0
_Bool abs_snd_msg(struct loc_addr_clnt_num* la, int n, int msg_type, int sender_sz, int msg_sz, int recp, char* sender, char* msg, int u_msg_no, int adtnl_int, _Bool adtnl_first){
      #ifdef DEBUG
      printf("in abs_snd_msg. type: %i, rcp: %i, sndr: %s, msg: %s\n", msg_type, recp, sender, msg);
      #endif
      _Bool ret = 1;
      for(int i = 0; i < n; ++i){
            ret = (send(la[i].clnt_num, &msg_type, 4, 0L) == 4);
            ret = (send(la[i].clnt_num, &u_msg_no, 4, 0L) == 4) && ret;
            if(adtnl_first && adtnl_int >= 0)ret = (send(la[i].clnt_num, &adtnl_int, 4, 0L) == 4) && ret;
            if(recp >= 0)ret = (send(la[i].clnt_num, &recp, 4, 0L) == 4) && ret;
            if(sender_sz)ret = (send(la[i].clnt_num, sender, 30, 0L) == 30) && ret;
            if(msg_sz)ret = (send(la[i].clnt_num, msg, msg_sz, 0L) == msg_sz) && ret;
            if(!adtnl_first && adtnl_int >= 0)ret = (send(la[i].clnt_num, &adtnl_int, 4, 0L) == 4) && ret;
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
            ret = ret && abs_snd_msg(&pl->l_a[pl->gpl->gpl[i].dir_p[0]], 1, msg_type, 30, msg_sz, pl->gpl->gpl[i].u_id, pl->name, msg, msg_no++, op_int, 0);
      }
      for(int i = 0; i < pl->sz-skip_lst; ++i){
            if(!in_glob_route(pl, i))ret = ret && abs_snd_msg(&pl->l_a[i], 1, msg_type, 30, msg_sz, pl->l_a[i].u_id, pl->name, msg, msg_no++, op_int, 0);
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
            abs_snd_msg(&pl->l_a[loc_addr], 1, MSG_SND, 30, msg_sz, recp, pl->name, msg, msg_no++, -1, 0);
      else if(peer_type == 2)
            abs_snd_msg(&pl->l_a[loc_addr], 1, MSG_PASS, 30, msg_sz, recp, pl->name, msg, msg_no++, -1, 0);
            /*init_prop_msg(pl, 0, MSG_PASS, msg, msg_sz, -1);*/
      return peer_type;
}

// name of sender
// peer_no refers to the rma->index of the caller/peer number of the sender
// alt_msg_type is substituted, if it exists, when a recp is a local peer
_Bool prop_msg(struct loc_addr_clnt_num* la, int peer_no, struct peer_list* pl, int msg_type,
               int alt_msg_type, int msg_sz, char* buf, int recp, char* sndr, int adtnl_int, _Bool adtnl_first){
      /*if(cur_msg_no == pre_msg_no || (msg_type == PEER_PASS && new_u_id == rma->pl->u_id))continue;*/
      // la_r implies MSG_PASS or PEER_PASS
      struct glob_peer_list_entry* route = NULL;
      if(la){
            abs_snd_msg(la, 1, (alt_msg_type >= 0) ? alt_msg_type : msg_type, 30, msg_sz, la->u_id, sndr, buf, msg_no++, adtnl_int, adtnl_first);
      }
      // TODO: is this usage of peer_no appropriate
      else if((route = glob_peer_route(pl, recp, peer_no, NULL, NULL))){
            abs_snd_msg(&pl->l_a[route->dir_p[0]], 1, msg_type, 30, msg_sz, recp, sndr, buf, msg_no++, adtnl_int, adtnl_first);
      }
      return 1;
}

// shares a file that i have access to with another user
_Bool file_share(struct peer_list* pl, int u_id, int u_fn){
      // sends int n_peers, and an int* of u_id's in order to download, as well as f_id
      // fs_add_acc will be called from the above
      /*FILE_SHARE*/
      struct file_acc* loc_file = find_file(&pl->file_system, u_fn);
      if(!loc_file)return 0;
      int sz = 0;
      for(; loc_file->f_list[sz] != -1; ++sz);
      loc_file->f_list[sz] = u_fn;
      struct loc_addr_clnt_num* la_r = find_peer(pl, u_id);
      // f_list's last element is -1
      // we need to send loc_file.f_list, and .fname
      // i can just prop_msg with file list cast to char* and size
      // sending additional first because it refers to size of f_list in FILE_SHARE messages
      return prop_msg(la_r, u_id, pl, FILE_SHARE, -1, sizeof(int)*(sz+1), (char*)loc_file->f_list, u_id, loc_file->fname, sz+1, 1);
}

// if msg size is unspecified or msg_type != PEER_PASS, msg_sz_cap can be safely set to 0
_Bool read_messages(int s, int* recp, char** name, char** msg, int* adtnl_int, int msg_sz_cap){
      if(recp)read(s, recp, 4);
      if(name)read(s, *name, 30);
      if(msg)read(s, *msg, (msg_sz_cap) ? msg_sz_cap : 1024);
      if(adtnl_int)read(s, adtnl_int, 4);
      return 1;
}

// fname size is capped at 30
int* read_msg_file_share(struct peer_list* pl, int* recp, int* u_fn, int* n_ints, char* fname, int peer_no){
      // we just need to read recp, u_fn and number of ints we're about to recv
      // n_ints must be sent first
      // we first need to read size of f_list
      // we need to set the last value of f_list to -1 and record its old value as u_fn
      // adtnl
      read(pl->l_a[peer_no].clnt_num, n_ints, 4);
      // recp
      read(pl->l_a[peer_no].clnt_num, recp, 4);
      // sndr
      read(pl->l_a[peer_no].clnt_num, fname, 30);
      /*#ifdef DEBUG*/
      printf("just read fname: %s\n", fname);
      /*#endif*/
      int* ret = malloc(sizeof(int)**n_ints);
      read(pl->l_a[peer_no].clnt_num, ret, sizeof(int)**n_ints);
      *u_fn = ret[*n_ints-1];
      ret[*n_ints-1] = -1;
      --*n_ints;
      /*read_messages(pl->l_a[peer_no].clnt_num, recp, NULL, NULL, u_fn, 0);*/
      struct loc_addr_clnt_num* la_r = find_peer(pl, *recp);
      prop_msg(la_r, peer_no, pl, FILE_SHARE, -1, sizeof(int)**n_ints, (char*)ret, *recp, fname, *u_fn, 1);
      return ret;
      /*prop_msg must be used here incase of indirect route aka global*/

}

// FILE_ALERT does NOT imply access
_Bool read_msg_file_alert(struct peer_list* pl, int* recp, int* new_u_fn, int peer_no){
      read_messages(pl->l_a[peer_no].clnt_num, recp, NULL, NULL, new_u_fn, 0);
      struct loc_addr_clnt_num* la_r = find_peer(pl, *recp);
      return prop_msg(la_r, peer_no, pl, FILE_ALERT, -1, 0, NULL, *recp, NULL, *new_u_fn, 0);
}

// FILE_CHUNK uses msg field to store filename
// returns a malloc'd char*
char* read_msg_file_chunk(struct peer_list* pl, int* recp, char* fname, int* chunk_sz, int* u_fn, int peer_no){
      read_messages(pl->l_a[peer_no].clnt_num, recp, NULL, &fname, chunk_sz, 0);
      read(pl->l_a[peer_no].clnt_num, u_fn, 4);
      char* buf = calloc(*chunk_sz+1, sizeof(char));
      read(pl->l_a[peer_no].clnt_num, buf, *chunk_sz);
      return buf;
}

// these wrappers handle propogation
// peer_no refers to the rma->index of the caller/peer number of the sender
_Bool read_msg_msg_pass(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int peer_no){
      read_messages(pl->l_a[peer_no].clnt_num, recp, &sndr_name, &msg, NULL, 0);
      struct loc_addr_clnt_num* la_r = find_peer(pl, *recp);
      return prop_msg(la_r, peer_no, pl, MSG_PASS, MSG_SND, 1024, msg, *recp, sndr_name, -1, 0);
}

// PEER_EXIT uses optional int field to send sndr info rather than sndr_name - nvm
_Bool read_msg_peer_exit(struct peer_list* pl, int* recp, char* sndr_name, int* sndr_u_id, int peer_no){
      read_messages(pl->l_a[peer_no].clnt_num, recp, &sndr_name, NULL, sndr_u_id, peer_no);
      struct loc_addr_clnt_num* la_r = find_peer(pl, *recp);
      return prop_msg(la_r, peer_no, pl, PEER_EXIT, -1, 0, NULL, *recp, sndr_name, *sndr_u_id, 0);
}

_Bool read_msg_msg_blast(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int peer_no){
      read_messages(pl->l_a[peer_no].clnt_num, recp, &sndr_name, &msg, NULL, 0);
      struct loc_addr_clnt_num* la_r = find_peer(pl, *recp);
      return prop_msg(la_r, peer_no, pl, MSG_BLAST, -1, 1024, msg, *recp, sndr_name, -1, 0);
}

// peer pass uses all fields
_Bool read_msg_peer_pass(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int* new_u_id, int peer_no){
      read_messages(pl->l_a[peer_no].clnt_num, recp, &sndr_name, &msg, new_u_id, 30);
      struct loc_addr_clnt_num* la_r = find_peer(pl, *recp);
      return prop_msg(la_r, peer_no, pl, PEER_PASS, -1, 30, msg, *recp, sndr_name, *new_u_id, 0);
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
      int u_fn = -1;
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
            int* f_list = NULL;
            switch(msg_type){
                  case FILE_ALERT:
                        // new u_fn is stored in new_u_id
                        read_msg_file_alert(rma->pl, &recp, &new_u_id, rma->index);
                        next_ufn = new_u_id+1;
                        break;
                  case FILE_SHARE:
                        /*abs_snd_msg();*/
                        // n_ints in file route is stored in new_u_id for some reason
                        f_list = read_msg_file_share(rma->pl, &recp, &u_fn, &new_u_id, name, rma->index);
                        if(recp == rma->pl->u_id)fs_add_acc(&rma->pl->file_system, u_fn, strdup(name), f_list);
                        else free(f_list);
                        break;
                  case FILE_CHUNK:
                        /*int chunk_sz = -1;*/
                        // buf stores file name, new_u_id stores chunk size - for some reason
                        // returns char* of file chunk
                        // TODO: file name shouldn't be sent to me! i'm just a middleman
                        fs_add_stor(&rma->pl->file_system, u_fn, read_msg_file_chunk(rma->pl, &recp, buf, &new_u_id, &u_fn, rma->index), new_u_id);
                        break;
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
                        // if we're removing a local peer, this thread can safely exit
                        if(ploc == 1)return;
            }
            memset(buf, 0, sizeof(buf));
            memset(name, 0, 30);
            pre_msg_no = cur_msg_no;
      }
}

int assign_u_fn(){
      return next_ufn++;
}

// returns an int[] with the in-order list of peers where fname is stored
// TODO: have a share file option that inform a peer of this list
int* upload_file(struct peer_list* pl, char* fname){
      if(!pl->sz)return NULL;
      int u_fn = assign_u_fn();
      FILE* fp = fopen(fname, "r");
      fseek(fp, 0L, SEEK_END);
      int fsz = ftell(fp);
      rewind(fp);
      int n_p = pl->sz + pl->gpl->sz;
      // easy system for now
      // TODO: use a better system to calculate n_chunks
      int n_chunks = (n_p <= 10) ? n_p : 10;
      int sz_per_chunk = fsz/n_chunks;
      #ifdef DEBUG
      printf("file will be uploaded in %i chunks\n", n_chunks);
      #endif
      off_t offset = 0;
      printf("file size: %i, sz_per_chunk: %i\n", fsz, sz_per_chunk);
      char* buf = calloc(fsz, sizeof(char));
      // +1 for last index, -1
      int* ret = calloc(n_chunks+1, sizeof(int));
      fread(buf, sizeof(char), fsz, fp);
      fclose(fp);
      // FILE_ALERT simply makes peers aware of new f_id
      init_prop_msg(pl, 0, FILE_ALERT, NULL, 0, u_fn);
      // TODO: files should also be distributed among global peers
      for(int i = 0; i < pl->sz; ++i){
            // FILE_CHUNK messages contain only recp, msg - msg contains file name, and adtnl_int - containing chunk sz
            // TODO: sz_per_chunk will be inaccurate on last iteration
            abs_snd_msg(&pl->l_a[i], 1, FILE_CHUNK, 0, strlen(fname), pl->l_a[i].u_id, NULL, fname, msg_no++, sz_per_chunk, 0);
            send(pl->l_a[i].clnt_num, &u_fn, 4, 0L);
            send(pl->l_a[i].clnt_num, buf+offset, sz_per_chunk, 0L);
            offset += sz_per_chunk;
            ret[ret[n_chunks]++] = pl->l_a[i].u_id;
      }
      ret[n_chunks] = -1;
      // adding our new file to our access list
      fs_add_acc(&pl->file_system, u_fn, fname, ret);
      return ret;
}
