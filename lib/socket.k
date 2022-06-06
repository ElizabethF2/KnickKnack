export AF_INET
export AF_INET6
export SOCK_STREAM
export SOCK_DGRAM

export new
export connect
export send
# ioctl, recv_nonblocking, listen (needs std support)
export recv
export close
export closed

AF_INET:; return 2
AF_INET6:; return 23
SOCK_STREAM:; return 1
SOCK_DGRAM:; return 2

new:; func_args domain, type
  this = std_new_dict()
  if this == 0 goto _end1

  std_set_primitive(this, "domain", domain)

  sock = ext_new_socket(domain, type, 0)
  if sock == 0 goto _end1

  std_set(this, "sock", sock)
  _end1:
  return this


connect:; func_args this, host, port
  sock = std_get(this, "sock")
  domain = std_get(this, "domain")
  host = ext_gethostbyname(host)
  r = ext_connect_socket(sock, domain, host, port)
  return r


send:; func_args this, payload
  sock = std_get(this, "sock")
  r = ext_send_socket(sock, payload, 0)
  return r


recv:; func_args this, length
  sock = std_get(this, "sock")
  r = ext_recv(sock, length, 0)
  return r


close:; func_args this
  std_remove(this, "sock")
  return


closed:; func_args this
  r = std_exists(this, "sock")
  return r
