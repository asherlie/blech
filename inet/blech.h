#define MSG_SND   0
#define MSG_PASS  1
#define PEER_PASS 2
#define PEER_EXIT 3
#define MSG_BLAST 4
#define FROM_OTHR 5

struct loc_addr_clnt_num{
      struct sockaddr_in l_a; int clnt_num;
      // [client name, mac]
      char** clnt_info;
      _Bool continuous;
};
