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
            if(hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name), name, 0) < 0)
                  strcpy(name, "[unknown]");
            if(strcasestr(name, d_name)){
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

int snd_msg_to_peers(struct peer_list* pl, char* msg, int msg_sz){
      printf("sending message to %i peers\n", pl->sz);
      for(int i = 0; i < pl->sz ; ++i){
            // assuming already bound
            /*bind_to_bdaddr(&pl->l_a[i].l_a.rc_bdaddr);*/
            send(pl->l_a[i].clnt_num, msg, msg_sz, 0L);
            printf("sent message \"%s\" to peer #%i\n", msg, i);
      }
      return pl->sz;
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
            ba2str(&rem_addr.rc_bdaddr, addr);
            if(hci_read_remote_name(clnt, &rem_addr.rc_bdaddr, sizeof(name), name, 0) < 0)
                  strcpy(name, "[unknown]");
            printf("accepted connection from %s@%s\n", name, addr);
            // DO NOT add the same rem-addr mul times
            pthread_mutex_lock(&pm);
            pl_add(pl, rem_addr, clnt, strdup(name), strdup(addr));
            pthread_mutex_unlock(&pm);
            memset(name, 0, sizeof(name));
            memset(addr, 0, sizeof(addr));
      }
}

void read_messages_pth(struct peer_list* pl){
      listen(pl->local_sock, 0);
      char buf[1024] = { 0 };
      int bytes_read;
      while(pl->continuous){
            if(!pl->sz)usleep(1000);
            for(int i = 0; i < pl->sz; ++i){
                  /*listen(pl->l_a[i].clnt_num, 1);*/
                  bytes_read = read(pl->l_a[i].clnt_num, buf, sizeof(buf));
                  if(bytes_read > 0)printf("%s: %s\n", pl->l_a[i].clnt_info[0], buf);
                  memset(buf, 0, bytes_read);
            }
      }
}

int main(int argc, char** argv){
      int bound = 1;
      struct peer_list* pl = malloc(sizeof(struct peer_list));
      pl_init(pl);
      pl->continuous = 1;
      if(argc >= 2){
            char* dname; char* mac;
            printf("looking for peer matching search string: \"%s\"\n", argv[1]);
            bdaddr_t* bd = get_bdaddr(argv[1], &dname, &mac);
            if(bd){
                  printf("attempting to connect to peer: %s...", dname);
                  int s;
                  bound = bind_to_bdaddr(bd, &s);
                  if(bound == 0){
                        puts("successfully established a connection");
                        struct sockaddr_rc la;
                        /*pl_add(pl, la, bound, dname, mac);*/
                        pl_add(pl, la, s, dname, mac);
                  }
                  else puts("failed to establish a connection");
            }
            else puts("no peers found");
      }
      if(bound == 1)puts("starting in accept-only mode");
      size_t sz = 0;
      ssize_t read;
      char* ln = NULL;
      pthread_t acc_th, rea_th;
      pthread_create(&acc_th, NULL, (void*)&accept_connections, pl);
      pthread_create(&rea_th, NULL, (void*)&read_messages_pth, pl);
      puts("blech is ready for connections");
      while(1){
            read = getline(&ln, &sz, stdin);
            ln[--read] = '\0';
            if(read == 1 && *ln == 'q')break;
            if(read == 1 && *ln == 'p'){
                  pl_print(pl);
                  continue;
            }
            snd_msg_to_peers(pl, ln, read);
      }
      // set continuous to 0 and clean up the read thread
      // we can't join the accept or read threads because they're waiting for connections/data
      pl->continuous = 0;
      return 1;
}
