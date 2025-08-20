#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <net/if.h>

#define BUF_SIZE 120

int main (int argc, char **argv)
{
  int port, rc, on = 1;
  int listen_sockfd = -1, new_sockfd = -1;
  char recv_buffer[BUF_SIZE];
  struct sockaddr_in6 addr;
  struct pollfd fds;
  struct in6_addr ipv6_addr;
  char *iface;
  struct ifreq ifr;

  if (argc < 3) 
  {
     fprintf(stderr,"usage: %s <hostname> <port_number>\n", argv[0]);
     return EXIT_FAILURE;
  }

  listen_sockfd = socket(AF_INET6, SOCK_STREAM, 0);
  if (listen_sockfd < 0)
  {
    printf("socket() failed: %d\n", errno);
    return EXIT_FAILURE;
  }

  rc = setsockopt(listen_sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
  if (rc < 0)
  {
    printf("setsockopt() failed: %d\n", errno);
    close(listen_sockfd);
    return EXIT_FAILURE;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;

  rc = inet_pton(AF_INET6, argv[1], &ipv6_addr);
  if (rc < 0)
  {
    printf("inet_pton() failed: %d\n", errno);
  }
  memcpy(&addr.sin6_addr, &ipv6_addr, sizeof(ipv6_addr));
  port = atoi(argv[2]);
  addr.sin6_port = htons(port);

  rc = bind(listen_sockfd, (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0)
  {
    printf("bind() failed: %d\n", errno);
    close(listen_sockfd);
    return EXIT_FAILURE;
  }

//Binding for outgoing traffic needs more investigation, both methods fails at the moment..
//  iface = "lo3";
//  rc = setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, iface, strlen(iface));
//  if (rc < 0)
//  {
//    printf("setsockopt() failed: %d\n", errno);
//    close(sockfd);
//    return EXIT_FAILURE;
//  }
//  else
//    printf("Socket successfully bound to device: %s\n", iface);

//  memset(&ifr, 0, sizeof(struct ifreq));
//  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "lo3");
//  ioctl(sockfd, SIOCGIFINDEX, &ifr);
//  setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE,  (void*)&ifr, sizeof(struct ifreq));

  rc = listen(listen_sockfd, 32);
  if (rc < 0)
  {
    printf("listen() failed: %d\n", errno);
    close(listen_sockfd);
    return EXIT_FAILURE;
  }

  new_sockfd = accept(listen_sockfd, NULL, NULL);

  if (new_sockfd < 0)
  {
    printf("accept() failed: %d\n", errno);
    return EXIT_FAILURE;
  }

  rc = fcntl(new_sockfd, F_SETFL, O_NONBLOCK);

  if (rc < 0)
  {
    printf("fcntl() failed: %d\n", errno);
    close(listen_sockfd);
    close(new_sockfd);
    return EXIT_FAILURE;
  }

  memset(recv_buffer, 0, sizeof(recv_buffer));

  memset(&fds, 0 , sizeof(fds));
  fds.fd = new_sockfd;
  fds.events = POLLIN;

  do
  {
    rc = poll(&fds, 1, 100);

    if (rc < 0)
    {
      printf("poll() failed: %d\n", errno);
      break;
    }

    if(fds.revents == 0)
    {
      continue;
    }
    else
    {
      if(!(fds.revents & POLLIN))
      {
        printf("Wrong revents = %d\n", fds.revents);
        break;
      }

      if(fds.revents & POLLIN)
      {
        rc = recv(fds.fd, recv_buffer, sizeof(recv_buffer), 0);
        if (rc < 0)
        {
          if (errno != EWOULDBLOCK)
          {
            printf("recv() failed: %d\n", errno);
            break;
          }
        }    

        if (rc == 0)
        {
          printf(" Connection closed\n");
          break;
        }

        printf("Receiving msg from middleware client: %s\n", recv_buffer);
        memset(recv_buffer, 0, sizeof(recv_buffer));
      }
    }
  } while(1);

  close(new_sockfd);
  close(listen_sockfd);
}
