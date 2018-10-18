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

int main(int argc, char** argv){
      struct peer_list* pl = malloc(sizeof(struct peer_list));
      /*./b nickname search_host*/
      char* sterm = NULL;
      // will these fields be overwritten when pl_init is called by blech_init?
      if(argc >= 2)pl->name = argv[1];
      else pl->name = strdup("[anonymous]");
      if(argc >= 3)sterm = argv[2];
      printf("hello %s, welcome to blech\n", pl->name);
      // attempts to connect to sterm or starts blech in accept only mode
      blech_init(pl, sterm);
      size_t sz = 0;
      ssize_t read;
      char* ln = NULL;
      puts("blech is ready for connections");
      while(1){
            read = getline(&ln, &sz, stdin);
            ln[--read] = '\0';
            if(read == 1 && *ln == 'q')break;
            if(read == 1 && *ln == 'p'){
                  pl_print(pl);
                  continue;
            }
            if(read > 2 && *ln == 'p' && ln[1] == 'm'){
                  int i = -1;
                  if(!strtoi(ln+3, NULL, &i)){
                        puts("enter a peer # to send a private message");
                        continue;
                  }
                  int msg_code;
                  struct loc_addr_clnt_num* la;
                  int recp = -1;
                  pthread_mutex_lock(&pl->pl_lock);
                  if(i < pl->sz){
                        msg_code = MSG_SND;
                        la = &pl->l_a[i];
                  }
                  else if(i < pl->gpl->sz+pl->sz){
                        msg_code = MSG_PASS;
                        la = &pl->l_a[*pl->gpl->gpl[i-pl->sz].dir_p];
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
                  abs_snd_msg(la, 1, msg_code, 30, read, recp, pl->name, ln, msg_no++, -1);
            }
            else snd_txt_to_peers(pl, ln, read);
            printf("%sme%s: \"%s\"\n", ANSI_MGNTA, ANSI_NON, ln);
      }
      safe_exit(pl);
      return 1;
}
