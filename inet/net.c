#include "net.h"
#include "snd.h"

int msg_no = 0;

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

void accept_connections(struct peer_list* pl){
      int clnt;
      char name[248] = {0};
      struct sockaddr_in rem_addr;
      socklen_t opt = sizeof(rem_addr);
      int u_id = -1;
      while(pl->continuous){
            clnt = accept(pl->local_sock, (struct sockaddr *)&rem_addr, &opt);
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
            for(int i = 0; i < pl->sz-1; ++i){
                  // new peer nickname goes in msg field message field
                  usleep(1000);
                  abs_snd_msg(&pl->l_a[pl->sz-1], 1, PEER_PASS, 30, 30, pl->l_a[pl->sz-1].u_id, pl->name, pl->l_a[i].clnt_info[0], msg_no++, pl->l_a[i].u_id);
                  #ifdef DEBUG
                  printf("sent local peer number %i info about peer #%i\n", pl->sz-1, pl->l_a[i].u_id);
                  #endif
            }
            // alerting new peer of current global peers
            #ifdef DEBUG
            printf("sending new info to %i glob\n", pl->gpl->sz);
            #endif
            /*i need to send the new user*/
            for(int i = 0; i < pl->gpl->sz; ++i){
                  // new peer nickname goes in msg field message field
                  usleep(1000);
                  abs_snd_msg(&pl->l_a[pl->sz-1], 1, PEER_PASS, 30, 30, pl->l_a[pl->sz-1].u_id, pl->gpl->gpl[i].clnt_info[0], pl->name, msg_no++, pl->gpl->gpl[i].u_id);
            }
            pthread_mutex_unlock(&pl->pl_lock);
            memset(name, 0, sizeof(name));
            /*memset((char*)addr, 0, sizeof(addr));*/
            /*free(addr);*/
            #ifdef DEBUG
            puts("sharing complete");
            #endif
      }
}

