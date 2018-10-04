#include "peer_list.h"

#define PORTNUM 2010

void gpl_init(struct glob_peer_list* gpl){
      gpl->sz = 0;
      gpl->cap = 1;
      gpl->gpl = malloc(sizeof(struct glob_peer_list_entry)*gpl->cap);
}

struct glob_peer_list_entry* gpl_add(struct glob_peer_list* gpl, char* name, char* mac){
      #ifdef DEBUG
      printf("adding gpl entry with mac: %s\n", mac);
      #endif
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
      gpl->gpl[gpl->sz].dir_p_cap = 20;
      gpl->gpl[gpl->sz].n_dir_p = 0;
      gpl->gpl[gpl->sz].dir_p = malloc(sizeof(int)*gpl->gpl[gpl->sz].dir_p_cap);
      ++gpl->sz;
      return &gpl->gpl[gpl->sz-1];
}

void gple_add_route_entry(struct glob_peer_list_entry* gple, int rel_no){
      if(gple->n_dir_p == gple->dir_p_cap){
            gple->dir_p_cap *= 2;
            int* tmp_route = malloc(sizeof(int)*gple->dir_p_cap);
            memcpy(tmp_route, gple->dir_p, sizeof(int)*gple->n_dir_p);
            free(gple->dir_p);
            gple->dir_p = tmp_route;
      }
      gple->dir_p[gple->n_dir_p++] = rel_no;
}

pthread_t add_read_thread(struct peer_list* pl, void *(*read_th_fnc) (void *)){
      if(pl->rt->sz == pl->rt->cap){
            pl->rt->cap *= 2;
            pthread_t* tmp_rt = malloc(sizeof(pthread_t)*pl->rt->cap);
            memcpy(tmp_rt, pl->rt->th, sizeof(pthread_t)*pl->rt->sz);
            free(pl->rt->th);
            pl->rt->th = tmp_rt;
      }
      struct read_msg_arg* rma = malloc(sizeof(struct read_msg_arg));
      rma->index = pl->rt->sz;
      rma->pl = pl;
      pthread_t ptt;
      pthread_create(&ptt, NULL, read_th_fnc, rma);
      pl->rt->th[pl->rt->sz++] = ptt;
      return ptt;
}

void pl_init(struct peer_list* pl){
      pl->gpl = malloc(sizeof(struct glob_peer_list));
      gpl_init(pl->gpl);
      pl->rt = malloc(sizeof(struct read_thread));
      rt_init(pl->rt);
      pl->sz = 0;
      pl->cap = 1;
      pl->l_a = malloc(sizeof(struct loc_addr_clnt_num)*pl->cap);
      pl->continuous = 1;
      struct sockaddr_in loc_addr;
      bzero(&loc_addr, sizeof(struct sockaddr_in));
      loc_addr.sin_family = AF_INET;
      loc_addr.sin_addr.s_addr = htonl(INADDR_ANY);
      loc_addr.sin_port = htons(PORTNUM);
      int s = socket(AF_INET, SOCK_STREAM, 0);
      bind(s, (struct sockaddr*)&loc_addr, sizeof(loc_addr));
      // listening mode
      listen(s, 0);
      pl->local_sock = s;
      listen(pl->local_sock, 0);
      pl->local_mac = malloc(sizeof(char)*18);
}

void rt_init(struct read_thread* rt){
      rt->sz = 0;
      rt->cap = 100;
      rt->th = malloc(sizeof(pthread_t)*rt->cap);
}

void pl_add(struct peer_list* pl, struct sockaddr_in la, int clnt_num, char* name, char* mac){
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
      pl->l_a[pl->sz].continuous = 1;
      pl->l_a[pl->sz++].clnt_num = clnt_num;
      // TODO:
      /*create a new thread to read and keep it in pl->r_th*/
      add_read_thread(pl, pl->read_func);
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

// if cont, *cont is set to if el is already in my route to mac
struct glob_peer_list_entry* glob_peer_route(struct peer_list* pl, char* mac, int el, _Bool* cont){
      for(int i = 0; i < pl->gpl->sz; ++i){
            if(strstr(pl->gpl->gpl[i].clnt_info[1], mac)){
                  if(cont){
                        *cont = 0;
                        for(int j = 0; j < pl->gpl->gpl[i].n_dir_p; ++j)
                              if(pl->gpl->gpl[i].dir_p[j] == el){
                                    *cont = 1;
                                    /*break;*/
                              }
                  }
                  return &pl->gpl->gpl[i];
            }
      }
      return NULL;
}

/*returns 3 if peer is me, 1 if local peer, 2 if global, 0 else*/
int has_peer(struct peer_list* pl, char* mac){
      if(strstr(pl->local_mac, mac))return 3;
      for(int i = 0; i < pl->sz; ++i)
            if(strstr(pl->l_a[i].clnt_info[1], mac))
                  return 1;
      if(glob_peer_route(pl, mac, -1, NULL))return 2;
      return 0;
}
