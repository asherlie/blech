#include "peer_list.h"

void gpl_init(struct glob_peer_list* gpl){
      gpl->sz = 0;
      gpl->cap = 1;
      gpl->gpl = malloc(sizeof(struct glob_peer_list_entry)*gpl->cap);
}

struct glob_peer_list_entry* gpl_add(struct glob_peer_list* gpl, char* name, char* mac){
      if(gpl->sz == gpl->cap){
            gpl->cap *= 2;
            struct glob_peer_list_entry* tmp_gpl = malloc(sizeof(struct glob_peer_list_entry)*gpl->cap);
            memcpy(tmp_gpl, gpl->gpl, sizeof(struct glob_peer_list_entry)*gpl->sz);
            free(gpl->gpl);
            gpl->gpl = tmp_gpl;
      }
      gpl->gpl[gpl->sz].clnt_info = malloc(sizeof(char*)*2);
      gpl->gpl[gpl->sz].clnt_info[0] = name;
      gpl->gpl[gpl->sz].clnt_info[1] = mac;
      gpl->gpl[gpl->sz].route_c = 20;
      gpl->gpl[gpl->sz].route_s = 0;
      gpl->gpl[gpl->sz].route = malloc(sizeof(int)*gpl->gpl[gpl->sz].route_c);
      ++gpl->sz;
      return &gpl->gpl[gpl->sz-1];
}

void gple_add_route_entry(struct glob_peer_list_entry* gple, int rel_no){
      if(gple->route_s == gple->route_c){
            gple->route_c *= 2;
            int* tmp_route = malloc(sizeof(int)*gple->route_c);
            memcpy(tmp_route, gple->route, sizeof(int)*gple->route_s);
            free(gple->route);
            gple->route = tmp_route;
      }
      gple->route[gple->route_s++] = rel_no;
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
      pl->local_mac = malloc(sizeof(char)*18);
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
      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 100000;
      // TODO: possibly reset each timeout every time a peer is added
      // timeouts should be (1/pl->sz)*1e6
      setsockopt(clnt_num, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
      #ifdef DEBUG
      printf("%s@%s sock has been set to timeout after %i usecs\n", name, mac, tv.tv_usec);
      #endif
}

void pl_print(struct peer_list* pl){
      printf("printing %i local peers and %i global peers\n", pl->sz, pl->gpl->sz);
      for(int i = 0; i < pl->sz; ++i){
            printf("[%slcl%s]%i: %s@%s\n", ANSI_BLU, ANSI_NON, i, pl->l_a[i].clnt_info[0], pl->l_a[i].clnt_info[1]);
      }
      for(int i = 0; i < pl->gpl->sz; ++i){
            printf("[%sglb%s]%i: %s@%s\n", ANSI_GRE, ANSI_NON, pl->sz+i, pl->gpl->gpl[i].clnt_info[0], pl->gpl->gpl[i].clnt_info[1]);
      }
}

int next_in_line(struct peer_list* pl, char* mac){
      for(int i = 0; i < pl->gpl->sz; ++i){
            if(strstr(pl->gpl->gpl[i].clnt_info[1], mac))
                  return pl->gpl->gpl[i].dir_p;
      }
      return -1;
}

/*returns 3 if peer is me, 1 if local peer, 2 if global, 0 else*/
int has_peer(struct peer_list* pl, char* mac){
      if(strstr(pl->local_mac, mac))return 3;
      for(int i = 0; i < pl->sz; ++i)
            if(strstr(pl->l_a[i].clnt_info[1], mac))
                  return 1;
      if(next_in_line(pl, mac) > -1)return 2;
      return 0;
}
