#define MSG_SND   0
#define MSG_PASS  1
#define PEER_PASS 2
#define PEER_EXIT 3

struct loc_addr_clnt_num{
      struct sockaddr_rc l_a; int clnt_num;
      // [client name, mac]
      char** clnt_info;
      _Bool continuous;
};
