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
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>
#include <net/if.h>

#define BUF_SIZE 120

int main (int argc, char **argv)
{
  int rc, s, on = 1;
  int sockfd = -1;
  char send_buffer[BUF_SIZE];
  const char *msg = "Some important message from source";
  struct addrinfo hints, *result, *rp; 
  struct pollfd fds;
  char *iface;
  struct ifreq ifr;

  if (argc < 3) 
  {
     fprintf(stderr,"usage: %s <hostname> <port_number>\n", argv[0]);
     return EXIT_FAILURE;
  }

  sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
  if (sockfd < 0)
  {
    printf("socket() failed: %d\n", errno);
    return EXIT_FAILURE;
  }

  rc = fcntl(sockfd, F_SETFL, O_NONBLOCK);
  if (rc < 0)
  {
    printf("fcntl() failed: %d\n", errno);
    close(sockfd);
    return EXIT_FAILURE;
  }

//Binding for outgoing traffic need more investigation, both methods fails at the moment..
//  iface = "lo1";
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
//  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "lo1");
//  ioctl(sockfd, SIOCGIFINDEX, &ifr);
//  setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE,  (void*)&ifr, sizeof(struct ifreq));

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = 0;

  rc = getaddrinfo(argv[1], argv[2], &hints, &result);

  if (rc != 0) {
    printf("getaddrinfo failed(): %d\n", errno);
    return EXIT_FAILURE;
  }

  for (rp = result; rp != NULL; rp = rp->ai_next)
  {
    rc = connect(sockfd, rp->ai_addr, rp->ai_addrlen);
    if(rc < 0)
    {
      continue;
    }
    else
      break;
  }

  strncpy(send_buffer, msg, BUF_SIZE-1);

  memset(&fds, 0 , sizeof(fds));
  fds.fd = sockfd;
  fds.events = POLLOUT;

  do
  {
    rc = poll(&fds, 1, 100);

    if (rc < 0)
    {
      printf("poll() failed: %d\n", errno);
      break;
    }

    if(rc < 0)
    {
      if (errno != EWOULDBLOCK)
      { 
        printf("send() failed: %d\n", errno);
        break;
      }
    }

    if(fds.revents == 0)
    {
      continue;
    }
    else
    {
      if(!(fds.revents & POLLOUT))
      {
        printf("Wrong revents = %d\n", fds.revents);
        break;
      }

      if(fds.revents & POLLOUT)
      {
        printf("Sending msg to middleware server: %s\n", send_buffer);
        rc = send(fds.fd, send_buffer, sizeof(send_buffer), 0);

        if(rc < 0)
        {
          if (errno != EWOULDBLOCK)
          {
            printf("send() failed:  %d\n", errno);
            break;
          }
        }
      }
    }
    sleep(5);
  } while(1);

  close(sockfd);
}

