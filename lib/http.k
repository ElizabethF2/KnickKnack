import "socket.k" as socket

export test
export parse_url

export std_init
std_init:
  AF_INET = socket.AF_INET()
  SOCK_STREAM = socket.SOCK_STREAM()
  return

parse_url:; func_args url
  parts = std_new_dict()
  
  # Isolate protocol
  i = std_call2("string.k", "index", url, "://")
  if_not i != -1 goto _else1
    s = std_call3("collections.k", "slice", url, 0, i)
    std_set(parts, "protocol", s)
    start = i + 3
    goto _end1
  _else1:
    std_set(parts, "protocol", "http")
    start = 0
  _end1:
  
  # Isolate path and save host and port together
  i = std_call3("string.k", "index_from", url, "/", start)
  if_not i != -1 goto _else2
    host_and_port = std_call3("collections.k", "slice", url, start, i)
    start = i
    i = std_size(url)
    s = std_call3("collections.k", "slice", url, start, i)
    std_set(parts, "path", s)
    goto _end2
  _else2:
    i = std_size(url)
    host_and_port = std_call3("collections.k", "slice", url, start, i)
    std_set(parts, "path", "/")
  _end2:
  s = std_call3("collections.k", "slice", url, start, i)
  
  # Split host and port
  i = std_call3("string.k", "index", host_and_port, ":")
  if_not i != -1 goto _else3
    s = std_call3("collections.k", "slice", host_and_port, 0, i)
    std_set(parts, "host", s)
    start = i + 1
    i = std_size(host_and_port)
    s = std_call3("collections.k", "slice", host_and_port, start, i)
    i = std_call1("convert.k", "string_to_int", s)
    std_set_primitive(parts, "port", i)
    goto _end3
  _else3:
    std_set(parts, "host", host_and_port)
    std_set_primitive(parts, "port", 80)
  _end3:

  return parts

test:; func_args url
  parsed = parse_url(url)
  host = std_get(parsed, "host")
  port = std_get(parsed, "port")
  path = std_get(parsed, "path")

  sock = socket.new(AF_INET, SOCK_STREAM)

  socket.connect(sock, host, port)

  msg = std_new_string()
  std_append(msg, "GET ")
  std_append(msg, path)
  std_append(msg, " HTTP/1.0\r\n")
  std_append(msg, "Host: ")
  std_append(msg, host)
  std_append(msg, "\r\n")
  std_append(msg, "User-Agent: Nintendo DS\r\n\r\n")

  std_print("MSG:\n")
  std_print(msg)

  socket.send(sock, msg)

  std_print("HTTP Result:\n")
  _do_while1:
    msg = socket.recv(sock, 1024)
    size = std_size(msg)
    if size < 1 goto _do_while1_break
    std_print(msg)
    goto _do_while1
  _do_while1_break:

  std_print("\n\nHTTP Done\n")
  return
