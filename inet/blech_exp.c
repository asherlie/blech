#include <string.h>
#include "peer_list.h"

#define PORTNUM 2010

int msg_no = 0;

_Bool strtoi(const char* str, unsigned int* ui, int* i){
      char* res;
      unsigned int r = (unsigned int)strtol(str, &res, 10);
      if(*res)return 0;
      if(i)*i = (int)r;
      if(ui)*ui = r;
      return 1;
}

int net_connect(char* host, int* sock, uint16_t port_num){
      struct sockaddr_in serv_addr;
      bzero(&serv_addr, sizeof(struct sockaddr_in));
      int status = 0, s;
      if(!host)return -1;
      s = socket(AF_INET, SOCK_STREAM, 0);
      struct hostent* serv = gethostbyname(host);
      if(!serv)return -1;
      serv_addr.sin_family = AF_INET;
      bcopy((char*)serv->h_addr, (char*)&serv_addr.sin_addr.s_addr, serv->h_length);
      serv_addr.sin_port = htons(port_num);
      status = connect(s, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in));
      *sock = s;
      return status;
}

struct loc_addr_clnt_num* find_peer(struct peer_list* pl, int u_id){
      struct loc_addr_clnt_num* ret = NULL;
      pthread_mutex_lock(&pl->pl_lock);
      for(int i = 0; i < pl->sz; ++i)
            if(pl->l_a[i].u_id == u_id)ret = &pl->l_a[i];
      pthread_mutex_unlock(&pl->pl_lock);
      return ret;
}

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
            /*if(recp_sz)ret = (send(la[i].clnt_num, recp, 18, 0L) == 18) && ret;*/
            if(recp >= 0)ret = (send(la[i].clnt_num, &recp, 4, 0L) == 4) && ret;
            if(sender_sz)ret = (send(la[i].clnt_num, sender, 30, 0L) == 30) && ret;
            if(msg_sz)ret = (send(la[i].clnt_num, msg, msg_sz, 0L) == msg_sz) && ret;
            // this is being sent but not being read by read_messages_pth - waiting for new u_id
            if(adtnl_int >= 0)ret = (send(la[i].clnt_num, &adtnl_int, 4, 0L) == 4) && ret;
            #ifdef DEBUG
            if(adtnl_int >= 0)printf("sending additional int in a %i message\n", msg_type);
            #endif
      }
      return ret;
}

/*
 *_Bool snd_peer_pass(char* recp, char* ){
 *      abs_snd_msg(&pl->l_a[i], 1, msg_type, 30, msg_sz, pl->l_a[i].u_id, pl->name, msg, msg_no++, op_int);
 *}
 */

// prop_msg works by sending a message to each global peer and each local no global peer
// if(skip_lst), last local peer will be skipped, helpful for peer passes
// TODO: change the way messages are sent
// op_int will be sent if >= 0
_Bool init_prop_msg(struct peer_list* pl, _Bool skip_lst, int msg_type, char* msg, int msg_sz, int op_int){
      /*struct glob_peer_list_entry* route = glob_peer_route(pl, recp, gpl_i, NULL);*/
      _Bool ret = 1;
      #ifdef DEBUG
      printf("init_prop_msg: iterating through %i global peers and %i local\n", pl->gpl->sz, pl->sz-skip_lst);
      #endif
      for(int i = 0; i < pl->gpl->sz; ++i){
            ret = ret && abs_snd_msg(&pl->l_a[pl->gpl->gpl[i].dir_p[0]], 1, msg_type, 30, msg_sz, pl->gpl->gpl[i].u_id, pl->name, msg, msg_no++, op_int);
            /*sendr sz: 30, recp: uid of recp, sender, my name, msg: name of new peer*/
      }
      for(int i = 0; i < pl->sz-skip_lst; ++i){
            if(!in_glob_route(pl, i))ret = ret && abs_snd_msg(&pl->l_a[i], 1, msg_type, 30, msg_sz, pl->l_a[i].u_id, pl->name, msg, msg_no++, op_int);
      }
      return ret;
}

// sndr is always a 30 byte string
_Bool ssnd_msg(struct loc_addr_clnt_num* la, int n_peers, int msg_type, char* msg, int msg_sz, int recp, char* sndr){
      #ifdef DEBUG
      printf("sending message of type: %i to %i peers recp param: %i\n", msg_type, n_peers, recp);
      #endif
      int chng_this = 13;
      switch(msg_type){
            case MSG_PASS : return abs_snd_msg(la, n_peers, MSG_PASS, 30, msg_sz, recp, sndr, msg, msg_no++, -1);
            case MSG_BLAST: return abs_snd_msg(la, n_peers, MSG_BLAST, 30, msg_sz, -1, sndr, msg, msg_no++, -1);
            /*case PEER_PASS: return abs_snd_msg(la, n_peers, PEER_PASS, 18, 0, 30, recp, NULL, msg);*/
            /*case PEER_PASS: return abs_snd_msg(la, n_peers, PEER_PASS, 18, 30, 0, recp, sndr, NULL, msg_no++);*/
            /*need u_id from name of local peer*/
            /*get rid of snd_msg*/
            case PEER_PASS: puts("sending peer passes from snd_msg is deprecated"); return abs_snd_msg(la, n_peers, PEER_PASS, 30, 30, recp, sndr, msg, msg_no++, chng_this);
            case FROM_OTHR: return abs_snd_msg(la, n_peers, FROM_OTHR, 30, msg_sz, -1, sndr, msg, msg_no++, -1);
            /*case MSG_SND  : return abs_snd_msg(la, n_peers, MSG_SND, 0, 0, msg_sz, NULL, NULL, msg);*/
            case MSG_SND  : return abs_snd_msg(la, n_peers, MSG_SND, 30, msg_sz, -1, sndr, msg, msg_no++, -1);
            case PEER_EXIT: return abs_snd_msg(la, n_peers, PEER_EXIT, 0, 0, -1, NULL, NULL, msg_no++, -1);
      }
      return 0;
}

int snd_txt_to_peers(struct peer_list* pl, char* msg, int msg_sz){
      pthread_mutex_lock(&pl->pl_lock);
      // this is not sending correctly - compare to msg snd protocol which works fine for pm's
      _Bool ret = init_prop_msg(pl, 0, MSG_BLAST, msg, msg_sz, -1);
      pthread_mutex_unlock(&pl->pl_lock);
      return ret;
}

void accept_connections(struct peer_list* pl){
      int clnt;
      /*char* addr = NULL;*/
      char name[248] = {0};
      struct sockaddr_in rem_addr;
      socklen_t opt = sizeof(rem_addr);
      int u_id = -1;
      while(pl->continuous){
            clnt = accept(pl->local_sock, (struct sockaddr *)&rem_addr, &opt);
            // don't set this if we're adding a peer who ATTACHED TO US
            pl->read_th_wait = 1;
            // we want to assign a new unique id to our new peer
            u_id = assign_uid(pl);
            #ifdef DEBUG
            printf("assign_uid called by accept_connections with result: %i\n", u_id);
            #endif
            send(clnt, &u_id, 4, 0L);
            // as soon as a new client is added, wait for them to send their desired name
            read(clnt, name, 30);
            send(clnt, pl->name, 30, 0L);
            // sending our u_id
            send(clnt, &pl->u_id, 4, 0L);
            pl->read_th_wait = 0;
            #ifdef DEBUG
            printf("accepted connection from %s. assigned new user u_id: %i\n", name, u_id);
            #endif
            printf("new [%slcl%s] user: %s has joined %s~the network~%s\n", ANSI_BLU, ANSI_NON, name, ANSI_RED, ANSI_NON);
            // DO NOT add the same rem-addr mul times
            /*pl_add(pl, rem_addr, clnt, strdup(name), strdup(addr));*/
            pl_add(pl, rem_addr, clnt, strdup(name), u_id);
            // each time a peer is added, we need to send updated peer information to all peers
            // alert my local peers 
            // sz-1 so as not to send most recent peer
            #ifdef DEBUG
            puts("executing peer pass from accept_connections");
            #endif
            // this isn't propogating to newest user
            pthread_mutex_lock(&pl->pl_lock);
            // alert our current peers of new peer
            // name refers to the name of our new peer
            // u_id is u_id of new user
            init_prop_msg(pl, 1, PEER_PASS, name, 30, u_id);
            // < sz-1 because sz-1 is new peer - they're aware of themselves
            #ifdef DEBUG
            printf("sending new peer info to %i local peers from peer %s\n", pl->sz-1, pl->l_a[pl->sz-1].clnt_info[0]);
            printf("sending existing peer info from %i local peers to new peer\n", pl->sz-1);
            #endif
            /*
             *for(int i = 0; i < pl->sz-1; ++i){
             *      abs_snd_msg(&pl->l_a[pl->sz-1], 1, PEER_PASS, 30, 30, pl->l_a[pl->sz-1], pl->name, pl->l_a[i]);
             *}
             */




            for(int i = 0; i < pl->sz-1; ++i){
                  /*snd_msg(&pl->l_a[pl->sz-1], 1, PEER_PASS, pl->l_a[i].clnt_info[0], 30, pl->l_a[i].clnt_info[1], pl->name);*/
                  // new peer nickname goes in msg field message field
                  usleep(1000);
                  /*abs_snd_msg(&pl->l_a[pl->sz-1], 1, PEER_PASS, 30, 30, pl->l_a[pl->sz-1].u_id, pl->l_a[i].clnt_info[0], pl->name, msg_no++, pl->l_a[i].u_id);*/
                  abs_snd_msg(&pl->l_a[pl->sz-1], 1, PEER_PASS, 30, 30, pl->l_a[pl->sz-1].u_id, pl->name, pl->l_a[i].clnt_info[0], msg_no++, pl->l_a[i].u_id);
                  #ifdef DEBUG
                  /*printf("sent peer with number %i to existing peer #%i\n", pl->l_a[i].u_id, );*/
                  printf("sent local peer number %i info about peer #%i\n", pl->sz-1, pl->l_a[i].u_id);
                  #endif
                  /*abs_snd_msg(&pl->l_a[pl->sz-1], 1, PEER_PASS, 30, 30, pl->l_a[pl->sz-1].u_id, pl->name, pl->l_a[i].clnt_info[0], msg_no++, pl->l_a[i].u_id);*/
            }
            // alerting new peer of current global peers
            // this is alerting them of themselves
            #ifdef DEBUG
            printf("sending new info to %i glob\n", pl->gpl->sz);
            #endif
            /*i need to send the new user*/
            for(int i = 0; i < pl->gpl->sz; ++i){
                  // new peer nickname goes in msg field message field
                  usleep(1000);
                  abs_snd_msg(&pl->l_a[pl->sz-1], 1, PEER_PASS, 30, 30, pl->l_a[pl->sz-1].u_id, pl->gpl->gpl[i].clnt_info[0], pl->name, msg_no++, pl->gpl->gpl[i].u_id);
                  /*abs_snd_msg(&pl->l_a[pl->sz-1], 1, PEER_PASS, 30, 30, pl->l_a[pl->sz-1].u_id, pl->name, pl->gpl->gpl[i].clnt_info[0], msg_no++, pl->gpl->gpl[i].u_id);*/
            }
            /*for(int i = 0; i < pl)*/
            pthread_mutex_unlock(&pl->pl_lock);
            memset(name, 0, sizeof(name));
            /*memset((char*)addr, 0, sizeof(addr));*/
            /*free(addr);*/
            #ifdef DEBUG
            puts("sharing complete");
            #endif
      }
}

// if msg size is unspecified or msg_type != PEER_PASS, msg_sz_cap can be safely set to 0
_Bool read_messages(int s, int* recp, char** name, char** msg, int* adtnl_int, int msg_sz_cap){
      int sz = 0;
      if(recp){
            #ifdef DEBUG
            puts("attempting to read: recp");
            #endif
            sz = read(s, recp, 4);
            #ifdef DEBUG
            printf("read %i bytes\n", sz);
            puts("done");
            #endif
      }
      if(name){
            #ifdef DEBUG
            puts("attempting to read: name");
            #endif
            sz = read(s, *name, 30);
            #ifdef DEBUG
            printf("read %i bytes\n", sz);
            puts("done");
            #endif
      }
      if(msg){
            #ifdef DEBUG
            puts("attempting to read: msg");
            #endif
            sz = read(s, *msg, (msg_sz_cap) ? msg_sz_cap : 1024);
            #ifdef DEBUG
            printf("read %i bytes\n", sz);
            puts("done");
            #endif
      }
      if(adtnl_int){
            #ifdef DEBUG
            puts("attempting to read: adtnl_int");
            #endif
            sz = read(s, adtnl_int, 1024);
            #ifdef DEBUG
            printf("read %i bytes\n", sz);
            puts("done");
            #endif
      }
      return 1;
}

// name of sender
// peer_no refers to the rma->index of the caller/peer number of the sender
_Bool prop_msg(struct loc_addr_clnt_num* la, int peer_no, struct peer_list* pl, 
               int msg_type, int msg_sz, char* buf, int recp, char* sndr, int adtnl_int){
      /*if(cur_msg_no == pre_msg_no || (msg_type == PEER_PASS && new_u_id == rma->pl->u_id))continue;*/
      // la_r implies MSG_PASS or PEER_PASS
      struct glob_peer_list_entry* route = NULL;
      if(la){
            abs_snd_msg(la, 1, msg_type, 30, msg_sz, la->u_id, sndr, buf, msg_no++, adtnl_int);
      }
      else if((route = glob_peer_route(pl, recp, peer_no, NULL))){
            abs_snd_msg(&pl->l_a[route->dir_p[0]], 1, msg_type, 30, msg_sz, recp, sndr, buf, msg_no++, adtnl_int);
      }
      return 1;
}

// these wrappers handle propogation
// peer_no refers to the rma->index of the caller/peer number of the sender
_Bool read_msg_msg_pass(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int peer_no){
      read_messages(pl->l_a[peer_no].clnt_num, recp, &sndr_name, &msg, NULL, 0);
      struct loc_addr_clnt_num* la_r = find_peer(pl, *recp);
      return prop_msg(la_r, peer_no, pl, MSG_PASS, 1024, msg, *recp, sndr_name, -1);
}

_Bool read_msg_msg_blast(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int peer_no){
      read_messages(pl->l_a[peer_no].clnt_num, recp, &sndr_name, &msg, NULL, 0);
      struct loc_addr_clnt_num* la_r = find_peer(pl, *recp);
      return prop_msg(la_r, peer_no, pl, MSG_BLAST, 1024, msg, *recp, sndr_name, -1);
}

// peer pass uses all fields
_Bool read_msg_peer_pass(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int* new_u_id, int peer_no){
      #ifdef DEBUG
      puts("read_msg_peer_pass has been entered");
      #endif
      read_messages(pl->l_a[peer_no].clnt_num, recp, &sndr_name, &msg, new_u_id, 30);
      struct loc_addr_clnt_num* la_r = find_peer(pl, *recp);
      #ifdef DEBUG
      puts("about to exit read_msg_peer_pass");
      #endif
      return prop_msg(la_r, peer_no, pl, PEER_PASS, 30, msg, *recp, sndr_name, *new_u_id);
}

_Bool read_msg_msg_snd(struct peer_list* pl, int* recp, char* sndr_name, char* msg, int peer_no){
      return read_messages(pl->l_a[peer_no].clnt_num, recp, &sndr_name, &msg, NULL, 0);
}

void read_messages_pth(struct read_msg_arg* rma){
      char buf[1024] = {0};
      int recp = -1;
      char name[30] = {0};
      int msg_type = -1, cur_msg_no = -1, pre_msg_no = -1;
      int msg_sz = 0;
      struct loc_addr_clnt_num* la_r = NULL;
      int new_u_id;
      /*while(rma->pl->read_th_wait)usleep(10000);*/
      while(rma->pl->l_a[rma->index].continuous){
            #ifdef DEBUG
            int n_reads = 0;
            #endif
            read(rma->pl->l_a[rma->index].clnt_num, &msg_type, 4);
            read(rma->pl->l_a[rma->index].clnt_num, &cur_msg_no, 4);
            #ifdef DEBUG
            puts("msg type and message number have been read - awaiting more messages");
            n_reads += 2;
            #endif
            // recp refers to the intended recipient of the message's u_id
            /*printf("%s%s%s: %s\n", (has_peer(rma->pl, name, -1, NULL) == 1) ? ANSI_BLU : ANSI_GRE, name, ANSI_NON, buf);*/
            switch(msg_type){
                  case MSG_SND:
                        #ifdef DEBUG
                        fputs("MSG_SND read about to begin...", stdout);
                        #endif
                        read_msg_msg_snd(rma->pl, &recp, name, buf, rma->index);
                        #ifdef DEBUG
                        puts("done");
                        #endif
                        printf("%s%s%s: %s\n", (has_peer(rma->pl, name, -1, NULL) == 1) ? ANSI_BLU : ANSI_GRE, name, ANSI_NON, buf);
                        break;
                  case MSG_PASS:
                        // why is this being reached during a peer pass
                        puts("not yet implemented");
                        break;
                  case PEER_PASS:
                        #ifdef DEBUG
                        fputs("PEER_PASS read about to begin...", stdout);
                        #endif
                        /*pthread_mutex_lock(&rma->pl->sock_lock);*/
                        read_msg_peer_pass(rma->pl, &recp, name, buf, &new_u_id, rma->index);
                        /*pthread_mutex_unlock(&rma->pl->sock_lock);*/
                        #ifdef DEBUG
                        puts("done");
                        #endif
                        _Bool has_route;
                        struct glob_peer_list_entry* route = glob_peer_route(rma->pl, recp, rma->index, &has_route);
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
                        puts("not yet implemented");
                        break;
                  case PEER_EXIT:
                        printf("user %s has disconnected\n", rma->pl->l_a[rma->index].clnt_info[0]);
                        char* lost_route[rma->pl->gpl->sz];
                        #ifdef DEBUG
                        puts("attempting to remove peer list entry");
                        #endif
                        int lost = pl_remove(rma->pl, rma->index, lost_route);
                        #ifdef DEBUG
                        puts("SUCCESS");
                        #endif
                        for(int i = 0; i < lost; ++i)
                              printf("route to global peer %s has been lost\n", lost_route[i]);
                        return;
            }
            memset(buf, 0, msg_sz);
            memset(name, 0, 30);
            pre_msg_no = cur_msg_no;
            continue;
            if(msg_type == MSG_PASS || msg_type == PEER_PASS || msg_type == MSG_BLAST || msg_type == PEER_EXIT){
                  read(rma->pl->l_a[rma->index].clnt_num, &recp, 4);
                  #ifdef DEBUG
                  puts("recipient has been read");
                  ++n_reads;
                  #endif
                  /*if(msg_type == MSG_PASS)la_r = find_peer(rma->pl, recp);*/
                  la_r = find_peer(rma->pl, recp);
                  // is this a local peer?
            }
            if(msg_type == MSG_PASS || msg_type == MSG_BLAST || msg_type == MSG_SND || msg_type == PEER_PASS){
                  // name of sender
                  read(rma->pl->l_a[rma->index].clnt_num, name, 30);
                  #ifdef DEBUG
                  puts("name has been read");
                  ++n_reads;
                  #endif
            }
            // peer pass now sends new peer nickname in msg field
            if(msg_type != PEER_EXIT/* && msg_type != PEER_PASS*/){
                  msg_sz = read(rma->pl->l_a[rma->index].clnt_num, buf, sizeof(buf));
                  #ifdef DEBUG
                  ++n_reads;
                  #endif
            }
            /*printf("got %s from msg buf\n", buf);*/
            // only instance as of now where op_int is used
            int new_u_id = -1;
            if(msg_type == PEER_PASS){
                  #ifdef DEBUG
                  printf("%i reads have been completed so far\n", n_reads);
                  puts("waiting for new u_id...");
                  #endif
                  // TODO: this isn't being sent by the abs_snd_msg
                  read(rma->pl->l_a[rma->index].clnt_num, &new_u_id, 4);
                  #ifdef DEBUG
                  ++n_reads;
                  puts("new user u_id has been read");
                  #endif
            }
            // propogation
            if(msg_type == MSG_PASS || msg_type == MSG_BLAST || msg_type == PEER_PASS/* || msg_type == PEER_EXIT*/){
                  /*if(already_recvd_msg(msg_num))continue;*/
                  /*printf("cur msg no: %i, pre msg no: %i\n", cur_msg_no, pre_msg_no);*/
                  if(cur_msg_no == pre_msg_no || (msg_type == PEER_PASS && new_u_id == rma->pl->u_id))continue;
                  // la_r implies MSG_PASS or PEER_PASS
                  struct glob_peer_list_entry* route = NULL;
                  if(la_r){
                        #ifdef DEBUG
                        puts("ending a message's propogation path");
                        #endif
                        // name is who it's from
                        // TODO: this is incorrect for PEER_EXIT
                        /*snd_msg(la_r, 1, msg_type, buf, msg_sz, la_r->u_id, name);*/
                        /*
                         *ssnd_msg
                         *ssnd_msg(struct loc_addr_clnt_num *la, int n_peers, int msg_type, char *msg, int msg_sz, int recp, char *sndr)
                         */
                        abs_snd_msg(la_r, 1, msg_type, 30, msg_sz, la_r->u_id, name, buf, msg_no++, new_u_id);
                        /*abs_snd_msg(struct loc_addr_clnt_num *la, int n, int msg_type, int sender_sz, int msg_sz, int recp, char *sender, char *msg, int u_msg_no, int adtnl_int)*/
                        /*abs_snd_msg(la, n_peers, msg_type, 30, 30, recp, sndr, NULL, msg_no++, -1);*/
                        /*snd_msg(pl->l_a, pl->sz, PEER_EXIT, NULL, 0, pl->u_id, NULL);*/
                  }
                  // we can make the assumption that all peers have the same ((local peer list) U (global peer list))
                  // and that the message was initialized with `init_prop_msg`
                  // if recp is set
                  else if((route = glob_peer_route(rma->pl, recp, rma->index, NULL))){
                        /*snd_msg(&rma->pl->l_a[route->dir_p[0]], 1, msg_type, buf, msg_sz, recp, name);*/
                        abs_snd_msg(&rma->pl->l_a[route->dir_p[0]], 1, msg_type, 30, msg_sz, recp, name, buf, msg_no++, new_u_id);
                  }
            }
            if(msg_type == PEER_EXIT){
                  printf("user %s has disconnected\n", rma->pl->l_a[rma->index].clnt_info[0]);
                  char* lost_route[rma->pl->gpl->sz];
                  #ifdef DEBUG
                  puts("attempting to remove peer list entry");
                  #endif
                  int lost = pl_remove(rma->pl, rma->index, lost_route);
                  #ifdef DEBUG
                  puts("SUCCESS");
                  #endif
                  for(int i = 0; i < lost; ++i)
                        printf("route to global peer %s has been lost\n", lost_route[i]);
                  return;
            }
            // if we'll be alerting
            if(msg_type == MSG_BLAST || msg_type == MSG_SND){
                  printf("%s%s%s: %s\n", (has_peer(rma->pl, name, -1, NULL) == 1) ? ANSI_BLU : ANSI_GRE, name, ANSI_NON, buf);
            }
            if(msg_type == PEER_PASS){
                  #ifdef DEBUG
                  printf("received a PEER_PASS msg from %s@%i\n", rma->pl->l_a[rma->index].clnt_info[0], rma->pl->l_a[rma->index].u_id);
                  #endif
                  _Bool has_route;
                  struct glob_peer_list_entry* route = glob_peer_route(rma->pl, recp, rma->index, &has_route);
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
                  /*init_prop_msg(rma->pl, PEER_PASS, NULL, 0);*/
                  /*
                   *all we need to do is pass the peer pass along to its recipient. if it's me. stop here
                   *either way, record peer
                   */
                  // if i'm not meant to pass this along
                  /*
                   *if(recp == rma->pl->u_id)continue;
                   *find_peer(rma->pl, recp);
                   */
                  /*abs_snd_msg(&pl->l_a[pl->gpl->gpl[i].dir_p[0]], 1, msg_type, 30, msg_sz, pl->gpl->gpl[i].u_id, pl->name, msg, msg_no++);*/
            }
            memset(buf, 0, msg_sz);
            memset(name, 0, 30);
            pre_msg_no = cur_msg_no;
      }
}

// TODO: free peer list memory
void safe_exit(struct peer_list* pl){
      pl->continuous = 0;
      pthread_mutex_lock(&pl->pl_lock);
      init_prop_msg(pl, 0, PEER_EXIT, NULL, 0, -1);
      // TODO: this should use init_prop_msg
      /*init_prop_msg();*/
      pthread_mutex_unlock(&pl->pl_lock);
}

int main(int argc, char** argv){
      int bound = 1;
      struct peer_list* pl = malloc(sizeof(struct peer_list));
      pl_init(pl, PORTNUM);
      pl->read_func = (void*)&read_messages_pth;
      pl->continuous = 1;
      /*./b nickname search_host*/
      char* sterm = NULL;
      if(argc >= 2)pl->name = argv[1];
      else pl->name = strdup("[anonymous]");
      if(argc >= 3)sterm = argv[2];
      printf("hello %s, welcome to blech\n", pl->name);
      if(sterm){
            /*char* mac;*/
            printf("looking for peer matching search string: \"%s\"\n", sterm);
            // sterm is ip
            int s, p_u_id;
            bound = net_connect(sterm, &s, PORTNUM);
            if(bound == 0){
                  puts("succesfully established a connection");
                  // reading our newly assigned user id
                  #ifdef DEBUG
                  fputs("waiting for my u_id assignment...", stdout);
                  #endif
                  read(s, &pl->u_id, 4);
                  #ifdef DEBUG
                  puts("got it!");
                  #endif
                  send(s, pl->name, 30, 0L);
                  char p_name[30] = {0};
                  #ifdef DEBUG
                  fputs("waiting for new peer's preferred name...", stdout);
                  #endif
                  read(s, p_name, 30);
                  #ifdef DEBUG
                  puts("got it!");
                  #endif
                  #ifdef DEBUG
                  fputs("waiting for new peer's u_id...", stdout);
                  #endif
                  read(s, &p_u_id, 4);
                  #ifdef DEBUG
                  puts("got it!");
                  #endif
                  // printing after we've read preferred name info to assure it's blech network we've connected to
                  printf("you have joined %s~the network~%s\n", ANSI_RED, ANSI_NON);
                  struct sockaddr_in la;
                  bzero(&la, sizeof(struct sockaddr_in));
                  // uhh this chunk doesn't make sense
                  /*mac = inet_ntoa(la.sin_addr);*/
                  pl_add(pl, la, s, strdup(p_name), p_u_id);
                  #ifdef DEBUG
                  printf("added user with name: %s and global peer num: %i\n", pl->l_a[pl->sz-1].clnt_info[0], p_u_id);
                  #endif
            }
            else puts("failed to establish a connection");
      }
      if(bound == 1){
            // only in this instance will we self assign a u_id
            pl->u_id = assign_uid();
            #ifdef DEBUG
            printf("self assigned my u_id %i\n", pl->u_id);
            #endif
            puts("starting in accept-only mode");
      }
      size_t sz = 0;
      ssize_t read;
      char* ln = NULL;
      pthread_t acc_th;
      /*pthread_mutex_lock(&pl->sock_lock);*/
      pthread_create(&acc_th, NULL, (void*)&accept_connections, pl);
      /*pthread_mutex_unlock(&pl->sock_lock);*/
      #ifdef DEBUG
      puts("accept thread created");
      #endif
      puts("blech is ready for connections");
      while(1){
            read = getline(&ln, &sz, stdin);
            ln[--read] = '\0';
            if(read == 1 && *ln == 'q')break;
            if(read == 1 && *ln == 'p'){
                  pl_print(pl);
                  continue;
            }
            if(read > 2 && *ln == 'p' && ln[1] == 'm'){
                  int i = -1;
                  if(!strtoi(ln+3, NULL, &i)){
                        puts("enter a peer # to send a private message");
                        continue;
                  }
                  int msg_code;
                  struct loc_addr_clnt_num* la;
                  int recp = -1;
                  pthread_mutex_lock(&pl->pl_lock);
                  if(i < pl->sz){
                        msg_code = MSG_SND;
                        la = &pl->l_a[i];
                  }
                  else if(i < pl->gpl->sz+pl->sz){
                        msg_code = MSG_PASS;
                        la = &pl->l_a[*pl->gpl->gpl[i-pl->sz].dir_p];
                        recp = pl->gpl->gpl[i-pl->sz].u_id;
                  }
                  else{
                        puts("enter an in range peer number");
                        pthread_mutex_unlock(&pl->pl_lock);
                        continue;
                  }
                  pthread_mutex_unlock(&pl->pl_lock);
                  read = getline(&ln, &sz, stdin);
                  ln[--read] = '\0';
                  /*snd_msg(la, 1, msg_code, ln, read, recp, pl->name);*/
                  abs_snd_msg(la, 1, msg_code, 30, read, recp, pl->name, ln, msg_no++, -1);
                  /*
                   * ssnd_msg(struct loc_addr_clnt_num *la, int n_peers, int msg_type, char *msg, int msg_sz, int recp, char *sndr)
                   *
                   * abs_snd_msg(struct loc_addr_clnt_num *la, int n, int msg_type, int sender_sz, int msg_sz, int recp, char *sender, char *msg, int u_msg_no, int adtnl_int)
                   */
            }
            else snd_txt_to_peers(pl, ln, read);
            printf("%sme%s: \"%s\"\n", ANSI_MGNTA, ANSI_NON, ln);
      }
      safe_exit(pl);
      return 1;
}
