#include "common.h"

#ifdef __WIN32__
  void knickknack_init_sockets();
#endif

knickknack_object* knickknack_ext_new_socket(int domain, int type, int protocol);
knickknack_object* knickknack_ext_gethostbyname(knickknack_object* name);
int knickknack_ext_connect_socket(knickknack_object* socket, int domain, knickknack_object* host, int port);
int knickknack_ext_send_socket(knickknack_object* socket, knickknack_object* payload, int flags);
knickknack_object* knickknack_ext_ioctl(knickknack_object* socket, int cmd);
knickknack_object* knickknack_ext_recv(knickknack_object* socket, int length, int flags);