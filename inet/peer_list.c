#include "peer_list.h"
#include "snd.h"
#include "net.h"

int next_uid = 0;
pthread_mutex_t u_id_lck;

void gpl_init(struct glob_peer_list* gpl){
      gpl->sz = 0;
      gpl->cap = 1;
      gpl->gpl = malloc(sizeof(struct glob_peer_list_entry)*gpl->cap);
}

void resize_buf(int sz, int* cap, int el_sz, void** buf, int factor){
      *cap *= factor;
      void* tmp_buf = malloc(el_sz*(*cap));
      memcpy(tmp_buf, *buf, el_sz*(sz));
      free(*buf);
      *buf = tmp_buf;
}

struct glob_peer_list_entry* gpl_add(struct glob_peer_list* gpl, char* name, int u_id){
      /*next_uid = u_id+1;*/
      set_next_uid(u_id+1);
      #ifdef DEBUG
      printf("adding gpl entry with u_id: %i\n", u_id);
      #endif
      /*pthread_mutex_lock(pl->pl_lock);*/
      if(gpl->sz == gpl->cap)
            resize_buf(gpl->sz, &gpl->cap, sizeof(struct glob_peer_list_entry), (void*)&gpl->gpl, 2);
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
      if(gple->n_dir_p == gple->dir_p_cap)
            resize_buf(gple->n_dir_p, &gple->dir_p_cap, sizeof(int), (void*)&gple->dir_p, 2);
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

pthread_t add_read_thread(struct peer_list* pl){
      pthread_mutex_lock(&pl->rt->r_th_lck);
      if(pl->rt->sz == pl->rt->cap)
            resize_buf(pl->rt->sz, &pl->rt->cap, sizeof(pthread_t), (void*)&pl->rt->th, 2);
      struct read_msg_arg* rma = malloc(sizeof(struct read_msg_arg));
      // TODO: is this a safe assumption?
      // should the scope of the caller's (pl_add) pthread
      // lock be extended past the call to this func
      // to assure no users have been added
      pl->l_a[pl->sz-1].rma = rma;
      rma->index = pl->rt->sz;
      rma->pl = pl;
      pthread_t ptt;
      #pragma GCC diagnostic ignored "-Wuninitialized"
      pl->rt->th[pl->rt->sz++] = ptt;
      pthread_mutex_unlock(&pl->rt->r_th_lck);
      pthread_create(&ptt, NULL, &read_messages_pth, rma);
      pthread_detach(ptt);
      return ptt;
}

void fs_init(struct filesys* fs){
      if(!fs)fs = malloc(sizeof(struct filesys));
      fs->n_files = 0;
      fs->cap = 1;
      fs->files = malloc(sizeof(struct file_acc)*fs->cap);
      fs->storage.sz = 0;
      fs->storage.cap = 1;
      fs->storage.file_chunks = malloc(sizeof(struct fs_block)*fs->storage.cap);
}

// we've been given access to a new file
// we're already aware of all universal file numbers - this function puts a name and u_id_lst to the u_fn
// u_id_lst will be the return value of upload_file
_Bool fs_add_acc(struct filesys* fs, int u_fn, char* fname, int* u_id_lst){
      _Bool ret = 0;
      if(fs->n_files == fs->cap){
            resize_buf(fs->n_files, &fs->cap, sizeof(struct file_acc), (void*)&fs->files, 2);
            ret = 1;
      }
      fs->files[fs->n_files].fname = fname;
      fs->files[fs->n_files].u_fn = u_fn;
      fs->files[fs->n_files++].f_list = u_id_lst;
      return ret;
}

// adds a segment of u_fn to fs->storage, invisible to the peer it's stored in unless shared with her
_Bool fs_add_stor(struct filesys* fs, int u_fn, char* data, int data_sz){
      if(fs->storage.sz == fs->storage.cap)
            resize_buf(fs->storage.sz, &fs->storage.cap, sizeof(struct fs_block), (void*)&fs->storage.file_chunks, 2);
      fs->storage.file_chunks[fs->storage.sz].u_fn = u_fn;
      fs->storage.file_chunks[fs->storage.sz].data_sz = data_sz;
      fs->storage.file_chunks[fs->storage.sz++].data = data;
      return 1;
}

struct fs_block* fs_get_stor(struct filesys* fs, int u_fn){
      for(int i = 0; i < fs->storage.sz; ++i){
            if(fs->storage.file_chunks[i].u_fn == u_fn)
                  return &fs->storage.file_chunks[i];
      }
      return NULL;
}

struct file_acc* fs_get_acc(struct filesys* fs, int u_fn){
      for(int i = 0; i < fs->n_files; ++i){
            if(fs->files[i].u_fn == u_fn)return &fs->files[i];
      }
      return NULL;
}

void fs_free(struct filesys* fs){
      for(int i = 0; i < fs->n_files; ++i){
            free(fs->files->fname);
            free(fs->files->f_list);
      }
      free(fs->files);
      for(int i = 0; i < fs->storage.sz; ++i){
            free(fs->storage.file_chunks[i].data);
      }
      free(fs->storage.file_chunks);
}

struct file_acc* find_file(struct filesys* fs, int u_fn){
      for(int i = 0; i < fs->n_files; ++i){
            if(fs->files[i].u_fn == u_fn)return &fs->files[i];
      }
      return NULL;
}

void fs_print(struct filesys* fs){
      for(int i = 0; i < fs->n_files; ++i){
            printf("%s@%i [", fs->files[i].fname, fs->files[i].u_fn);
            int le = 0;
            for(; fs->files[i].f_list[le+1] != -1; ++le)
                  printf("%i, ", fs->files[i].f_list[le]);
            printf("%i]\n", fs->files[i].f_list[le]);
      }
}

void pl_set_local_sock(struct peer_list* pl, in_addr_t* bind_addr, uint16_t port_num){
      if(pl->sock_set)close(pl->local_sock);
      pl->sock_set = 1;
      struct sockaddr_in loc_addr;
      memset(&loc_addr, 0, sizeof(struct sockaddr_in));
      loc_addr.sin_family = AF_INET;
      loc_addr.sin_port = htons(port_num);
      loc_addr.sin_addr.s_addr = (bind_addr) ? *bind_addr : htonl(INADDR_ANY);
      /*loc_addr.sin_addr.s_addr = inet_addr("~some_ip~");*/
      int s = socket(AF_INET, SOCK_STREAM, 0);
      bind(s, (struct sockaddr*)&loc_addr, sizeof(loc_addr));
      pl->local_sock = s;
      listen(pl->local_sock, 0);
}

void pl_init(struct peer_list* pl){
      fs_init(&pl->file_system);
      pl->gpl = malloc(sizeof(struct glob_peer_list));
      gpl_init(pl->gpl);
      pl->rt = malloc(sizeof(struct read_thread));
      rt_init(pl->rt);
      pl->sz = 0;
      pl->cap = 1;
      pl->l_a = malloc(sizeof(struct loc_addr_clnt_num)*pl->cap);
      pl->sock_set = 0;
      pl->continuous = 1;
      // listening mode
      /*listen(s, 0);*/
      pthread_mutex_t pmu;
      pthread_mutex_init(&pmu, NULL);
      pl->pl_lock = pmu;
      pthread_mutex_t smu;
      pthread_mutex_init(&smu, NULL);
      pl->sock_lock = smu;
      pl->read_th_wait = 0;
}

void rt_init(struct read_thread* rt){
      rt->sz = 0;
      rt->cap = 100;
      rt->th = malloc(sizeof(pthread_t)*rt->cap);
      // TODO: destroy this
      pthread_mutex_init(&rt->r_th_lck, NULL);
}

void pl_add(struct peer_list* pl, struct sockaddr_in la, int clnt_num, char* name, int u_id){
      if(has_peer(pl, NULL, u_id, NULL, NULL, NULL))return;
      set_next_uid(u_id+1);
      pthread_mutex_lock(&pl->pl_lock);
      if(pl->sz == pl->cap)
            resize_buf(pl->sz, &pl->cap, sizeof(struct loc_addr_clnt_num), (void*)&pl->l_a, 2);
      pl->l_a[pl->sz].l_a = la;
      pl->l_a[pl->sz].clnt_info = malloc(sizeof(char*)*2);
      if(!name){
            pl->l_a[pl->sz].clnt_info[0] = malloc(10);
            strcpy(pl->l_a[pl->sz].clnt_info[0], "[unknown]");
      }
      else pl->l_a[pl->sz].clnt_info[0] = name;
      pl->l_a[pl->sz].u_id = u_id;
      pl->l_a[pl->sz].continuous = 1;
      pl->l_a[pl->sz++].clnt_num = clnt_num;
      #ifdef DEBUG
      printf("pl_add: new peer with name: %s has been added\n", pl->l_a[pl->sz-1].clnt_info[0]);
      #endif
      pthread_mutex_unlock(&pl->pl_lock);
      if(pl->read_th_wait){
            #ifdef DEBUG
            puts("waiting for accept_thread to gather info");
            #endif
            while(pl->read_th_wait)usleep(1000);
      }
      #ifdef DEBUG
      puts("info gathered!");
      #endif
      add_read_thread(pl);
}

// if(gpl_i)*gpl_i is set to indices of global peers with broken routes
// gpl_i should be able to acommodate pl->gpl->sz items
// returns number of global peers who i've lost access to
int pl_remove(struct peer_list* pl, int peer_ind, char** gpl_i){
      pthread_mutex_lock(&pl->pl_lock);
      // TODO: resize pl->rt
      pl->l_a[peer_ind].continuous = 0;
      // TODO: safely free rma
      // we no longer need the baggage associated with peer_ind's read thread
      // free(pl->l_a[peer_ind].rma);
      free(pl->l_a[peer_ind].clnt_info[0]);
      free(pl->l_a[peer_ind].clnt_info);
      int gpl_s = 0;
      memmove(pl->l_a+peer_ind, pl->l_a+peer_ind+1, pl->sz-peer_ind-1);
      // adjusting gpl routes
      for(int i = 0; i < pl->gpl->sz; ++i){
            gple_remove_route_entry(&pl->gpl->gpl[i], peer_ind);
            // if gpl->gpl[i] is inaccessible
            if(!pl->gpl->gpl[i].n_dir_p){
                  gpl_remove(pl->gpl, i, 1);
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
      #ifdef DEBUG
      puts("pl free has been called");
      #endif
      pl->continuous = 0;
      /*free(pl->name);*/
      for(; pl->sz; pl_remove(pl, 0, NULL));
      free(pl->l_a);
      // seg fault occurs here
      pthread_mutex_destroy(&pl->pl_lock);
      gpl_free(pl->gpl);
      free(pl->gpl);
      // TODO: fs_free should not be called from within pl_free
      // could cause seg faults
      fs_free(&pl->file_system);
}

// TODO: synch issues?
void gpl_remove(struct glob_peer_list* gpl, int gpl_i, _Bool keep_name){
      for(int i = keep_name; i < 2; ++i)
            free(gpl->gpl[gpl_i].clnt_info[i]);
      free(gpl->gpl[gpl_i].dir_p);
      memmove(gpl->gpl+gpl_i, gpl->gpl+gpl_i+1, gpl->sz-gpl_i-1);
      --gpl->sz;
}

void gpl_free(struct glob_peer_list* gpl){
      for(; gpl->sz; gpl_remove(gpl, 0, 0));
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

// TODO: should u_id's be printed?
void pl_print(struct peer_list* pl){
      /*printf("printing %i local peers and %i global peers\n", pl->sz, pl->gpl->sz);*/
      printf("[%sme%s]: %s@%i\n", ANSI_RED, ANSI_NON, pl->name, pl->u_id);
      for(int i = 0; i < pl->sz; ++i){
            printf("[%slcl%s]%i: %s@%i\n", ANSI_BLU, ANSI_NON, i, pl->l_a[i].clnt_info[0], pl->l_a[i].u_id);
      }
      for(int i = 0; i < pl->gpl->sz; ++i){
            printf("[%sglb%s]%i: %s@%i\n", ANSI_GRE, ANSI_NON, pl->sz+i, pl->gpl->gpl[i].clnt_info[0], pl->gpl->gpl[i].u_id);
      }
}

// if cont, *cont is set to if el is already in my route to u_id
// if gpl_ind, *gpl_ind is set to the gpl index of u_id
struct glob_peer_list_entry* glob_peer_route(struct peer_list* pl, int u_id, int el, _Bool* cont, int* gpl_ind){
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
                  if(gpl_ind)*gpl_ind = i;
                  return &pl->gpl->gpl[i];
            }
      }
      return NULL;
}

/*returns 3 if peer is me, 1 if local peer, 2 if global, 0 else*/
/* if loc_num, *loc_num is set to local peer number of u_id/closest route to global peer */
/* if glob_num, *glob_num is set to global peer number of u_id */
// TODO: this should not rely on name - nicks are not unique
int has_peer(struct peer_list* pl, char* name, int u_id, int* u_id_set, int* loc_num, int* glob_num){
      if(name && strstr(pl->name, name))return 3;
      for(int i = 0; i < pl->sz; ++i)
            if(pl->l_a[i].u_id == u_id || (name && strstr(pl->l_a[i].clnt_info[0], name))){
                  if(u_id_set)*u_id_set = pl->l_a[i].u_id;
                  if(loc_num)*loc_num = i;
                  return 1;
            }
      struct glob_peer_list_entry* gple = NULL;
      if((gple = glob_peer_route(pl, u_id, -1, NULL, glob_num))){
            if(loc_num)*loc_num = *gple->dir_p;
            return 2;
      }
      return 0;
}

void set_next_uid(int val){
      pthread_mutex_lock(&u_id_lck);
      next_uid = val;
      pthread_mutex_unlock(&u_id_lck);
}

int assign_uid(){
      pthread_mutex_lock(&u_id_lck);
      int ret = next_uid++;
      pthread_mutex_unlock(&u_id_lck);
      return ret;
}

// TODO: free peer list memory
void safe_exit(struct peer_list* pl){
      pl->continuous = 0;
      pthread_mutex_lock(&pl->pl_lock);
      init_prop_msg(pl, 0, PEER_EXIT, NULL, 0, pl->u_id);
      pthread_mutex_unlock(&pl->pl_lock);
      free(pl->rt->th);
      free(pl->rt);
      pl_free(pl);
      free(pl);
}

_Bool blech_init(struct peer_list* pl, char* sterm, int portnum){
      pthread_mutex_init(&u_id_lck, NULL);
      pl_init(pl);
      // TODO: self assign ip in local range and auto attempt to connect to others in range
      // net_connect should be called until successful
      /*
       *in_addr_t iad = htonl(inet_addr("1.0.0.0"));
       *printf("%i\n", iad);
       *pl_set_local_sock(pl, &iad, portnum);
       */

      // set local socket to inet
      pl_set_local_sock(pl, NULL, portnum);
      pl->continuous = 1;
      int bound = 1;
      if(sterm){
            printf("looking for peer matching search string: \"%s\"\n", sterm);
            // sterm is ip
            int s, p_u_id;
            bound = net_connect(sterm, &s, portnum);
            if(bound == 0){
                  // TODO: add timeout here
                  puts("succesfully established a connection");
                  // reading our newly assigned user id
                  read(s, &pl->u_id, 4);
                  send(s, pl->name, 30, 0L);
                  char p_name[30] = {0};
                  read(s, p_name, 30);
                  read(s, &p_u_id, 4);
                  // printing after we've read preferred name info to assure it's blech network we've connected to
                  printf("you have joined %s~the network~%s\n", ANSI_RED, ANSI_NON);
                  // TODO: pl_add should take in a sockaddr_in* so i can pass NULL if needed
                  struct sockaddr_in la;
                  bzero(&la, sizeof(struct sockaddr_in));
                  pl_add(pl, la, s, strdup(p_name), p_u_id);
                  #ifdef DEBUG
                  printf("added user with name: %s and global peer num: %i\n", pl->l_a[pl->sz-1].clnt_info[0], p_u_id);
                  #endif
            }
            else puts("failed to establish a connection");
      }
      if(bound == 1){
            // only in this instance will we self assign a u_id
            pl->u_id = assign_uid();
            #ifdef DEBUG
            printf("self assigned my u_id %i\n", pl->u_id);
            #endif
            puts("starting in accept-only mode");
      }
      pthread_t acc_th;
      /*pthread_mutex_lock(&pl->sock_lock);*/
      pthread_create(&acc_th, NULL, &accept_connections, pl);
      /*pthread_mutex_unlock(&pl->sock_lock);*/
      #ifdef DEBUG
      puts("accept thread created");
      #endif
      return !bound;
}

struct loc_addr_clnt_num* find_peer(struct peer_list* pl, int u_id){
      struct loc_addr_clnt_num* ret = NULL;
      pthread_mutex_lock(&pl->pl_lock);
      for(int i = 0; i < pl->sz; ++i)
            if(pl->l_a[i].u_id == u_id)ret = &pl->l_a[i];
      pthread_mutex_unlock(&pl->pl_lock);
      return ret;
}

// TODO: can this be replaced by a clal to glob_peer_route?
int* get_dir_p(struct peer_list* pl, int glob_u_id, int* n){
      struct glob_peer_list_entry* gple = glob_peer_route(pl, glob_u_id, -1, NULL, NULL);
      if(!gple)return NULL;
      *n = gple->n_dir_p;
      return gple->dir_p;
}

int u_id_to_loc_id(struct peer_list* pl, int u_id){
      for(int i = 0; i < pl->sz; ++i){
            if(pl->l_a[i].u_id == u_id)return i;
      }
      return -1;
}
