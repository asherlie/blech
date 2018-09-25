#include "peer_list.h"

void gpl_init(struct glob_peer_list* gpl){
      gpl->sz = 0;
      gpl->cap = 1;
      gpl->gpl = malloc(sizeof(struct glob_peer_list_entry)*gpl->cap);
}

void gpl_add(struct glob_peer_list* gpl){
      if(gpl->sz == gpl->cap){
            gpl->cap *= 2;
            struct glob_peer_list_entry* tmp_gpl = malloc(sizeof(struct glob_peer_list_entry)*gpl->cap);
            memcpy(gpl->gpl, tmp_gpl, sizeof(struct glob_peer_list_entry)*gpl->sz);
      }
}

void pl_init(struct peer_list* pl){
      pl->gpl = malloc(sizeof(struct glob_peer_list));
      gpl_init(pl->gpl);
      pl->sz = 0;
      pl->cap = 1;
      pl->l_a = malloc(sizeof(struct loc_addr_clnt_num)*pl->cap);
      pl->continuous = 1;
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
            memcpy(tmp_l_a, pl->l_a, sizeof(struct loc_addr_clnt_num)*pl->sz);
            free(pl->l_a);
            pl->l_a = tmp_l_a;
      }
      pl->l_a[pl->sz].l_a = la;
      pl->l_a[pl->sz].clnt_info = malloc(sizeof(char*)*2);
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

int* compute_global_path(struct peer_list* pl, char* mac){
      for(int i = 0; i < pl->gpl->sz; ++i){
            if(strstr(pl->gpl->gpl[i].clnt_info[1], mac))
                  return pl->gpl->gpl[i].route;
      }
      // this should never happen
      return NULL;
}
