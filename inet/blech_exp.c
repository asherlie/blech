#include <string.h>
#include "peer_list.h"
#include "snd.h"

extern int msg_no;
extern int next_ufn;

_Bool strtoi(const char* str, unsigned int* ui, int* i){
      char* res;
      unsigned int r = (unsigned int)strtol(str, &res, 10);
      if(*res)return 0;
      if(i)*i = (int)r;
      if(ui)*ui = r;
      return 1;
}

void print_usage(char* p_name){
      printf("usage: %s <nick> [peer address] [-p port number]\n", p_name);
}

void print_i_help(){
      puts("    /h : print this menu\n    /u <filename> : upload file\n    /dl : download file"
      "\n    /sh : share file with another user\n    /pm <peer_no> : send peer_no a private message"
      "\n    /p : print peers\n    /f : print files\n    /q : quit");
}

int main(int argc, char** argv){
      // port number defaults to 2012
      int portnum = 2012;
      /*_Bool ind_skip[argc]; memset(ind_skip, 0, argc);*/
      int pset_ind = -1;
      // to limit scope of tmp_i
     {
      int tmp_i;
      for(int i = 0; i < argc; ++i)
            if(*argv[i] == '-' && argv[i][1] == 'p' && argc > i+1 && strtoi(argv[i+1], NULL, &tmp_i)){
                  pset_ind = i;
                  portnum = tmp_i;
                  printf("port number has been set to: %i\n", portnum);
                  break;
            }
     }
      if(argc < 2 || (argc < 4 && pset_ind != -1)){
            print_usage(*argv);
            return -1;
      }
      struct peer_list* pl = malloc(sizeof(struct peer_list));
      // set name to first arg unless -p <port> is first and second arg
      int pl_ni = (pset_ind == 0) ? 3 : 1;
      pl->name = argv[pl_ni];
      // set search term to second arg unless -p <port> is second and third arg
      char* sterm = NULL;
      if((argc >= 3 && pset_ind == -1) || (argc >= 5 && pset_ind != -1))
            sterm = (pset_ind == -1) ? argv[2] : ((pl_ni+1 < pset_ind+2) ? argv[pl_ni+1] : argv[pset_ind+2]);
      printf("hello %s, welcome to blech\nenter \"/h\" at any time for help\n", pl->name);
      // attempts to connect to sterm or starts blech in accept only mode
      blech_init(pl, sterm, portnum);
      size_t sz = 0;
      ssize_t read;
      char* ln = NULL;
      puts("blech is ready for connections");
      while(1){
            read = getline(&ln, &sz, stdin);
            ln[--read] = '\0';
            if(read > 1 && *ln == '/'){
                  if(read == 2 && ln[1] == 'h'){
                        print_i_help();
                        continue;
                  }
                  if(read >= 3){
                        // grant access to file
                        if(ln[1] == 's' && ln[2] == 'h'){
                              int u_id = -1, u_fn = -1;
                              puts("enter u_fn followed by u_id");
                              read = getline(&ln, &sz, stdin);
                              ln[--read] = '\0';
                              char* lnsep = ln;
                              char* u_fn_s = strsep(&lnsep, " ");
                              if(!strtoi(u_fn_s, NULL, &u_fn)){
                                    puts("enter a valid u_fn");
                                    continue;
                              }
                              if(!lnsep || !strtoi(lnsep, NULL, &u_id)){
                                    puts("enter a valid u_id");
                                    continue;
                              }
                              printf("sharing file %i with peer %i\n", u_fn, u_id);
                              if(!file_share(pl, u_id, u_fn))puts("failed to share file");
                              continue;
                        }
                        if(ln[1] == 'd' && ln[2] == 'l'){
                              // TODO: implement this
                              // puts("enter u_fn to download followed by filename to save to"); 
                              /*void download_file(struct peer_list* pl, int u_fn, char* dl_fname){*/
                              puts(ln+3);
                              char* u_fn_s_e = strchr(ln+3, ' ');
                              // setting whitespace to NUL char so ln+3 will be our u_fn
                              int u_fn = -1;
                              if(!strtoi(ln+3, NULL, &u_fn) || (u_fn < 0 || u_fn >= next_ufn)){
                                    puts("enter a valid u_fn");
                                    continue;
                              }
                              *u_fn_s_e = 0;
                              download_file(pl, 0, "fi.dl");
                              continue;
                        }
                        if(ln[1] == 'u' && ln[2] == ' '){
                              if(upload_file(pl, strdup(ln+3)))puts("file has been uploaded");
                              else puts("file could not be uploaded");
                              continue;
                        }
                        if(ln[1] == 'p' && ln[2] == 'm'){
                              int i = -1;
                              if(!strtoi(ln+4, NULL, &i)){
                                    puts("enter a peer # to send a private message");
                                    continue;
                              }
                              int recp = -1;
                              pthread_mutex_lock(&pl->pl_lock);
                              if(i < pl->sz)
                                    recp = pl->l_a[i].u_id;
                              else if(i < pl->gpl->sz+pl->sz){
                                    recp = pl->gpl->gpl[i-pl->sz].u_id;
                              }
                              else{
                                    puts("enter an in range peer number");
                                    pthread_mutex_unlock(&pl->pl_lock);
                                    continue;
                              }
                              pthread_mutex_unlock(&pl->pl_lock);
                              read = getline(&ln, &sz, stdin);
                              ln[--read] = '\0';
                              snd_pm(pl, ln, read, recp);
                        }
                  }
                  if(ln[1] == 'q')break;
                  if(ln[1] == 'p'){
                        pl_print(pl);
                        continue;
                  }
                  if(ln[1] == 'f'){
                        fs_print(&pl->file_system);
                        if(!pl->file_system.n_files)puts("you do not have access to any files");
                        continue;
                  }
            }
            else snd_txt_to_peers(pl, ln, read);
            printf("%sme%s: \"%s\"\n", ANSI_MGNTA, ANSI_NON, ln);
      }
      free(ln);
      safe_exit(pl);
      return 1;
}
