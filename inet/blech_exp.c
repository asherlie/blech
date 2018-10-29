#include <string.h>
#include "peer_list.h"
#include "snd.h"

extern int msg_no;

_Bool strtoi(const char* str, unsigned int* ui, int* i){
      char* res;
      unsigned int r = (unsigned int)strtol(str, &res, 10);
      if(*res)return 0;
      if(i)*i = (int)r;
      if(ui)*ui = r;
      return 1;
}

void print_usage(char* p_name){
      printf("usage: %s <nick> [peer address]\n", p_name);
}

void print_i_help(){
      puts("    /h : print this menu\n    /u <filename> : upload file\n    /dl : download file"
      "\n    /sh : share file with another user\n    /pm <peer_no> : send peer_no a private message"
      "\n    /p : print peers\n    /f : print files\n    /q : quit");
}

int main(int argc, char** argv){
      if(argc < 2){
            print_usage(*argv);
            return -1;
      }
      struct peer_list* pl = malloc(sizeof(struct peer_list));
      char* sterm = NULL;
      // will these fields be overwritten when pl_init is called by blech_init?
      if(argc >= 2)pl->name = argv[1];
      else pl->name = strdup("[anonymous]");
      if(argc >= 3)sterm = argv[2];
      printf("hello %s, welcome to blech\nenter \"/h\" at any time for help\n", pl->name);
      // attempts to connect to sterm or starts blech in accept only mode
      blech_init(pl, sterm);
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
                        // TODO: write this
                        if(ln[1] == 's' && ln[2] == 'h'){
                              int u_id = -1, u_fn = -1;
                              puts("enter u_fn followed by u_id");
                              read = getline(&ln, &sz, stdin);
                              ln[--read] = '\0';
                              /*char* u_fn_s = strsep(&ln, " ");*/
                              /*
                               *strtoi(u_fn_s, NULL, &u_fn);
                               *strtoi(ln
                               */
                              strtoi(strsep(&ln, " "), NULL, &u_id);
                              puts(ln);
                              strtoi(ln, NULL, &u_fn);
                              printf("sharing file %i with peer %i\n", u_fn, u_id);
                              if(!file_share(pl, u_id, u_fn))puts("failed to share file");
                              continue;
                        }
                        if(ln[1] == 'd' && ln[2] == 'l'){
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
                                    /*la = &pl->l_a[i];*/
                              else if(i < pl->gpl->sz+pl->sz){
                                    /*la = &pl->l_a[*pl->gpl->gpl[i-pl->sz].dir_p];*/
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
                        continue;
                  }
            }
            else snd_txt_to_peers(pl, ln, read);
            printf("%sme%s: \"%s\"\n", ANSI_MGNTA, ANSI_NON, ln);
      }
      safe_exit(pl);
      return 1;
}
