#include <string.h>
#include "peer_list.h"

#define PORTNUM 2010

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

struct loc_addr_clnt_num* find_peer(struct peer_list* pl, char* mac){
      struct loc_addr_clnt_num* ret = NULL;
      pthread_mutex_lock(&pl->pl_lock);
      for(int i = 0; i < pl->sz; ++i)
            if(strstr(pl->l_a[i].clnt_info[1], mac))ret = &pl->l_a[i];
      pthread_mutex_unlock(&pl->pl_lock);
      return ret;
}

// if *_sz == 0, entry will not be sent
_Bool abs_snd_msg(struct loc_addr_clnt_num* la, int n, int msg_type, int recp_sz, int sender_sz, int msg_sz, char* recp, char* sender, char* msg){
      _Bool ret = 1;
      for(int i = 0; i < n; ++i){
            ret = (send(la[i].clnt_num, &msg_type, 4, 0L) == 4);
            if(recp_sz)ret = (send(la[i].clnt_num, recp, 18, 0L) == 18) && ret;
            if(sender_sz)ret = (send(la[i].clnt_num, sender, 30, 0L) == 30) && ret;
            if(msg_sz)ret = (send(la[i].clnt_num, msg, msg_sz, 0L) == msg_sz) && ret;
      }
      return ret;
}

// sndr is always a 30 byte string
_Bool snd_msg(struct loc_addr_clnt_num* la, int n_peers, int msg_type, char* msg, int msg_sz, char* recp, char* sndr){
      #ifdef DEBUG
      printf("sending message of type: %i to %i peers recp param: %s\n", msg_type, n_peers, recp);
      #endif
      switch(msg_type){
            case MSG_PASS : return abs_snd_msg(la, n_peers, MSG_PASS, 18, 30, msg_sz, recp, sndr, msg);
            case MSG_BLAST: return abs_snd_msg(la, n_peers, MSG_BLAST, 0, 30, msg_sz, NULL, sndr, msg);
            case PEER_PASS: return abs_snd_msg(la, n_peers, PEER_PASS, 18, 0, 30, recp, NULL, msg);
            case FROM_OTHR: return abs_snd_msg(la, n_peers, FROM_OTHR, 0, 30, msg_sz, NULL, sndr, msg);
            case MSG_SND  : return abs_snd_msg(la, n_peers, MSG_SND, 0, 0, msg_sz, NULL, NULL, msg);
            case PEER_EXIT: return abs_snd_msg(la, n_peers, PEER_EXIT, 0, 0, 0, NULL, NULL, NULL);
      }
      return 0;
}

int snd_txt_to_peers(struct peer_list* pl, char* msg, int msg_sz){
      pthread_mutex_lock(&pl->pl_lock);
      int ret = snd_msg(pl->l_a, pl->sz, MSG_BLAST, msg, msg_sz, NULL, pl->name);
      pthread_mutex_unlock(&pl->pl_lock);
      return ret;
}

void accept_connections(struct peer_list* pl){
      int clnt;
      char* addr = NULL;
      char name[248] = {0};
      struct sockaddr_in rem_addr;
      socklen_t opt = sizeof(rem_addr);
      while(pl->continuous){
            clnt = accept(pl->local_sock, (struct sockaddr *)&rem_addr, &opt);
            // as soon as a new client is added, wait for them to send their desired name
            read(clnt, name, 30);
            send(clnt, pl->name, 30, 0L);
            addr = inet_ntoa(rem_addr.sin_addr);
            #ifdef DEBUG
            printf("accepted connection from %s@%s\n", name, addr);
            #endif
            printf("new [%slcl%s] user: %s@%s has joined %s~the network~%s\n", ANSI_BLU, ANSI_NON, name, addr, ANSI_RED, ANSI_NON);
            // DO NOT add the same rem-addr mul times
            pl_add(pl, rem_addr, clnt, strdup(name), strdup(addr));
            // each time a peer is added, we need to send updated peer information to all peers
            // alert my local peers 
            // sz-1 so as not to send most recent peer
            #ifdef DEBUG
            puts("executing peer pass from accept_connections");
            #endif
            snd_msg(pl->l_a, pl->sz-1, PEER_PASS, name, 30, strdup(addr), NULL);
            #ifdef DEBUG
            printf("sending %i peer passes to new peer from accept connections\n", pl->sz);
            #endif
            pthread_mutex_lock(&pl->pl_lock);
            for(int i = 0; i < pl->sz-1; ++i){
                  snd_msg(&pl->l_a[pl->sz-1], 1, PEER_PASS, pl->l_a[i].clnt_info[0], 30, pl->l_a[i].clnt_info[1], NULL);
                  usleep(1000);
            }
            pthread_mutex_unlock(&pl->pl_lock);
            memset(name, 0, sizeof(name));
            /*memset((char*)addr, 0, sizeof(addr));*/
            /*free(addr);*/
      }
}

// TODO: MSG_PASS messages should be printed by read_messages_pth with a flag to propogate mass messages
void read_messages_pth(struct read_msg_arg* rma){
      #ifdef DEBUG
      puts("READ MESSAGE THREAD STARTED");
      #endif
      char buf[1024] = {0};
      char recp[18] = {0};
      char name[30] = {0};
      int msg_type = -1;
      int bytes_read;
      struct loc_addr_clnt_num* la_r = NULL;
      struct peer_list* pl = rma->pl;
      struct loc_addr_clnt_num* la = &pl->l_a[rma->index];
      while(la->continuous){
            /*listen(pl->l_a[i].clnt_num, 1);*/
            // first reading message type byte
            read(la->clnt_num, &msg_type, 4);
            if(msg_type == PEER_EXIT){
                  printf("user %s has disconnected\n", rma->pl->l_a[rma->index].clnt_info[0]);
                  return;
            }
            if(msg_type == PEER_PASS || msg_type == MSG_PASS || msg_type == MSG_BLAST){
                  if(msg_type != MSG_BLAST){
                        bytes_read = read(la->clnt_num, recp, 18);
                        la_r = find_peer(pl, recp);
                  }
                  bytes_read = read(la->clnt_num, name, 30);
            }
            if(msg_type == PEER_PASS){
                  #ifdef DEBUG
                  printf("received a PEER_PASS msg from %s@%s\n", la->clnt_info[0], la->clnt_info[1]);
                  #endif
                  _Bool has_route;
                  struct glob_peer_list_entry* route = glob_peer_route(pl, recp, rma->index, &has_route);
                  if(route && has_route)continue;
                  #ifdef DEBUG
                  if(!route)puts("new user found");
                  else if(!has_route)puts("new route to existing user found");
                  #endif
                  // if we have the global peer already but this PEER_PASS is coming from a different local peer
                  // we'll want to record this new possible route in gpl->dir_p
                  if(!route)printf("new [%sglb%s] peer: %s@%s has joined %s~the network~%s\n", ANSI_GRE, ANSI_NON, name, recp, ANSI_RED, ANSI_NON);
                  // doing some quick maths to avoid resending to our sender
                  pthread_mutex_lock(&pl->pl_lock);
                  snd_msg(pl->l_a, rma->index, PEER_PASS, name, 30, recp, NULL);
                  snd_msg(pl->l_a+rma->index+1, pl->sz-rma->index+1, PEER_PASS, name, 30, recp, NULL);
                  pthread_mutex_unlock(&pl->pl_lock);
                  // all we need to know for route is who sent us this peer information
                  if(route)gple_add_route_entry(route, rma->index);
                  else gple_add_route_entry(gpl_add(pl->gpl, name, recp), rma->index);
                  // TODO: implement full route recording
                  continue;
            }
            bytes_read = read(la->clnt_num, buf, sizeof(buf));
            if((msg_type == FROM_OTHR || msg_type == MSG_SND || msg_type == MSG_BLAST)){
                  if(bytes_read <= 0){
                        // TODO remove peer from peer list
                        // adjust peer list by removing global peers who are no longer accessible
                        // TODO: solve synchronization issues with pl
                        printf("could not print message from %s\n", rma->pl->l_a[rma->index].clnt_info[0]);
                        // we should be able to safely exit this read loop if our index is no longer connected
                        continue;
                  }
                  #ifdef DEBUG
                  if(msg_type == FROM_OTHR)printf("received FROM_OTHR message from \"%s\"\n", name);
                  #endif
                  // are msg blasts accurate? is from_other used even with blasts to local peers
                  printf("%s%s%s: %s\n", (msg_type == MSG_SND) ? ANSI_BLU : ANSI_GRE, (msg_type == FROM_OTHR || msg_type == MSG_BLAST) ? name : la->clnt_info[0], ANSI_NON, buf);
            }
            if(msg_type == MSG_SND || msg_type == MSG_PASS || msg_type == MSG_BLAST){
                  // if we finally found recp
                  // la_r will only be !NULL if MSG_PASS - if we have the recipient as a local peer
                  if(la_r)snd_msg(la_r, 1, FROM_OTHR, buf, bytes_read, NULL, name);
                  else if(msg_type == MSG_PASS || msg_type == MSG_BLAST){
                        #ifdef DEBUG
                        printf("read index: %i, n local peers: %i\n", rma->index, pl->sz);
                        puts("sending first half of pass or blast messages");
                        printf("snd_msg(pl->l_a, %i, %i, buf, bytes, recp)\n", rma->index, msg_type);
                        #endif
                        snd_msg(pl->l_a, rma->index, msg_type, buf, bytes_read, recp, name);
                        #ifdef DEBUG
                        puts("sending second half of pass or blast messages");
                        printf("snd_msg(pl->l_a+%i, %i, %i, buf, bytes, recp)\n", rma->index+1, pl->sz-rma->index-1, msg_type);
                        #endif
                        pthread_mutex_lock(&pl->pl_lock);
                        snd_msg(pl->l_a+rma->index+1, pl->sz-rma->index-1, msg_type, buf, bytes_read, recp, name);
                        pthread_mutex_unlock(&pl->pl_lock);
                  }
            }
            memset(buf, 0, bytes_read);
            memset(name, 0, 30);
      }
}

// TODO: free peer list memory
void safe_exit(struct peer_list* pl){
      pl->continuous = 0;
      pthread_mutex_lock(&pl->pl_lock);
      snd_msg(pl->l_a, pl->sz, PEER_EXIT, NULL, 0, NULL, NULL);
      pthread_mutex_unlock(&pl->pl_lock);
}

int main(int argc, char** argv){
      int bound = 1;
      struct peer_list* pl = malloc(sizeof(struct peer_list));
      pl_init(pl, PORTNUM);
      pl->read_func = (void*)&read_messages_pth;
      pl->continuous = 1;
      /*./b nickname search_hjost*/
      char* sterm = NULL;
      if(argc >= 2)pl->name = argv[1];
      else pl->name = strdup("[anonymous]");
      if(argc >= 3)sterm = argv[2];
      printf("hello %s, welcome to blech\n", pl->name);
      if(sterm){
            char* mac;
            printf("looking for peer matching search string: \"%s\"\n", sterm);
            // sterm is ip
            int s;
            bound = net_connect(sterm, &s, PORTNUM);
            if(bound == 0){
                  puts("succesfully established a connection");
                  printf("you have joined %s~the network~%s\n", ANSI_RED, ANSI_NON);
                  send(s, pl->name, 30, 0L);
                  char p_name[30] = {0};
                  read(s, p_name, 30);
                  struct sockaddr_in la;
                  bzero(&la, sizeof(struct sockaddr_in));
                  // uhh this chunk doesn't make sense
                  mac = inet_ntoa(la.sin_addr);
                  pl_add(pl, la, s, p_name, mac);
            }
            else puts("failed to establish a connection");
      }
      if(bound == 1)puts("starting in accept-only mode");
      size_t sz = 0;
      ssize_t read;
      char* ln = NULL;
      pthread_t acc_th;
      pthread_create(&acc_th, NULL, (void*)&accept_connections, pl);
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
                  char* recp = NULL;
                  pthread_mutex_lock(&pl->pl_lock);
                  if(i < pl->sz){
                        msg_code = MSG_SND;
                        la = &pl->l_a[i];
                  }
                  else if(i < pl->gpl->sz+pl->sz){
                        msg_code = MSG_PASS;
                        la = &pl->l_a[*pl->gpl->gpl[i-pl->sz].dir_p];
                        recp = pl->gpl->gpl[i-pl->sz].clnt_info[1];
                  }
                  else{
                        puts("enter an in range peer number");
                        pthread_mutex_unlock(&pl->pl_lock);
                        continue;
                  }
                  pthread_mutex_unlock(&pl->pl_lock);
                  read = getline(&ln, &sz, stdin);
                  ln[--read] = '\0';
                  snd_msg(la, 1, msg_code, ln, read, recp, pl->name);
            }
            else snd_txt_to_peers(pl, ln, read);
            printf("%sme%s: \"%s\"\n", ANSI_MGNTA, ANSI_NON, ln);
      }
      safe_exit(pl);
      return 1;
}
