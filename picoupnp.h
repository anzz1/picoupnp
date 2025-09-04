// picoupnp.h

/*
* picoupnp v1.01
* https://github.com/anzz1/picoupnp
*/

//////////////////////////////////////////////////////////////////////
//
// SUPPORTED PROTOCOLS
//
// UPNP
// http://upnp.org/specs/gw/UPnP-gw-WANIPConnection-v1-Service.pdf
//
// NAT-PMP (IEEE/RFC 6886)
// https://www.rfc-editor.org/rfc/rfc6886
//
// PCP (IEEE/RFC 6887)
// https://www.rfc-editor.org/rfc/rfc6887
//
//
// FUNCTIONS
//
// void AddPortMapping(unsigned short port, unsigned int protocol);
// void AddPortRangeMapping(unsigned short start_port, unsigned short end_port, unsigned int protocol);
//
// void DeletePortMapping(unsigned short port, unsigned int protocol);
// void DeletePortRangeMapping(unsigned short start_port, unsigned short end_port, unsigned int protocol);
//
//   - Port must be 1-65535
//   - Protocol must be either IPPROTO_TCP or IPPROTO_UDP
//
// unsigned int GetLocalIP();
// unsigned int GetDefaultGatewayIP();
//
// int HTTPGetXMLRequest(char* hostname, const char* port, char* url, char* response, int maxlen);
//   - text only
//   - response is also used for request, make sure the buffer is large enough to hold it
// int HTTPPostXMLRequest(char* hostname, const char* port, char* url, char* response, int maxlen, const char* postdata, const char* headers);
//   - text only
//   - response is also used for request, make sure the buffer is large enough to hold it, including postdata and headers
//
// int GetURLParts(char* url, char* host, char* port, char* path)
//   - breaks URL to HOST, PORT, PATH elements
// int GetIPString(unsigned int ip_in, char ip_out[16])
//   - long ipv4 to string
//
// (PICOUPNP_ASYNC is defined)
// AddPortMappingAsync, AddPortRangeMappingAsync, DeletePortMappingAsync, DeletePortRangeMappingAsync
//   - asynchronous (threaded) versions of port mapping functions. creates a thread for the request and continues execution immediately.
//
// (PICOUPNP_EXTIP_HOST is defined)
// unsigned int GetExternalIP();
//
// If for some reason it is desired, there is also separate UPNP_* / NATPMP_* / NATPCP_* functions to
// add or delete port mappings. The default functions without the prefix try all three for maximum
// success rate.
//
//////////////////////////////////////////////////////////////////////

#ifndef __PICOUPNP_H
#define __PICOUPNP_H

// Define this to enable asynchronous (threaded) functions
//#define PICOUPNP_ASYNC

// Define this to enable GetExternalIP() helper function
#define PICOUPNP_EXTIP_HOST "whatismyip.akamai.com"

#ifdef _WIN32
  #ifndef WINVER
    #define WINVER 0x0501
  #endif
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0501
  #endif
  #ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
  #endif

  #if !(defined(_WINSOCKAPI_) || defined(_WINSOCK_H))
    #include <winsock2.h>
    #include <ws2tcpip.h>
  #endif
  #include <windows.h>
  #include <iphlpapi.h>

  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "iphlpapi.lib")

    #pragma function(memcpy)
    void* __cdecl memcpy(void *dst, const void *src, size_t length) {
      size_t len = 0;
      unsigned char* destination = (unsigned char*)dst;
      for (len = 0; len < length; len++)
        destination[len] = ((unsigned char*)src)[len];
      return dst;
    }
  #endif
#else // !_WIN32
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <net/if.h>
  #include <linux/rtnetlink.h>
  #include <unistd.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <x86intrin.h>
  #ifdef PICOUPNP_ASYNC
    #include <pthread.h>
  #endif
  #define closesocket close
  #if defined(__clang__) || defined(__GNUC__)
    #define __cdecl __attribute__((__cdecl__))
  #endif
#endif // (_WIN32 || !_WIN32)

#ifndef SOL_TCP
  #define SOL_TCP 6
#endif
#ifndef TCP_USER_TIMEOUT
  #define TCP_USER_TIMEOUT 18
#endif

#ifdef _WIN32
  #define PIU_MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
  #define PIU_FREE(x) HeapFree(GetProcessHeap(), 0, (x))
#else
  #define PIU_MALLOC malloc
  #define PIU_FREE free
#endif

#ifdef _MSC_VER
  #define PIU_MEMZERO(a, b) __stosb((a), 0, (b))
#else
  #define PIU_MEMZERO(a, b) memset((a), 0, (b))
#endif

#ifdef __cplusplus
namespace picoupnp
{
#endif

#ifdef _WIN32
  const unsigned int piu_timeout = 2000;
#else
  struct timeval piu_timeout = { 2, 0 };
#endif

static const char UPNP_AddPortMappingXML[] =
"<?xml version=\"1.0\"?>\r\n"
"<s:Envelope"
" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
"<s:Body>"
"<u:AddPortMapping xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">"
"<NewRemoteHost></NewRemoteHost>"
"<NewExternalPort></NewExternalPort>"
"<NewProtocol></NewProtocol>"
"<NewInternalPort></NewInternalPort>"
"<NewInternalClient></NewInternalClient>"
"<NewEnabled>1</NewEnabled>"
"<NewPortMappingDescription></NewPortMappingDescription>"
"<NewLeaseDuration>604800</NewLeaseDuration>"
"</u:AddPortMapping>"
"</s:Body>"
"</s:Envelope>\r\n";

static const char UPNP_DeletePortMappingXML[] =
"<?xml version=\"1.0\"?>\r\n"
"<s:Envelope"
" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
"<s:Body>"
"<u:DeletePortMapping xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">"
"<NewRemoteHost></NewRemoteHost>"
"<NewExternalPort></NewExternalPort>"
"<NewProtocol></NewProtocol>"
"</u:DeletePortMapping>"
"</s:Body>"
"</s:Envelope>\r\n";

inline static char* piu_uhtoa(unsigned short h, char* a)
{
  if (h < 10) {
    a[0] = '0' + h;
    a[1] = 0;
  } else if (h < 100) {
    a[0] = '0' + (h / 10);
    a[1] = '0' + (h % 10);
    a[2] = 0;
  } else if (h < 1000) {
    a[0] = '0' + (h / 100);
    a[1] = '0' + (h % 100) / 10;
    a[2] = '0' + (h % 10);
    a[3] = 0;
  } else if (h < 10000) {
    a[0] = '0' + (h / 1000);
    a[1] = '0' + (h % 1000) / 100;
    a[2] = '0' + (h % 100) / 10;
    a[3] = '0' + (h % 10);
    a[4] = 0;
  } else {
    a[0] = '0' + (h / 10000);
    a[1] = '0' + (h % 10000) / 1000;
    a[2] = '0' + (h % 1000) / 100;
    a[3] = '0' + (h % 100) / 10;
    a[4] = '0' + (h % 10);
    a[5] = 0;
  }
  return a;
}

// s2 should be in lowercase
inline static char* piu_stristr(const char* s1, const char* s2)
{
  unsigned int i;
  char *p;
  for (p = (char*)s1; *p != 0; p++) {
    i = 0;
    do {
      if (s2[i] == 0) return p;
      if (p[i] == 0) break;
      if (s2[i] != ((p[i]>64 && p[i]<91) ? (p[i]+32):p[i])) break;
    } while (++i);
  }
  return 0;
}

int GetURLParts(char* url, char* host, char* port, char* path)
{
  char *p = url;
  unsigned int x, y, z;

  host[0] = 0;
  port[0] = 0;
  path[0] = '/';
  path[1] = 0;
  z = 0;

  while (*p && *p != ':') p++;
  if (*p) {
    p++;
    while (*p && *p == '/') p++;
  }
  if (*p == 0)
    p = url;

  for (x = 0; x < 255; x++) {
    if (p[x] == ':') {
      host[x] = 0;
      x++;
      for (y = 0; y < 6; y++) {
        if (p[x+y] < 48 || p[x+y] > 57) break;
        port[y] = p[x+y];
      }
      port[y] = 0;
      x += y;
      z = 1;
    }
    if (p[x] == '/') {
      if (!z) host[x] = 0;
      for (y = 0; y < 255; y++) {
        if (p[x+y] == 0) break;
        path[y] = p[x+y];
      }
      path[y] = 0;
      return x;
    }
    if (!z)
      host[x] = p[x];
    if (p[x] == 0)
      return x;
  }

  return 0;
}

int HTTPXMLRequest(char* hostname, const char* port, char* request, char* response, int maxlen)
{
  int sd;
  struct addrinfo hints, *info;
  char *p;

  PIU_MEMZERO(&hints, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  if (getaddrinfo(hostname, ((port && *port && *port != '0') ? port : "80"), &hints, &info)) return 0;

  sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sd == -1) {
    freeaddrinfo(info);
    return 0;
  }
  setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&piu_timeout, sizeof(piu_timeout));
  setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&piu_timeout, sizeof(piu_timeout));
  if (connect(sd, info->ai_addr, (int)info->ai_addrlen) == -1) {
    closesocket(sd);
    freeaddrinfo(info);
    return 0;
  }
  setsockopt(sd, SOL_TCP, TCP_USER_TIMEOUT, (const char *)&piu_timeout, sizeof(piu_timeout));
  if (send(sd, request, strlen(request), 0) == -1) {
    closesocket(sd);
    freeaddrinfo(info);
    return 0;
  }
  PIU_MEMZERO(response, maxlen);
  if (recv(sd, response, maxlen-1, 0) == -1) {
    closesocket(sd);
    freeaddrinfo(info);
    return 0;
  }
  closesocket(sd);
  freeaddrinfo(info);

  p = response;

  while (*p && *p == ' ') p++;

  if (*p != 'H' && *p != 'h') return 0;
  p++;
  if (*p != 'T' && *p != 't') return 0;
  p++;
  if (*p != 'T' && *p != 't') return 0;
  p++;
  if (*p != 'P' && *p != 'p') return 0;
  p++;
  if (*p++ != '/') return 0;

  while (*p && *p != ' ') p++;
  while (*p && *p == ' ') p++;

  if (*p++ != '2') return 0;
  if (*p++ != '0') return 0;
  if (*p++ != '0') return 0;

  while (*p) {
    if (*p++ == '\r' && *p++ == '\n' && *p++ == '\r' && *p++ == '\n') {
      while (*p) *response++ = *p++;
      *response = 0;
      return 1;
    }
  }

  return 0;
}

int HTTPPostXMLRequest(char* hostname, const char* port, char* url, char* response, int maxlen, const char* postdata, const char* headers)
{
  char* p;
  unsigned int i;
  char slen[6];
  unsigned short len;
  memcpy(response, "POST ", 5);
  p = response+5;
  i = 0;
  while (url[i]) *p++ = url[i++];
  memcpy(p, " HTTP/1.1\r\nHost: ", 17);
  p += 17;
  i = 0;
  while (hostname[i]) *p++ = hostname[i++];
  *p++ = ':';
  i = 0;
  while (port[i]) *p++ = port[i++];
  memcpy(p, "\r\nAccept: text/xml\r\nContent-Type: text/xml; charset=\"utf-8\"\r\nContent-Length: ", 77);
  p += 77;
  len = (unsigned short)strlen(postdata);
  piu_uhtoa(len, slen);
  i = 0;
  while (slen[i]) *p++ = slen[i++];
  *p++ = '\r';
  *p++ = '\n';
  i = 0;
  if (headers) while (headers[i]) *p++ = headers[i++];
  memcpy(p, "Connection: close\r\n\r\n", 21);
  p += 21;
  memcpy(p, postdata, len+1);

  return HTTPXMLRequest(hostname, port, response, response, maxlen);
}

int HTTPGetXMLRequest(char* hostname, const char* port, char* url, char* response, int maxlen)
{
  char* p;
  int i;
  memcpy(response, "GET ", 4);
  p = response+4;
  i = 0;
  while (url[i]) *p++ = url[i++];
  memcpy(p, " HTTP/1.1\r\nHost: ", 17);
  p += 17;
  i = 0;
  while (hostname[i]) *p++ = hostname[i++];
  *p++ = ':';
  i = 0;
  while (port[i]) *p++ = port[i++];
  memcpy(p, "\r\nAccept: text/xml\r\nConnection: close\r\n\r\n", 42);

  return HTTPXMLRequest(hostname, port, response, response, maxlen);
}

static int UPNP_ParseEndPoint(const char* xml /* in */, char* endpoint /* out[255] */)
{
  char *start = 0;
  char *end = 0;
  char *id = 0;
  char *url = 0;
  char *p = 0;
  unsigned int x;

  endpoint[0] = 0;

  if (!xml || !*xml)
    return 0;

  id = piu_stristr(xml, "wanipconn1");
  if (!id)
    id = piu_stristr(xml, "wanipconnection");
  if (!id)
    return 0;

  p = (char*)xml;
  while (*p) {
    start = piu_stristr(p, "<service>");
    if (!start || start > id) return 0;
    p = start+9;
    end = piu_stristr(p, "</service>");
    if (!end) return 0;
    if (end > id) {
      url = piu_stristr(p, "<controlurl>");
      if (!url) return 0;
      url += 12;
      if (url > end) return 0;
      for (x = 0; x < 255; x++) {
        if (!url[x]) return 0;
        if (url[x] == '<') {
          endpoint[x] = 0;
          return x;
        }
        endpoint[x] = url[x];
      }
      endpoint[0] = 0;
      return 0;
    }
    p = end+10;
  }

  return 0;
}

static int UPNP_GetRootDescXmlUrl(unsigned int localip, char* response, int maxlen)
{
  int sd;
  struct sockaddr_in server, client;
  char* p;
  unsigned int i;
  int err, addrlen;
  int yes = 1;
  int ttl = 2;

  sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sd == -1) return 0;

  PIU_MEMZERO(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(1900);
  server.sin_addr.s_addr = inet_addr("239.255.255.250");

  PIU_MEMZERO(&client, sizeof(client));
  client.sin_family = AF_INET;
  client.sin_port = 0;
  client.sin_addr.s_addr = localip;

  setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&piu_timeout, sizeof(piu_timeout));
  setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&piu_timeout, sizeof(piu_timeout));
  setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&client.sin_addr.s_addr, sizeof(int));
  setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, (const char *)&ttl, sizeof(int));

  if (bind(sd, (struct sockaddr*)&client, sizeof(struct sockaddr)) == -1) {
    closesocket(sd);
    return 0;
  }

  addrlen = sizeof(struct sockaddr);
  if (sendto(sd, "M-SEARCH * HTTP/1.0\r\n"
    "Host: 239.255.255.250:1900\r\n"
    "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
    "Man: \"ssdp:discover\"\r\n"
    "MX: 2\r\n\r\n",
    137, 0, (struct sockaddr*)&server, addrlen) == -1)
  {
    closesocket(sd);
    return 0;
  }

  PIU_MEMZERO(response, maxlen);
  err = recvfrom(sd, response, maxlen-1, 0, (struct sockaddr*)&server, &addrlen);
  closesocket(sd);
  if (err == -1) return 0;

  p = piu_stristr(response, "location:");
  if (!p) return 0;
  p += 9;
  while (*p == ' ') p++;
  for (i = 0; i < 255; i++) {
    if (p[i] == 0 || p[i] == '\r' || p[i] == '\n' || p[i] == ' ') break;
    response[i] = p[i];
  }
  response[i] = 0;
  return i;
}

int GetIPString(unsigned int ip_in, char* ip_out /* [16] */)
{
  char* str = inet_ntoa(*(struct in_addr*)&ip_in);
  if (str) {
    ip_out[0] = 0;
    for (int i = 0; i < 15; i++) {
      ip_out[i] = str[i];
    }
    ip_out[15] = 0;
    return 1;
  }
  return 0;
}

#ifdef _WIN32
WSADATA wsaData;
int WSAInit(void)
{
  int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sd == -1) {
    if (WSAGetLastError() == WSANOTINITIALISED) {
      return WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    return -1;
  }
  closesocket(sd);
  return 0;
}

unsigned int GetDefaultGatewayIP(void)
{
  MIB_IPFORWARDROW ip_forward;
  PIU_MEMZERO(&ip_forward, sizeof(ip_forward));
  if(!GetBestRoute(0, 0, &ip_forward)) return ip_forward.dwForwardNextHop;
  return 0;
}
#else // !_WIN32

struct route_info
{
  struct in_addr dstAddr;
  struct in_addr srcAddr;
  struct in_addr gateway;
  char ifname[IF_NAMESIZE];
};

static int ReadNetlinkSock(int sd, char *bufPtr, size_t buf_size, int seqNum, int pId)
{
  struct nlmsghdr *nlHdr;
  int readLen = 0, msgLen = 0;

  do
  {
    if((readLen = recv(sd, bufPtr, buf_size - msgLen, 0)) < 0) {
      return -1;
    }

    nlHdr = (struct nlmsghdr *)bufPtr;
    if((NLMSG_OK(nlHdr, readLen) == 0) || (nlHdr->nlmsg_type == NLMSG_ERROR)) {
      return -1;
    }

    if(nlHdr->nlmsg_type == NLMSG_DONE) {
      break;
    }
    else {
      bufPtr += readLen;
      msgLen += readLen;
    }

    if((nlHdr->nlmsg_flags & NLM_F_MULTI) == 0) {
      break;
    }
  }
  while((nlHdr->nlmsg_seq != seqNum) || (nlHdr->nlmsg_pid != pId));

  return msgLen;
}

static int ParseNetlinkRoutes(struct nlmsghdr *nlHdr, struct route_info *rtInfo)
{
  struct rtmsg *rtMsg;
  struct rtattr *rtAttr;
  int rtLen;

  rtMsg = (struct rtmsg *)NLMSG_DATA(nlHdr);
  if((rtMsg->rtm_family != AF_INET) || (rtMsg->rtm_table != RT_TABLE_MAIN))
    return -1;

  rtAttr = (struct rtattr *)RTM_RTA(rtMsg);
  rtLen = RTM_PAYLOAD(nlHdr);

  for(; RTA_OK(rtAttr,rtLen); rtAttr = RTA_NEXT(rtAttr,rtLen)) {
    switch(rtAttr->rta_type)
    {
      case RTA_OIF:
        if_indextoname(*(int *)RTA_DATA(rtAttr), rtInfo->ifname);
        break;

      case RTA_GATEWAY:
        memcpy(&rtInfo->gateway, RTA_DATA(rtAttr), sizeof(rtInfo->gateway));
        break;

      case RTA_PREFSRC:
        memcpy(&rtInfo->srcAddr, RTA_DATA(rtAttr), sizeof(rtInfo->srcAddr));
        break;

      case RTA_DST:
        memcpy(&rtInfo->dstAddr, RTA_DATA(rtAttr), sizeof(rtInfo->dstAddr));
        break;
    }
  }

  return 0;
}

unsigned int GetDefaultGatewayIP(void)
{
  struct nlmsghdr *nlMsg;
  struct rtmsg *rtMsg;
  struct route_info route_info;
  char msgBuf[8192];
  int sd, len, msgSeq = 0;

  sd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
  if(sd == -1) {
    return 0;
  }

  PIU_MEMZERO(msgBuf, sizeof(msgBuf));
  nlMsg = (struct nlmsghdr *)msgBuf;
  rtMsg = (struct rtmsg *)NLMSG_DATA(nlMsg);

  nlMsg->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
  nlMsg->nlmsg_type = RTM_GETROUTE;

  nlMsg->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
  nlMsg->nlmsg_seq = msgSeq++;
  nlMsg->nlmsg_pid = getpid();

  if(send(sd, nlMsg, nlMsg->nlmsg_len, 0) < 0) {
    close(sd);
    return 0;
  }

  if((len = ReadNetlinkSock(sd, msgBuf, sizeof(msgBuf), msgSeq, getpid())) < 0) {
    close(sd);
    return 0;
  }

  for(; NLMSG_OK(nlMsg,len); nlMsg = NLMSG_NEXT(nlMsg,len)) {
    PIU_MEMZERO(&route_info, sizeof(route_info));
    if (ParseNetlinkRoutes(nlMsg, &route_info) < 0) continue;
    if (route_info.dstAddr.s_addr == 0) {
      close(sd);
      return route_info.gateway.s_addr;
    }
  }

  close(sd);
  return 0;
}
#endif

unsigned int GetLocalIP(void)
{
  char hostname[256];
  struct addrinfo hints, *info;
  unsigned int addr;

#ifdef _WIN32
  if (WSAInit()) return 0;

  MIB_IPFORWARDROW ip_forward;
  PIU_MEMZERO(&ip_forward, sizeof(ip_forward));
  if (!GetBestRoute(0, 0, &ip_forward)) {
    DWORD dwSize = sizeof(MIB_IPADDRTABLE);
    PMIB_IPADDRTABLE pip_table = (PMIB_IPADDRTABLE)PIU_MALLOC(sizeof(MIB_IPADDRTABLE));
    if (pip_table) {
      DWORD ret = GetIpAddrTable(pip_table, &dwSize, 0);
      if (ret == ERROR_INSUFFICIENT_BUFFER) {
        PIU_FREE(pip_table);
        pip_table = (PMIB_IPADDRTABLE)PIU_MALLOC(dwSize);
        if (GetIpAddrTable(pip_table, &dwSize, 0)) {
          PIU_FREE(pip_table);
          pip_table = 0;
        }
      }
      else if (ret != ERROR_SUCCESS) {
        PIU_FREE(pip_table);
        pip_table = 0;
      }
      if (pip_table) {
        for (unsigned int i = 0; i < pip_table->dwNumEntries; i++) {
          if (pip_table->table[i].dwIndex == ip_forward.dwForwardIfIndex) {
            DWORD addr = pip_table->table[i].dwAddr;
            PIU_FREE(pip_table);
            return addr;
          }
        }
        PIU_FREE(pip_table);
      }
    }
  }
#endif

  if (gethostname(hostname, sizeof(hostname))) return 0;
  PIU_MEMZERO(&hints, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  if (getaddrinfo(hostname, NULL, &hints, &info)) return 0;
  addr = *(unsigned int*)&info->ai_addr->sa_data[2];
  freeaddrinfo(info);
  return addr;
}

#ifdef PICOUPNP_EXTIP_HOST
unsigned int GetExternalIP(void)
{
#ifdef _WIN32
  if (WSAInit()) return 0;
#endif

  char* response = (char*)PIU_MALLOC(65535);
  if (response) {
    if (HTTPGetXMLRequest(PICOUPNP_EXTIP_HOST, "80", "/", response, 65535)) {
      return inet_addr(response);
    }
    PIU_FREE(response);
  }
  return 0;
}
#endif // PICOUPNP_EXTIP_HOST

#define NATPMP_PORT 5351
#define NATPMP_MAP_UDP 1
#define NATPMP_MAP_TCP 2

struct natpmp_request {
  unsigned char version;
  unsigned char opcode;
  unsigned short reserved;
  unsigned short internal_port;
  unsigned short external_port;
  unsigned int lifetime;
};

static void NATPMP_PortMapping(unsigned short port, unsigned int protocol, unsigned int delete)
{
  int sd;
  unsigned int gatewayip, addrlen;
  struct natpmp_request req;
  struct sockaddr_in server;

#ifdef _WIN32
  if (WSAInit()) return;
#endif

  gatewayip = GetDefaultGatewayIP();
  if (!gatewayip) return;

  req.version = 0;
  req.reserved = 0;
  req.opcode = (protocol == IPPROTO_UDP ? NATPMP_MAP_UDP : NATPMP_MAP_TCP);
  req.internal_port = htons(port);
  req.external_port = (delete ? 0 : req.internal_port);
  req.lifetime = (delete ? 0 : htonl(604800));

  sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sd == -1) return;

  setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&piu_timeout, sizeof(piu_timeout));
  setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&piu_timeout, sizeof(piu_timeout));

  PIU_MEMZERO(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(NATPMP_PORT);
  server.sin_addr.s_addr = gatewayip;
  addrlen = sizeof(struct sockaddr);

  sendto(sd, (const char*)&req, sizeof(struct natpmp_request), 0, (struct sockaddr*)&server, addrlen);
  closesocket(sd);
}

void NATPMP_AddPortMapping(unsigned short port, unsigned int protocol)
{
  NATPMP_PortMapping(port, protocol, 0);
}

void NATPMP_DeletePortMapping(unsigned short port, unsigned int protocol)
{
  NATPMP_PortMapping(port, protocol, 1);
}

#define NATPCP_PORT NATPMP_PORT
#define NATPCP_MAP 1

struct natpcp_map_request {
  unsigned char version;
  unsigned char opcode;
  unsigned short reserved1;
  unsigned int lifetime;
  unsigned char internal_ip6_zeros[10];
  unsigned short internal_ip6_ones;
  unsigned int internal_ip4;
  unsigned int nonce1;
  unsigned int nonce2;
  unsigned int nonce3;
  unsigned char protocol;
  unsigned char reserved2;
  unsigned short reserved3;
  unsigned short internal_port;
  unsigned short external_port;
  unsigned char external_ip6_zeros[10];
  unsigned short external_ip6_ones;
  unsigned int external_ip4;
};

static void NATPCP_PortMapping(unsigned short port, unsigned int protocol, unsigned int localip, unsigned int delete)
{
  int sd;
  unsigned int gatewayip, addrlen;
  struct natpcp_map_request req;
  struct sockaddr_in server;
  static unsigned int nonce1 = 0, nonce2 = 0, nonce3 = 0;

  if (!nonce1 && !nonce2 && !nonce3) {
    unsigned long long tsc = __rdtsc();
    nonce1 = ((unsigned int *)&tsc)[0] ^ 0x11223344;
    nonce2 = ((unsigned int *)&tsc)[1] ^ 0x55667788;
    nonce3 = ((unsigned int *)&tsc)[0] ^ ((unsigned int *)&tsc)[1] ^ 0x11551155;
  }

#ifdef _WIN32
  if (WSAInit()) return;
#endif

  gatewayip = GetDefaultGatewayIP();
  if (!gatewayip) return;

  req.version = 2;
  req.opcode = NATPCP_MAP;
  req.reserved1 = 0;
  req.lifetime = (delete ? 0 : htonl(604800));
  PIU_MEMZERO(req.internal_ip6_zeros, 10);
  req.internal_ip6_ones = 0xFFFF;
  req.internal_ip4 = localip;
  req.nonce1 = nonce1;
  req.nonce2 = nonce2;
  req.nonce3 = nonce3;
  req.protocol = (protocol == IPPROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP);
  req.reserved2 = 0;
  req.reserved3 = 0;
  req.internal_port = htons(port);
  req.external_port = (delete ? 0 : req.internal_port);
  PIU_MEMZERO(req.external_ip6_zeros, 10);
  req.external_ip6_ones = 0xFFFF;
  req.external_ip4 = 0;

  sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sd == -1) return;

  setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&piu_timeout, sizeof(piu_timeout));
  setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&piu_timeout, sizeof(piu_timeout));

  PIU_MEMZERO(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(NATPCP_PORT);
  server.sin_addr.s_addr = gatewayip;
  addrlen = sizeof(struct sockaddr);

  sendto(sd, (const char*)&req, sizeof(struct natpcp_map_request), 0, (struct sockaddr*)&server, addrlen);
  closesocket(sd);
}

void NATPCP_AddPortMapping(unsigned short port, unsigned int protocol)
{
  unsigned int localip = GetLocalIP();
  if (!localip)
    return;
  NATPCP_PortMapping(port, protocol, localip, 0);
}

void NATPCP_DeletePortMapping(unsigned short port, unsigned int protocol)
{
  unsigned int localip = GetLocalIP();
  if (!localip)
    return;
  NATPCP_PortMapping(port, protocol, localip, 1);
}

int UPNP_AddPortMapping(unsigned short port, unsigned int protocol)
{
  char xml[768], slocalip[16], url[256], host[256], hport[6], sport[6];
  char *response, *p;
  unsigned int i = 0;
  unsigned int localip;

#ifdef _WIN32
  if (WSAInit()) return 0;
#endif

  if (protocol != IPPROTO_IP && protocol != IPPROTO_TCP && protocol != IPPROTO_UDP)
    return 0;

  localip = GetLocalIP();
  if (!localip)
    return 0;

  if (!GetIPString(localip, slocalip))
    return 0;

  response = (char*)PIU_MALLOC(65535);
  if (response) {
    PIU_MEMZERO(response, 65535);
    if (UPNP_GetRootDescXmlUrl(localip, response, 65535)) {
      if (GetURLParts(response, host, hport, url)) {
        if (HTTPGetXMLRequest(host, hport, url, response, 65535)) {
          if (!UPNP_ParseEndPoint(response, url)) url[0] = 0;
          memcpy(xml, UPNP_AddPortMappingXML, 278);
          p = xml + 278;
          piu_uhtoa(port, sport);
          while (sport[i]) *p++ = sport[i++];
          memcpy(p, UPNP_AddPortMappingXML + 278, 31);
          p += 31;
          *p++ = (protocol == IPPROTO_UDP ? 'U' : 'T');
          *p++ = (protocol == IPPROTO_UDP ? 'D' : 'C');
          *p++ = 'P';
          memcpy(p, UPNP_AddPortMappingXML + 309, 31);
          p += 31;
          i = 0;
          while (sport[i]) *p++ = sport[i++];
          memcpy(p, UPNP_AddPortMappingXML + 340, 37);
          p += 37;
          i = 0;
          while (slocalip[i]) *p++ = slocalip[i++];
          memcpy(p, UPNP_AddPortMappingXML + 377, 188);

          i = HTTPPostXMLRequest(host, hport, (url[0] ? url : "/ctl/IPConn"), response, 65535, xml, "SOAPAction: \"urn:schemas-upnp-org:service:WANIPConnection:1#AddPortMapping\"\r\n");
        }
      }
    }
    PIU_FREE(response);
  }
  return i;
}

int UPNP_DeletePortMapping(unsigned short port, unsigned int protocol)
{
  char xml[768], slocalip[16], url[256], host[256], hport[6], sport[6];
  char *response, *p;
  unsigned int i = 0;
  unsigned int localip;

#ifdef _WIN32
  if (WSAInit()) return 0;
#endif

  if (protocol != IPPROTO_IP && protocol != IPPROTO_TCP && protocol != IPPROTO_UDP)
    return 0;

  localip = GetLocalIP();
  if (!localip)
    return 0;

  if (!GetIPString(localip, slocalip))
    return 0;

  response = (char*)PIU_MALLOC(65535);
  if (response) {
    PIU_MEMZERO(response, 65535);
    if (UPNP_GetRootDescXmlUrl(localip, response, 65535)) {
      if (GetURLParts(response, host, hport, url)) {
        if (HTTPGetXMLRequest(host, hport, url, response, 65535)) {
          if (!UPNP_ParseEndPoint(response, url)) url[0] = 0;
          memcpy(xml, UPNP_DeletePortMappingXML, 281);
          p = xml + 281;
          piu_uhtoa(port, sport);
          while (sport[i]) *p++ = sport[i++];
          memcpy(p, UPNP_DeletePortMappingXML + 281, 31);
          p += 31;
          *p++ = (protocol == IPPROTO_UDP ? 'U' : 'T');
          *p++ = (protocol == IPPROTO_UDP ? 'D' : 'C');
          *p++ = 'P';
          memcpy(p, UPNP_DeletePortMappingXML + 312, 61);

          i = HTTPPostXMLRequest(host, hport, (url[0] ? url : "/ctl/IPConn"), response, 65535, xml, "SOAPAction: \"urn:schemas-upnp-org:service:WANIPConnection:1#DeletePortMapping\"\r\n");
        }
      }
    }
    PIU_FREE(response);
  }
  return i;
}

void AddPortMapping(unsigned short port, unsigned int protocol)
{
  NATPMP_AddPortMapping(port, protocol);
  NATPCP_AddPortMapping(port, protocol);
  UPNP_AddPortMapping(port, protocol);
}

void DeletePortMapping(unsigned short port, unsigned int protocol)
{
  NATPMP_DeletePortMapping(port, protocol);
  NATPCP_DeletePortMapping(port, protocol);
  UPNP_DeletePortMapping(port, protocol);
}

void AddPortRangeMapping(unsigned short start_port, unsigned short end_port, unsigned int protocol)
{
  for (unsigned short port = start_port; port <= end_port; port++) {
    AddPortMapping(port, protocol);
  }
}

void DeletePortRangeMapping(unsigned short start_port, unsigned short end_port, unsigned int protocol)
{
  for (unsigned short port = start_port; port <= end_port; port++) {
    DeletePortMapping(port, protocol);
  }
}

#ifdef PICOUPNP_ASYNC
#ifdef _WIN32

static unsigned int __stdcall portMapThread(void* param)
{
  if ((unsigned int)param & 1) {
    DeletePortMapping((unsigned int)param >> 1, (unsigned int)param >> 17);
  }
  else {
    AddPortMapping((unsigned int)param >> 1, (unsigned int)param >> 17);
  }
  return 0;
}

void AddPortMappingAsync(unsigned short port, unsigned int protocol)
{
  CloseHandle(CreateThread(0, 0, portMapThread, (void*)((protocol << 17) | (port << 1) | 0), 0, 0));
}
void DeletePortMappingAsync(unsigned short port, unsigned int protocol)
{
  CloseHandle(CreateThread(0, 0, portMapThread, (void*)((protocol << 17) | (port << 1) | 1), 0, 0));
}

#else // !_WIN32

static void* __cdecl portMapThread(void* param)
{
  if ((unsigned int)param & 1) {
    DeletePortMapping((unsigned int)param >> 1, (unsigned int)param >> 17);
  }
  else {
    AddPortMapping((unsigned int)param >> 1, (unsigned int)param >> 17);
  }
  return 0;
}

void AddPortMappingAsync(unsigned short port, unsigned int protocol)
{
  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&thread, &attr, &portMapThread, (void*)((protocol << 17) | (port << 1) | 0));
  pthread_attr_destroy(&attr);
}
void DeletePortMappingAsync(unsigned short port, unsigned int protocol)
{
  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&thread, &attr, &portMapThread, (void*)((protocol << 17) | (port << 1) | 1));
  pthread_attr_destroy(&attr);
}
#endif // (_WIN32 || !_WIN32)

void AddPortRangeMappingAsync(unsigned short start_port, unsigned short end_port, unsigned int protocol)
{
  for (unsigned short port = start_port; port <= end_port; port++) {
    AddPortMappingAsync(port, protocol);
  }
}
void DeletePortRangeMappingAsync(unsigned short start_port, unsigned short end_port, unsigned int protocol)
{
  for (unsigned short port = start_port; port <= end_port; port++) {
    DeletePortMappingAsync(port, protocol);
  }
}

#endif // PICOUPNP_ASYNC

#ifdef __cplusplus
};
#endif

#endif // __PICOUPNP_H
