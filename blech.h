struct a_c_arg{
      struct peer_list* pl;
      int sock;
      struct sockaddr_rc rem_addr;
};

struct loc_addr_clnt_num{
      struct sockaddr_rc l_a; int clnt_num;
      // stores client name, mac 
      char** clnt_info;
};
