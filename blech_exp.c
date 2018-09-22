#include <string.h>
#include "peer_list.h"

/* this code borrows from www.people.csail.mit.edu/albert/bluez-intro/c404.html */
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
                  if(m_name)*m_name = strdup(name);
                  if(m_addr)*m_addr = strdup(addr);
                  return &(ii+i)->bdaddr;
            }
      }
      free(ii);
      close(sock);
      return NULL;
}

// TODO: name and mac addr should be stored in struct loc_addr_clnt_num
// this function should be obsolete
// TODO: delete this
void get_name_mac(int sock, bdaddr_t* bdaddr, char** name, char** mac){
      char* nm = malloc(248*sizeof(char));
      char* mc = malloc(248*sizeof(char));
      hci_read_remote_name(sock, bdaddr, 248, nm, 0);
      ba2str(bdaddr, mc);
      *name = nm;
      *mac = mc;
}

// this doesn't have to run in a thread
// the main thread can take input from getline
// and based on input will relegate to snd_msg_to_peerso
// there will be a background thread for accepting new connections
// which will lock when one is found so nothing fishy happens
// with iteration thru peer_list
int snd_msg_to_peers(struct snd_tp_arg* arg){
      printf("sending message to %i peers\n", arg->pl->sz);
      for(int i = 0; i < arg->pl->sz ; ++i){
            // assuming already bound
            /*bind_to_server(&arg->pl->l_a[i].l_a.rc_bdaddr, NULL, NULL, 0);*/
            send(arg->pl->l_a[i].clnt_num, arg->msg, arg->msg_sz, 0L);
            printf("sent message \"%s\" to %s@%s\n", arg->msg, arg->pl->l_a[i].clnt_info[0], arg->pl->l_a[i].clnt_info[1]);
      }
      return arg->pl->sz;
}

void accept_connections(struct peer_list* pl, struct sockaddr_rc rem_addr){
      struct sockaddr_rc loc_addr = {0};
      loc_addr.rc_family = AF_BLUETOOTH;
      loc_addr.rc_bdaddr = *BDADDR_ANY;
      loc_addr.rc_channel = (uint8_t)1;
      int s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
      bind(s, (struct sockaddr*)&loc_addr, sizeof(loc_addr));
      // listening mode
      // TODO: remain in listen mode until it's time to send a message
      listen(s, 1);
      puts("ready for connection");
      socklen_t opt = sizeof(rem_addr);
      int clnt;
      char buf[1024] = { 0 };
      pthread_mutex_t pm;
      pthread_mutex_init(&pm, NULL);
      while(pl->continuous){
            clnt = accept(s, (struct sockaddr *)&rem_addr, &opt);
            ba2str(&rem_addr.rc_bdaddr, buf);
            printf("accepted connection from %s\n", buf);
            memset(buf, 0, sizeof(buf));
            // is this name or mac?
            // DO NOT add the same rem-addr mul times
            pthread_mutex_lock(&pm);
            pl_add(pl, rem_addr, clnt, strdup(buf), NULL);
            pthread_mutex_unlock(&pm);
      }

}

void accept_connections_pth(struct a_c_arg* arg){
      accept_connections(arg->pl, arg->rem_addr);
}

int bind_to_server(bdaddr_t* bd){
      int status = 0, s;
      if(!bd)return -1;
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
      return status;
}

int main(int argc, char** argv){
      int bound = 1;
      struct peer_list* pl = malloc(sizeof(struct peer_list));
      pl_init(pl);
      pl->continuous = 1;
      if(argc >= 2){
            char* dname; char* mac;
            printf("looking for server matching search string: %s\n", argv[1]);
            bdaddr_t* bd = get_bdaddr(argv[1], &dname, &mac);
            if(bd){
                  printf("attempting to connect to server: %s\n", dname);
                  bound = bind_to_server(bd);
                  if(bound != -1){
                        struct sockaddr_rc la;
                        pl_add(pl, la, bound, dname, mac);
                  }
            }
            else puts("no server found");
      }
      if(bound == 1)puts("starting in server-only mode");
      size_t sz = 0;
      ssize_t read;
      char* ln = NULL;
      // TODO: aca should only contain pl
      struct a_c_arg* aca = malloc(sizeof(struct a_c_arg));
      aca->pl = pl;
      pthread_t acc_th;
      pthread_create(&acc_th, NULL, (void*)&accept_connections_pth, aca);
      struct snd_tp_arg snd_arg;
      snd_arg.pl = pl;
      while(1){
            read = getline(&ln, &sz, stdin);
            ln[--read] = '\0';
            if(!read || (read == 1 && *ln == 'q'))break;
            snd_arg.msg = ln; snd_arg.msg_sz = read;
            snd_msg_to_peers(&snd_arg);
      }
      return 1;
}
