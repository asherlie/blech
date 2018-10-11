#include "peer_list.h"

int next_uid = 0;

void gpl_init(struct glob_peer_list* gpl){
      gpl->sz = 0;
      gpl->cap = 1;
      gpl->gpl = malloc(sizeof(struct glob_peer_list_entry)*gpl->cap);
}

struct glob_peer_list_entry* gpl_add(struct glob_peer_list* gpl, char* name, int u_id){
      next_uid = u_id+1;
      #ifdef DEBUG
      printf("adding gpl entry with u_id: %i\n", u_id);
      #endif
      /*pthread_mutex_lock(pl->pl_lock);*/
      if(gpl->sz == gpl->cap){
            gpl->cap *= 2;
            struct glob_peer_list_entry* tmp_gpl = malloc(sizeof(struct glob_peer_list_entry)*gpl->cap);
            memcpy(tmp_gpl, gpl->gpl, sizeof(struct glob_peer_list_entry)*gpl->sz);
            free(gpl->gpl);
            gpl->gpl = tmp_gpl;
      }
      gpl->gpl[gpl->sz].clnt_info = malloc(sizeof(char*)*2);
      gpl->gpl[gpl->sz].clnt_info[0] = name;
      gpl->gpl[gpl->sz].u_id = u_id;
      gpl->gpl[gpl->sz].dir_p_cap = 20;
      gpl->gpl[gpl->sz].n_dir_p = 0;
      gpl->gpl[gpl->sz].dir_p = malloc(sizeof(int)*gpl->gpl[gpl->sz].dir_p_cap);
      ++gpl->sz;
      /*pthread_mutex_unlock(pl->pl_lock);*/
      return &gpl->gpl[gpl->sz-1];
}

// TODO: fix synchronization issues
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

// TODO: fix synchronization issues
// as of now, should only be used within pl_remove to resolve synchronization issues
// returns 1 if item was removed
_Bool gple_remove_route_entry(struct glob_peer_list_entry* gple, int rel_no){
      for(int i = 0; i < gple->n_dir_p; ++i){
            if(gple->dir_p[i] == rel_no){
                  memmove(gple->dir_p+i, gple->dir_p+i+1, gple->n_dir_p-i-1);
                  --gple->n_dir_p;
                  return 1;
            }
      }
      return 0;
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
      pthread_detach(ptt);
      pl->rt->th[pl->rt->sz++] = ptt;
      return ptt;
}

void pl_init(struct peer_list* pl, uint16_t port_num){
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
      loc_addr.sin_port = htons(port_num);
      int s = socket(AF_INET, SOCK_STREAM, 0);
      bind(s, (struct sockaddr*)&loc_addr, sizeof(loc_addr));
      // listening mode
      listen(s, 0);
      pl->local_sock = s;
      listen(pl->local_sock, 0);
      pl->local_mac = malloc(sizeof(char)*18);
      pthread_mutex_t pmu;
      pthread_mutex_init(&pmu, NULL);
      pl->pl_lock = pmu;
}

void rt_init(struct read_thread* rt){
      rt->sz = 0;
      rt->cap = 100;
      rt->th = malloc(sizeof(pthread_t)*rt->cap);
}

void pl_add(struct peer_list* pl, struct sockaddr_in la, int clnt_num, char* name, int u_id){
      next_uid = u_id+1;
      pthread_mutex_lock(&pl->pl_lock);
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
      pl->l_a[pl->sz].u_id = u_id;
      pl->l_a[pl->sz].continuous = 1;
      // made obsolete by in_glob_route
      /*pl->l_a[pl->sz].in_route = 0;*/
      pl->l_a[pl->sz++].clnt_num = clnt_num;
      // TODO:
      /*create a new thread to read and keep it in pl->r_th*/
      #ifdef DEBUG
      printf("pl_add: new peer with name: %s has been added\n", pl->l_a[pl->sz-1].clnt_info[0]);
      #endif
      pthread_mutex_unlock(&pl->pl_lock);
      add_read_thread(pl, pl->read_func);
}

// if(gpl_i)*gpl_i is set to indices of global peers with broken routes
// gpl_i should be able to acommodate pl->gpl->sz items
// returns number of global peers who i've lost access to
int pl_remove(struct peer_list* pl, int peer_ind, char** gpl_i){
      pthread_mutex_lock(&pl->pl_lock);
      pl->l_a[peer_ind].continuous = 0;
      // this is seg faulting
      free(pl->l_a[peer_ind].clnt_info[0]);
      /*free(pl->l_a[peer_ind].clnt_info[1]);*/
      int gpl_s = 0;
      memmove(pl->l_a+peer_ind, pl->l_a+peer_ind+1, pl->sz-peer_ind-1);
      // adjusting gpl routes
      /*int tmp_route = -1;*/
      for(int i = 0; i < pl->gpl->sz; ++i){
            gple_remove_route_entry(&pl->gpl->gpl[i], peer_ind);
            // if gpl->gpl[i] is inaccessible
            if(!pl->gpl->gpl[i].n_dir_p){
                  gpl_remove(pl->gpl, i);
                  if(gpl_i)gpl_i[gpl_s] = pl->gpl->gpl[i].clnt_info[0];
                  ++gpl_s;
            }
            for(int j = 0; j < pl->gpl->gpl->n_dir_p; ++j){
                  // decrement
                  if(pl->gpl->gpl->dir_p[j] > peer_ind){
                        --pl->gpl->gpl->dir_p[j];
                  }
            }
      }
      /*
       *[1,2,3,4,5]
       *removing 3
       *move lst+ind, lst+ind+1, pl->sz-ind-1
       *move lst+2  , lst+3,     5-2-1
       */
      --pl->sz;
      pthread_mutex_unlock(&pl->pl_lock);
      return gpl_s;
}

void pl_free(struct peer_list* pl){
      pl->continuous = 0;
      free(pl->name);
      /*free(pl->local_mac);*/
      for(; pl->sz; pl_remove(pl, 0, NULL));
      pthread_mutex_destroy(&pl->pl_lock);
      gpl_free(pl->gpl);
}

// TODO: synch issues?
void gpl_remove(struct glob_peer_list* gpl, int gpl_i){
      free(gpl->gpl[gpl_i].clnt_info[0]);
      free(gpl->gpl[gpl_i].clnt_info[1]);
      free(gpl->gpl[gpl_i].dir_p);
      memmove(gpl->gpl+gpl_i, gpl->gpl+gpl_i+1, gpl->sz-gpl_i-1);
      --gpl->sz;
}

void gpl_free(struct glob_peer_list* gpl){
      for(; gpl->sz; gpl_remove(gpl, 0));
      free(gpl->gpl);
}

_Bool in_glob_route(struct peer_list* pl, int pl_ind){
      for(int i = 0; i < pl->gpl->sz; ++i){
            for(int j = 0; j < pl->gpl->gpl[i].n_dir_p; ++j){
                  if(pl->gpl->gpl[i].dir_p[j] == pl_ind)return 1;
            }
      }
      return 0;
}

void pl_print(struct peer_list* pl){
      printf("printing %i local peers and %i global peers\n", pl->sz, pl->gpl->sz);
      for(int i = 0; i < pl->sz; ++i){
            printf("[%slcl%s]%i: %s@%i\n", ANSI_BLU, ANSI_NON, i, pl->l_a[i].clnt_info[0], pl->l_a[i].u_id);
      }
      for(int i = 0; i < pl->gpl->sz; ++i){
            printf("[%sglb%s]%i: %s@%i\n", ANSI_GRE, ANSI_NON, pl->sz+i, pl->gpl->gpl[i].clnt_info[0], pl->gpl->gpl[i].u_id);
      }
}

// if cont, *cont is set to if el is already in my route to mac
struct glob_peer_list_entry* glob_peer_route(struct peer_list* pl, int u_id, int el, _Bool* cont){
      for(int i = 0; i < pl->gpl->sz; ++i){
            if(pl->gpl->gpl[i].u_id == u_id){
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
// TODO: this should not rely on name - nicks are not unique
int has_peer(struct peer_list* pl, char* name, int u_id){
      if(strstr(pl->name, name))return 3;
      for(int i = 0; i < pl->sz; ++i)
            if(strstr(pl->l_a[i].clnt_info[0], name))
                  return 1;
      if(glob_peer_route(pl, u_id, -1, NULL))return 2;
      return 0;
}

int assign_uid(){
      return next_uid++;
}
