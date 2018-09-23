#include "peer_list.h"

void pl_init(struct peer_list* pl){
      pl->sz = 0;
      pl->cap = 1;
      pl->l_a = malloc(sizeof(struct loc_addr_clnt_num)*pl->cap);
      pl->continuous = 1;
      pl->lock = 0;
      struct sockaddr_rc loc_addr = {0};
      loc_addr.rc_family = AF_BLUETOOTH;
      loc_addr.rc_bdaddr = *BDADDR_ANY;
      loc_addr.rc_channel = (uint8_t)1;
      int s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
      bind(s, (struct sockaddr*)&loc_addr, sizeof(loc_addr));
      // listening mode
      // TODO: remain in listen mode until it's time to send a message
      listen(s, 0);
      pl->local_sock = s;
}

void pl_add(struct peer_list* pl, struct sockaddr_rc la, int clnt_num, char* name, char* mac){
      if(pl->sz == pl->cap){
            pl->cap *= 2;
            struct loc_addr_clnt_num* tmp_l_a = malloc(sizeof(struct loc_addr_clnt_num)*pl->cap);
            memcpy(tmp_l_a, pl->l_a, pl->sz);
            free(pl->l_a);
            pl->l_a = tmp_l_a;
      }
      pl->l_a[pl->sz].l_a = la;
      pl->l_a[pl->sz].clnt_info = malloc(sizeof(char*)*2);
      /*char* unk = malloc*/
      if(!name){
            pl->l_a[pl->sz].clnt_info[0] = malloc(10);
            strcpy(pl->l_a[pl->sz].clnt_info[0], "[unknown]");
      }
      else pl->l_a[pl->sz].clnt_info[0] = name;
      if(!mac){
            pl->l_a[pl->sz].clnt_info[1] = malloc(10);
            strcpy(pl->l_a[pl->sz].clnt_info[1], "[unknown]");
      }
      else pl->l_a[pl->sz].clnt_info[1] = mac;
      pl->l_a[pl->sz++].clnt_num = clnt_num;
}

void pl_print(struct peer_list* pl){
      printf("printing %i peers\n", pl->sz);
      for(int i = 0; i < pl->sz; ++i){
            printf("%i: %s@%s\n", i, pl->l_a[i].clnt_info[0], pl->l_a[i].clnt_info[1]);
      }
}
