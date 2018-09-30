#include <string.h>
#include "peer_list.h"

_Bool strtoi(const char* str, unsigned int* ui, int* i){
      char* res;
      unsigned int r = (unsigned int)strtol(str, &res, 10);
      if(*res)return 0;
      if(i)*i = (int)r;
      if(ui)*ui = r;
      return 1;
}

/* this code borrows from www.people.csail.mit.edu/albert/bluez-intro/c404.html */
bdaddr_t* get_bdaddr(char* d_name, char** m_name, char** m_addr){
      inquiry_info *ii = NULL;
      int max_rsp, num_rsp;
      int dev_id, sock, len, flags;
      char addr[19] = {0};
      char name[248] = {0};
      dev_id = hci_get_route(NULL);
      sock = hci_open_dev(dev_id);
      if (dev_id < 0 || sock < 0){
            perror("opening socket");
            exit(1);
      }
      // hci_inquiry takes 1.28*len seconds
      len = 8;
      /*len = 5;*/
      max_rsp = 255;
      flags = IREQ_CACHE_FLUSH;
      ii = (inquiry_info*)malloc(max_rsp*sizeof(inquiry_info));
      num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
      if(num_rsp < 0)perror("hci_inquiry");
      for(int i = 0; i < num_rsp; ++i){
            ba2str(&(ii+i)->bdaddr, addr);
            memset(name, 0, sizeof(name));
            if(hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name), name, 0) >= 0 && strcasestr(name, d_name)){
                  if(m_name)*m_name = strdup(name);
                  if(m_addr)*m_addr = strdup(addr);
                  // memory leak is intentional for the time being to keep this bdaddr_t* valid
                  // TODO: fix this memory leak
                  return &(ii+i)->bdaddr;
            }
      }
      free(ii);
      close(sock);
      return NULL;
}

int bind_to_bdaddr(bdaddr_t* bd, int* sock){
      int status = 0, s;
      if(!bd)return -1;
      struct sockaddr_rc addr = { 0 };
      s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
      // set the connection parameters - who to connect to
      addr.rc_family = AF_BLUETOOTH;
      addr.rc_channel = (uint8_t) 1;
      addr.rc_bdaddr = *bd;
      // connect to peer
      status = connect(s, (struct sockaddr *)&addr, sizeof(addr));
      *sock = s;
      return status;
}

struct loc_addr_clnt_num* find_peer(struct peer_list* pl, char* mac){
      for(int i = 0; i < pl->sz; ++i)
            if(strstr(pl->l_a[i].clnt_info[1], mac))return &pl->l_a[i];
      return NULL;
}

// sndr is a 30 byte string
int snd_msg(struct loc_addr_clnt_num* la, int n_peers, int msg_type, char* msg, int msg_sz, char* recp, char* sndr){
      #ifdef DEBUG
      printf("sending message of type: %i to %i peers recp param: %s\n", msg_type, n_peers, recp);
      #endif
      for(int i = 0; i < n_peers; ++i){
            // assuming already bound
            /*bind_to_bdaddr(&pl->l_a[i].l_a.rc_bdaddr);*/
            send(la[i].clnt_num, &msg_type, 4, 0L);
            // MSG_PASS indicates that a message is being sent indirectly and that we want to act as a middleperson
            // it will not be printed by read_messages_pth
            // from main: if not found in pl, snd_msg ith MSG_PASS and recp set to mac of end recp
            // we can assume that snd_msg will only be invoked with PASS if recp is not in pl or la
            if(msg_type == MSG_PASS){
                  if(!recp){
                        puts("blech::snd_msg: msg_type == MSG_PASS and recp is unspecified");
                        return -1;
                  }
                  /*MAC takes up 17 chars*/
                  // MAC is a good thing to have because it's guaranteed unique
                  send(la[i].clnt_num, recp, 18, 0L);
                  send(la[i].clnt_num, sndr, 30, 0L);
            }
            // msg_sz is used to indicate peer number in a PEER_PASS
            // each index in `route` refers to local peer number
            // PP, MAC is sent
            if(msg_type == PEER_PASS){
                  // sending local pl->l_a[index] to help other nodes construct routes for each glob_peer_list_entry
                  #ifdef DEBUG
                  puts("executing peer pass");
                  #endif
                  // this is obselete with current implementation
                  // TODO: implement passing along of full path
                  /*send(la[i].clnt_num, &msg_sz, 4, 0L);*/
                  // when a PEER_PASS is sent, msg must be a hostname - for now assuming sizeof 30, allow for msg_sz later
                  send(la[i].clnt_num, recp, 18, 0L);
                  send(la[i].clnt_num, msg, 30, 0L);
            }
            else send(la[i].clnt_num, msg, msg_sz, 0L);
            // TODO: possibly send recp info in blast mode, which is being passed in anyway
            // to know when to stop passing - are users aware of their mac addresses
            // as of now, passing will continue until the message is sent to someone with just one peer
            // which will be problematic when it comes to circular graphs
            /*if(msg_type == MSG_SND || msg_type == MSG_BLAST)printf("%sme%s: \"%s\"\n", ANSI_BLU, ANSI_NON, msg);*/
            // recp will be NULL unless it's the last step of a MSG_PASS
            if(msg_type == MSG_SND){
                  // we don't want to print anything if a message is passing through us
                  if(!recp)printf("%sme%s: \"%s\"\n", ANSI_BLU, ANSI_NON, msg);
            }
            /*printf("sent message \"%s\" to peer #%i\n", msg, i);*/
      }
      return n_peers;
}

int snd_txt_to_peers(struct peer_list* pl, char* msg, int msg_sz){
      // TODO: snd_txt_to_peers should use a mixture between PEER_PASS and MSG_PASS
      // TODO: should PEER_PASS and MSG_PASS be combined?
      printf("%sme%s: \"%s\"\n", ANSI_BLU, ANSI_NON, msg);
      return snd_msg(pl->l_a, pl->sz, MSG_BLAST, msg, msg_sz, NULL, NULL);
}

void accept_connections(struct peer_list* pl){
      int clnt;
      char addr[19] = {0};
      char name[248] = {0};
      struct sockaddr_rc rem_addr;
      socklen_t opt = sizeof(rem_addr);
      pthread_mutex_t pm;
      pthread_mutex_init(&pm, NULL);
      while(pl->continuous){
            clnt = accept(pl->local_sock, (struct sockaddr *)&rem_addr, &opt);
            // as soon as a new client is added, wait for them to send their desired name
            read(clnt, name, 30);
            /*send(clnt, pl->name, 30....*/
            send(clnt, pl->name, 30, 0L);
            ba2str(&rem_addr.rc_bdaddr, addr);
            /*
             *if(hci_read_remote_name(clnt, &rem_addr.rc_bdaddr, sizeof(name), name, 0) < 0)
             *      strcpy(name, "[unknown]");
             */
            #ifdef DEBUG
            printf("accepted connection from %s@%s\n", name, addr);
            #endif
            printf("new [%slcl%s] user: %s@%s has joined %s~the network~%s\n", ANSI_BLU, ANSI_NON, name, addr, ANSI_RED, ANSI_NON);
            // DO NOT add the same rem-addr mul times
            pthread_mutex_lock(&pm);
            pl_add(pl, rem_addr, clnt, strdup(name), strdup(addr));
            // each time a peer is added, we need to send updated peer information to all peers
            pthread_mutex_unlock(&pm);
            // alert my local peers 
            /*sz-1 so as not to send most recent peer*/
            /*TODO: is this correct?*/
            #ifdef DEBUG
            puts("executing peer pass from accept_connections");
            #endif
            snd_msg(pl->l_a, pl->sz-1, PEER_PASS, name, 30, strdup(addr), NULL);
            /*snd_msg(pl->l_a, pl->sz, PEER_PASS, NULL, 0, strdup(addr));*/
            // TODO: alert global peers - shouldn't have to because my local peers will alert their locals, etc.
            // TODO: pass existing peers along to new peer
            #ifdef DEBUG
            printf("sending %i peer passes to new peer from accept connections\n", pl->sz);
            #endif
            for(int i = 0; i < pl->sz-1; ++i){
                  snd_msg(&pl->l_a[pl->sz-1], 1, PEER_PASS, pl->l_a[i].clnt_info[0], 30, pl->l_a[i].clnt_info[1], NULL);
                  usleep(1000);
            }
            memset(name, 0, sizeof(name));
            memset(addr, 0, sizeof(addr));
      }
}

// TODO: MSG_PASS messages should be printed by read_messages_pth with a flag to propogate mass messages
void read_messages_pth(struct read_msg_arg* rma){
/*void read_messages_pth(struct loc_addr_clnt_num* la){*/
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
            if(msg_type == PEER_PASS || msg_type == MSG_PASS){
                  bytes_read = read(la->clnt_num, recp, 18);
                  bytes_read = read(la->clnt_num, name, 30);
                  la_r = find_peer(pl, recp);
            }
            if(msg_type == PEER_PASS){
                  #ifdef DEBUG
                  printf("received a PEER_PASS msg from %s@%s\n", la->clnt_info[0], la->clnt_info[1]);
                  #endif
                  /*int route_ind = -1;*/
                  /*read(pl->l_a[i].clnt_num, &route_ind, 1);*/
                  // pl->l_a[i] just sent me an integer representing the index of a peer they just added
                  /*read(pl->l_a[i].clnt_num, recp, 18);*/
                  // if this is our first local peer, record recp as our own local mac str
                  // TODO: figure out what to do with this - as of now info about me is sent
                  // this is not strict enough
                  // this passes when we make one connection and doesn't let us accept new peer info
                  /*if(pl->sz == 1 && !pl->gpl->sz)strncpy(pl->local_mac, recp, 18);*/
                  // if we've already recvd this information, don't record or pass it along again
                  // if pl->sz == 2 && pl->gpl is 0, they're sending my info back to me as an initial PEER_PASS
                  // if pl->sz == 1, we have nowhere to pass peers because we've just received this PEER_PASS from peer 0
                  /*if(pl->sz == 1 || has_peer(pl, recp))continue;*/
                  _Bool has_route;
                  struct glob_peer_list_entry* route = glob_peer_route(pl, recp, rma->index, &has_route);
                  /*if(pl->sz == 1 || (route && has_route))continue;*/
                  if(route && has_route)continue;
                  #ifdef DEBUG
                  if(!route)puts("new user found");
                  else if(!has_route)puts("new route to existing user found");
                  #endif
                  // if we have the global peer already but this PEER_PASS is coming from a different local peer
                  // we'll want to record this new possible route in gpl->dir_p
                  if(!route)printf("new [%sglb%s] peer: %s@%s has joined %s~the network~%s\n", ANSI_GRE, ANSI_NON, name, recp, ANSI_RED, ANSI_NON);
                  /*snd_msg(pl->l_a, pl->sz, PEER_PASS, NULL, 0, recp);*/
                  // doing some quick maths to avoid resending to our sender
                  snd_msg(pl->l_a, rma->index, PEER_PASS, name, 30, recp, NULL);
                  snd_msg(pl->l_a+rma->index+1, pl->sz-rma->index+1, PEER_PASS, name, 30, recp, NULL);
                  // all we need to know for route is who sent us this peer information
                  if(route)gple_add_route_entry(route, rma->index);
                  else gple_add_route_entry(gpl_add(pl->gpl, name, recp), rma->index);
                  // TODO: implement full route recording
                  continue;
            }
            bytes_read = read(la->clnt_num, buf, sizeof(buf));
            if((msg_type == FROM_OTHR || msg_type == MSG_SND || msg_type == MSG_BLAST)){
                  if(bytes_read <= 0){
                        puts("cannot print message");
                        continue;
                  }
                  // print T
                  #ifdef DEBUG
                  if(msg_type == FROM_OTHR)printf("received FROM_OTHR message from \"%s\"\n", name);
                  #endif
                  printf("%s: %s\n", (msg_type == FROM_OTHR) ? name : la->clnt_info[0], buf);
            }
            if(msg_type == MSG_SND || msg_type == MSG_PASS || msg_type == MSG_BLAST){
                  // if we finally found recp
                  // la_r will only be !NULL if MSG_PASS - if we have the recipient as a local peer
                  if(la_r){
                        /*snd_msg(la_r, 1, MSG_SND, buf, bytes_read, orig_sender);*/
                        snd_msg(la_r, 1, FROM_OTHR, buf, bytes_read, NULL, name);
                  }
                  else if(msg_type == MSG_PASS || msg_type == MSG_BLAST){
                        /*msg_pass needs an original sender entry to pass along so that recp knows who sent them msg*/
                        /*snd_msg(pl->l_a, pl->sz, MSG_PASS, buf, bytes_read, recp);*/
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
                        // TODO: which is accurate?
                        /*snd_msg(pl->l_a+rma->index+1, pl->sz-rma->index+1, msg_type, buf, bytes_read, recp);*/
                        snd_msg(pl->l_a+rma->index+1, pl->sz-rma->index-1, msg_type, buf, bytes_read, recp, name);
                  }
                  memset(buf, 0, bytes_read);
            }
            memset(name, 0, 30);
      }
}

int main(int argc, char** argv){
      int bound = 1;
      struct peer_list* pl = malloc(sizeof(struct peer_list));
      pl_init(pl);
      pl->read_func = (void*)&read_messages_pth;
      pl->continuous = 1;
      /*./b nickname search_hjost*/
      char* sterm = NULL;
      if(argc >= 2)pl->name = argv[1]; //sterm = argv[1];
      else pl->name = strdup("[anonymous]");
      if(argc >= 3)sterm = argv[2];
      printf("hello %s, welcome to blech\n", pl->name);
      if(sterm){
            char* dname; char* mac;
            printf("looking for peer matching search string: \"%s\"\n", sterm);
            bdaddr_t* bd = get_bdaddr(sterm, &dname, &mac);
            if(bd){
                  printf("attempting to connect to peer with hostname: %s...", dname);
                  int s;
                  bound = bind_to_bdaddr(bd, &s);
                  if(bound == 0){
                        puts("succesfully established a connection");
                        printf("you have joined %s~the network~%s\n", ANSI_RED, ANSI_NON);
                        send(s, pl->name, 30, 0L);
                        char p_name[30] = {0};
                        read(s, p_name, 30);
                        struct sockaddr_rc la;
                        /*pl_add(pl, la, s, dname, mac);*/
                        pl_add(pl, la, s, p_name, mac);
                  }
                  else puts("failed to establish a connection");
            }
            else puts("no peers found");
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
                        continue;
                  }
                  read = getline(&ln, &sz, stdin);
                  ln[--read] = '\0';
                  /*snd_msg(la, 1, msg_code, ln, read, recp, nick);*/
                  snd_msg(la, 1, msg_code, ln, read, recp, pl->name);
            }
            else snd_txt_to_peers(pl, ln, read);
      }
      // we can't join the accept or read threads because they're waiting for connections/data
      // TODO: look into setting timeout for accept, read and joining threads
      pl->continuous = 0;
      return 1;
}
