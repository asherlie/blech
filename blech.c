#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <bluetooth/rfcomm.h>

// sends messages and assumes they have been received

/* this code borrows from www.people.csail.mit.edu/albert/bluez-intro/c404.html */
// TODO: have separate get_bdaddr_list, add option for indexed selector to choose name and MAC ADDR
bdaddr_t* get_bdaddr(char* d_name, char** m_name, char** m_addr){
      inquiry_info *ii = NULL;
      int max_rsp, num_rsp;
      int dev_id, sock, len, flags;
      char addr[19] = {0};
      char name[248] = {0};
      dev_id = hci_get_route(NULL);
      sock = hci_open_dev( dev_id );
      if (dev_id < 0 || sock < 0){
            perror("opening socket");
            exit(1);
      }
      len = 8;
      max_rsp = 255;
      flags = IREQ_CACHE_FLUSH;
      ii = (inquiry_info*)malloc(max_rsp*sizeof(inquiry_info));
      num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
      printf("%i devices found\n", num_rsp);
      if(num_rsp < 0)perror("hci_inquiry");
      for(int i = 0; i < num_rsp; ++i){
            ba2str(&(ii+i)->bdaddr, addr);
            memset(name, 0, sizeof(name));
            if(hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name), name, 0) < 0)
            strcpy(name, "[unknown]");
                  if(strcasestr(name, d_name)){
                  printf("found a match: %s  %s\n", addr, name);
                  if(m_name)*m_name = strdup(name);
                  if(m_addr)*m_addr = strdup(addr);
                  return &(ii+i)->bdaddr;
            }
      }
      free(ii);
      close(sock);
      return NULL;
}

void client(){
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char buf[1024] = { 0 };
    int s, clnt, bytes_read;
    socklen_t opt = sizeof(rem_addr);

    // allocate socket
    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    // bind socket to port 1 of the first available 
    // local bluetooth adapter
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = *BDADDR_ANY;
    loc_addr.rc_channel = (uint8_t) 1;
    bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr));

    // put socket into listening mode
    listen(s, 1);

    // accept one connection
    clnt = accept(s, (struct sockaddr *)&rem_addr, &opt);

    ba2str( &rem_addr.rc_bdaddr, buf );
    fprintf(stderr, "accepted connection from %s\n", buf);
    memset(buf, 0, sizeof(buf));

    // read data from the client
    while(getchar() != 'q'){ 
          bytes_read = read(clnt, buf, sizeof(buf));
          if( bytes_read > 0 ) {
              printf("partner: %s\n", buf);
          }
    }
    // close connection
    close(clnt);
    close(s);
}

int main(int argc, char **argv){
      if(argc < 2){
            puts("enter mode or search string");
            return 1;
      }
      if(strncmp(argv[1], "-c", 3) == 0)client();
      else{
            int status = 0;
            #ifndef TEST
            int s;
            char* dname; char* mac;
            bdaddr_t* bd = get_bdaddr(argv[1], &dname, &mac);
            if(!bd)return 1;
            struct sockaddr_rc addr = { 0 };
            /*char dest[18] = "01:23:45:67:89:AB";*/

            // allocate a socket
            s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

            // set the connection parameters (who to connect to)
            addr.rc_family = AF_BLUETOOTH;
            addr.rc_channel = (uint8_t) 1;
            addr.rc_bdaddr = *bd;
            /*str2ba( dest, &addr.rc_bdaddr );*/
            // connect to server
            printf("attempting to connect to %s\n", dname);
            status = connect(s, (struct sockaddr *)&addr, sizeof(addr));
            printf("received status %i\n", status);
            // send a message
            #endif
            if( status == 0 ) {
                  puts("ready to send messages");
                  while(1){
                        char* msg = NULL;
                        size_t sz = 0;
                        unsigned int sl = getline(&msg, &sz, stdin);
                        if(msg[sl-1] == '\n')msg[--sl] = 0;
                        if(sl == 1 && *msg == 'q')break;
                        #ifndef TEST
                        status = write(s, msg, sz);
                        printf("sent message \"%s\" to %s@%s\n", msg, dname, mac);
                        #else
                        printf("%s\n", msg);
                        #endif
                  }
            }
            else perror("uh oh");
            #ifndef TEST
            close(s);
            #endif
      }
}
