// main.c

/*
* Sample application for picoupnp.h
*
*/

#ifdef _WIN32
  #include "win32/w32_crt_stub.h"
#endif
#include <stdio.h>
#include "picoupnp.h"

inline static unsigned short __atouh(const char* a)
{
  unsigned short i,x;
  for (i = 0, x = 0; a[i] > 47 && a[i] < 58 && i < 6; i++)
    x = x * 10 + a[i] - 48;
  return x;
}

int usage(void)
{
  puts("Usage: picoupnp -a|-d <port> tcp|udp\n"                  \
       "           Add or delete port redirection\n"             \
       "       picoupnp -a|-d <start_port>-<end_port> tcp|udp\n" \
       "           Add or delete port range redirection\n"       \
       "       picoupnp -l\n"                                    \
       "           Print local IP address\n"                     \
       "       picoupnp -e\n"                                    \
       "           Print external IP address\n"                  \
       "       picoupnp -g\n"                                    \
       "           Print default gateway IP address\n"           \
       "       picoupnp -i\n"                                    \
       "           Print information");
  return -1;
}

int main(int argc, char** argv)
{
  unsigned short start_port = 0, end_port = 0;
  unsigned int protocol;
  int delete;

  if (argc < 2 || (argv[1][0] != '-' && argv[1][0] != '/')) {
    return usage();
  }

  if (argv[1][1] == 'A' || argv[1][1] == 'a') {
    if (argc < 4) return usage();
    delete = 0;
  }
  else if (argv[1][1] == 'D' || argv[1][1] == 'd') {
    if (argc < 4) return usage();
    delete = 1;
  }
  else if (argv[1][1] == 'L' || argv[1][1] == 'l') {
    unsigned long ip = GetLocalIP();
    char sip[16];
    GetIPString(ip, sip);
    puts(sip);
    return !(ip != 0 && ip != -1);
  }
  else if (argv[1][1] == 'E' || argv[1][1] == 'e') {
    unsigned long ip = GetExternalIP();
    char sip[16];
    GetIPString(ip, sip);
    puts(sip);
    return !(ip != 0 && ip != -1);
  }
  else if (argv[1][1] == 'G' || argv[1][1] == 'g') {
    unsigned long ip = GetDefaultGatewayIP();
    char sip[16];
    GetIPString(ip, sip);
    puts(sip);
    return !(ip != 0 && ip != -1);
  }
  else if (argv[1][1] == 'I' || argv[1][1] == 'i') {
    char sip[16];
    GetIPString(GetLocalIP(), sip);
    printf("Local IP address           : %s\n", sip);
    GetIPString(GetDefaultGatewayIP(), sip);
    printf("Default gateway IP address : %s\n", sip);
    GetIPString(GetExternalIP(), sip);
    printf("External IP address        : %s\n", sip);
    return 0;
  }
  else {
    return usage();
  }

  start_port = __atouh(argv[2]);
  if (start_port == 0) {
    puts("Invalid port");
    return -1;
  }
  for (unsigned int i = 0; argv[2][i]; i++) {
    if (argv[2][i] == '-') {
      i++;
      end_port = __atouh(argv[2]+i);
      if (end_port < start_port) {
        puts("Invalid port range");
        return -1;
      }
    }
  }

  if ((argv[3][0] == 'T' || argv[3][0] == 't') &&
      (argv[3][1] == 'C' || argv[3][1] == 'c') &&
      (argv[3][2] == 'P' || argv[3][2] == 'p'))
  {
    protocol = IPPROTO_TCP;
  }
  else if ((argv[3][0] == 'U' || argv[3][0] == 'u') &&
      (argv[3][1] == 'D' || argv[3][1] == 'd') &&
      (argv[3][2] == 'P' || argv[3][2] == 'p'))
  {
    protocol = IPPROTO_UDP;
  }
  else {
    return usage();
  }

  if (delete) {
    do {
      printf("Deleting port mapping for %hu %s ...\n", start_port, (protocol == IPPROTO_TCP ? "TCP" : "UDP"));
      DeletePortMapping(start_port, protocol);
      start_port++;
    } while (start_port <= end_port);
  }
  else {
    do {
      printf("Adding port mapping for %hu %s ...\n", start_port, (protocol == IPPROTO_TCP ? "TCP" : "UDP"));
      AddPortMapping(start_port, protocol);
      start_port++;
    } while (start_port <= end_port);
  }
  return 0;
}
