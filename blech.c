#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <pthread.h>
#include <bluetooth/rfcomm.h>

struct snd_tp_arg{
      _Bool cont;
      int sock;
      char* d_name;
      char* mac;
      /*bdaddr_t */
};

struct loc_addr_clnt_num{
      struct sockaddr_rc l_a;
      int clnt_num; };
// TODO: this should be sorted to allow for binary search
// for easiest shortest path node calculation
struct peer_list{
      /*struct sockaddr* pl;*/
      /*loc_addr* l_a;*/
      struct loc_addr_clnt_num* l_a;
      int cap;
      int sz;
      _Bool continuous;
};
// sends messages and assumes they have been received

/* this code borrows from www.people.csail.mit.edu/albert/bluez-intro/c404.html */
// TODO: have separate get_bdaddr_list, add option for indexed selector to choose name and MAC ADDR
bdaddr_t* get_bdaddr(char* d_name, char** m_name, char** m_addr){
      inquiry_info *ii = NULL;
      int max_rsp, num_rsp;
      int dev_id, sock, len, flags;
      char addr[19] = {0};
      char name[248] = {0};
      dev_id = hci_get_route(NULL);
      sock = hci_open_dev( dev_id );
      if (dev_id < 0 || sock < 0){
            perror("opening socket");
            exit(1);
      }
      len = 8;
      max_rsp = 255;
      flags = IREQ_CACHE_FLUSH;
      ii = (inquiry_info*)malloc(max_rsp*sizeof(inquiry_info));
      num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
      /*printf("%i devices found\n", num_rsp);*/
      if(num_rsp < 0)perror("hci_inquiry");
      for(int i = 0; i < num_rsp; ++i){
            ba2str(&(ii+i)->bdaddr, addr);
            memset(name, 0, sizeof(name));
            if(hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name), name, 0) < 0)
                  strcpy(name, "[unknown]");
            if(strcasestr(name, d_name)){
                  /*printf("found a match: %s  %s\n", addr, name);*/
                  if(m_name)*m_name = strdup(name);
                  if(m_addr)*m_addr = strdup(addr);
                  return &(ii+i)->bdaddr;
            }
      }
      free(ii);
      close(sock);
      return NULL;
}
void snd_to_partner(struct snd_tp_arg* arg){
      while(arg->cont){
      /*while(1){*/
            char* msg = NULL;
            size_t sz = 0;
            unsigned int sl = getline(&msg, &sz, stdin);
            if(msg[sl-1] == '\n')msg[--sl] = 0;
            if(sl == 1 && *msg == 'q')break;
            #ifndef TEST
            write(arg->sock, msg, sz);
            printf("sent message \"%s\" to %s@%s\n", msg, arg->d_name, arg->mac);
            #else
            printf("%s\n", msg);
            #endif
      }
      arg->cont = 0;
      return;
}

/*
 *void flip_q(_Bool* flip){
 *      while(getchar() != 'q');
 *      *flip = 0;
 *      return;
 *}
 */

void pl_init(struct peer_list* pl){
      pl->sz = 0;
      pl->cap = 1;
      pl->l_a = malloc(sizeof(struct loc_addr_clnt_num)*pl->cap);
      pl->continuous = 1;
}

void pl_add(struct peer_list* pl, struct sockaddr_rc la, int clnt_num){
      if(pl->sz == pl->cap){
            pl->cap *= 2;
            struct loc_addr_clnt_num* tmp_l_a = malloc(sizeof(struct loc_addr_clnt_num)*pl->cap);
            memcpy(tmp_l_a, pl->l_a, pl->sz);
            free(pl->l_a);
            pl->l_a = tmp_l_a;
      }
      pl->l_a[pl->sz].l_a = la;
      pl->l_a[pl->sz++].clnt_num = clnt_num;
}

void accept_connections(struct peer_list* pl, int sock, struct sockaddr_rc rem_addr){
      socklen_t opt = sizeof(rem_addr);
      // using this criteria to allow cancelling/timing out of acception
      int clnt;
      char buf[1024] = { 0 };
      while(pl->continuous){
            clnt = accept(sock, (struct sockaddr *)&rem_addr, &opt);
            ba2str(&rem_addr.rc_bdaddr, buf);
            printf("accepted connection from %s\n", buf);
            memset(buf, 0, sizeof(buf));
            pl_add(pl, rem_addr, clnt);
      }
}

struct a_c_arg{
      struct peer_list* pl;
      int sock;
      struct sockaddr_rc rem_addr;
};

void accept_connections_pth(struct a_c_arg* arg){
      accept_connections(arg->pl, arg->sock, arg->rem_addr);
}

void server(){
      struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
      char buf[1024] = { 0 };
      int s, bytes_read;
      // allocate socket
      s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
      // bind socket to port 1 of the first available 
      // local bluetooth adapter
      loc_addr.rc_family = AF_BLUETOOTH;
      loc_addr.rc_bdaddr = *BDADDR_ANY;
      loc_addr.rc_channel = (uint8_t)1;
      bind(s, (struct sockaddr*)&loc_addr, sizeof(loc_addr));
      // put socket in listening mode
      listen(s, 1);
      // accept one connection
      puts("ready for connection");
      // all clients should periodically receive this peer_list 
      struct peer_list* pl = malloc(sizeof(struct peer_list));
      pl_init(pl);
      struct a_c_arg* aca = malloc(sizeof(struct a_c_arg));
      aca->pl = pl; aca->sock = s; aca->rem_addr = rem_addr;
      pthread_t acc_con;
      // TODO: remember to close(pl->l_a[i].clnt_num);
      pthread_create(&acc_con, NULL, (void*)&accept_connections_pth, aca);
      /*clnt = accept(s, (struct sockaddr *)&rem_addr, &opt);*/
      /*
       *ba2str(&rem_addr.rc_bdaddr, buf);
       *printf("accepted connection from %s\n", buf);
       *memset(buf, 0, sizeof(buf));
       */
      // read data from the client
      // make a spearate function rw_loop that takes in an int for client/server number
      // and keeps reading and writing to the server 
      // this can be used in both client and server
      // wait until we have >0 connections
      while(!pl->sz);
      pthread_t snd_thr;
      struct snd_tp_arg* arg = malloc(sizeof(struct snd_tp_arg));
      // shouldn't be sending to sock, pass in peer_list and use pl->a_l.clnt_num
      arg->sock = s; arg->d_name = arg->mac = NULL;
      pthread_create(&snd_thr, NULL, (void*)&snd_to_partner, arg);
      // TODO: read_from_partner() should run in a separate thread
      while(arg->cont){ 
            for(int i = 0; i < pl->sz; ++i){
                  bytes_read = read(pl->l_a[i].clnt_num, buf, sizeof(buf));
                  // TODO: print partner name
                  if(bytes_read > 0)printf("partner: %s\n", buf);
            }
      }
      pl->continuous = 0;
      pthread_join(acc_con, NULL);
      pthread_join(snd_thr, NULL);
      // close connections
      for(int i = 0; i < pl->sz; ++i)close(pl->l_a[i].clnt_num);
      close(s);
}

// user can bind to the name of any node in the network
int bind_to_server(bdaddr_t* bd, char* dname, char* mac){
      int status = 0, s;
      #ifndef TEST
      if(!bd)return 1;
      struct sockaddr_rc addr = { 0 };
      // allocate a socket
      s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
      // set the connection parameters (who to connect to)
      addr.rc_family = AF_BLUETOOTH;
      addr.rc_channel = (uint8_t) 1;
      addr.rc_bdaddr = *bd;
      // connect to server
      /*printf("attempting to connect to %s\n", dname);*/
      status = connect(s, (struct sockaddr *)&addr, sizeof(addr));
      printf("received status %i\n", status);
      // send a message
      #endif
      if(status == 0){
            puts("ready to send messages");
            struct snd_tp_arg arg;
            arg.sock = s; arg.cont = 1; arg.d_name = dname; arg.mac = mac;
            snd_to_partner(&arg);
      }
      else perror("uh oh");
      #ifndef TEST
      close(s);
      #endif
      return 0;
}

// TODO: connect to multiple hosts at once
// a thread should be periodically sending their list of partners to their list of partners
// blech starts in user mode unless a server search string isn't provided 
// or the provided server search string is invalid
int main(int argc, char** argv){
      if(argc >= 2){
            char* dname; char* mac;
            printf("attempting to bind to server matching search string: %s\n", argv[1]);
            bdaddr_t* bd = get_bdaddr(argv[1], &dname, &mac);
            if(bd){
                  printf("attempting to connect to server: %s\n", dname);
                  return bind_to_server(bd, dname, mac);
            }
      }
      puts("no target user provided or search string not found. starting server mode");
      server();
      return 0;
}
