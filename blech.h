struct loc_addr_clnt_num{
      struct sockaddr_rc l_a; int clnt_num;
      // [client name, mac]
      char** clnt_info;
};
