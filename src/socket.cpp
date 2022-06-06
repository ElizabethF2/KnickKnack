#if KNICKKNACK_ENABLE_SOCKETS

#include "socket.h"
#include "api.h"
#include "std.h"

#ifdef __WIN32__
  #include <winsock2.h>
#else
  #include <sys/types.h>
  #include <sys/socket.h>

  #ifdef NDS
    #include <dswifi9.h>
    #include <netinet/in.h>
    #include <netdb.h>
  #endif
#endif

#ifdef __WIN32__
  void knickknack_init_sockets()
  {
    WORD versionWanted = MAKEWORD(1, 1);
    WSADATA wsaData;
    WSAStartup(versionWanted, &wsaData);
  }
#endif

void knickknack_ext_close_socket(void* data)
{
  #ifdef __WIN32__
    closesocket((SOCKET)data);
  #else
    close((int)data);
  #endif
}

knickknack_object* knickknack_ext_new_socket(int domain, int type, int protocol)
{
  auto sock = socket(domain, type, protocol);
  if (sock != -1)
  {
    auto capsule = knickknack_new_capsule(knickknack_ext_close_socket, (void*)sock);
    if (capsule)
    {
      return capsule;
    }
    knickknack_ext_close_socket((void*)sock);
  }
  return nullptr;
}

knickknack_object* knickknack_ext_gethostbyname(knickknack_object* name)
{
  auto result = knickknack_std_new_string();
  
  if (result)
  {
    auto host = gethostbyname(knickknack_get_readonly_string_from_object(name)->c_str());
    knickknack_get_string_from_object(result)->append(inet_ntoa(*(struct in_addr*)(host->h_addr_list[0])));
  }
  
  return result;
}

int knickknack_ext_connect_socket(knickknack_object* socket, int domain, knickknack_object* host, int port)
{
  auto sock = (int)knickknack_get_data_from_capsule(knickknack_ext_close_socket, socket);

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = domain;
  addr.sin_addr.s_addr = inet_addr(knickknack_get_readonly_string_from_object(host)->c_str());
  addr.sin_port = htons(port);
  
  return connect(sock, (struct sockaddr*)&addr, sizeof(addr));;
}

int knickknack_ext_send_socket(knickknack_object* socket, knickknack_object* payload, int flags)
{
  auto sock = (int)knickknack_get_data_from_capsule(knickknack_ext_close_socket, socket);
  auto pl = knickknack_get_readonly_string_from_object(payload);
  return send(sock, pl->c_str(), pl->size(), flags);
}

knickknack_object* knickknack_ext_ioctl(knickknack_object* socket, int cmd)
{
  auto result = knickknack_std_new_list();
  
  if (result)
  {
    auto sock = (int)knickknack_get_data_from_capsule(knickknack_ext_close_socket, socket);
    unsigned long out;

    #ifdef __WIN32__
      auto ret = ioctlsocket(sock, cmd, &out);
    #else
      auto ret = ioctl(sock, cmd, &out);
    #endif

    knickknack_std_append_primitive(result, (void*)ret);
    knickknack_std_append_primitive(result, (void*)out);
  }

  return result;
}

knickknack_object* knickknack_ext_recv(knickknack_object* socket, int length, int flags)
{
  auto result = knickknack_std_new_string();

  if (result)
  {
    auto buflen = KNICKKNACK_BLOCK_SIZE > length ? length : KNICKKNACK_BLOCK_SIZE;
    char buffer[buflen];

    auto remaining = length;
    auto res = knickknack_get_string_from_object(result);
    auto sock = (int)knickknack_get_data_from_capsule(knickknack_ext_close_socket, socket);

    while(remaining > 0)
    {
      auto got = recv(sock, buffer, buflen, flags);
      if (got <= 0)
      {
        break;
      }

      for (int i = 0; i < got; ++i)
      {
        res->push_back(buffer[i]);
      }

      remaining -= got;
    }
  }

  return result;
}

#endif
