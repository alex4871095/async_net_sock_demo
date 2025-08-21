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
#include <arpa/inet.h>
#include <net/if.h>

#define BUF_SIZE 120
#define HEX_SIZE 240
#define LOG_SIZE 300

int send_flag = 0;
char send_buffer[BUF_SIZE];
pthread_mutex_t lock;

void stringToHex(const char *input_string, char *output_hex_string) {
  int i;
  int len = strlen(input_string);

  for (i = 0; i < len; i++) {
    sprintf(&output_hex_string[i * 2], "%02X", (unsigned char)input_string[i]);
  }
  output_hex_string[len * 2] = '\0';
}

typedef struct {
  int argc;
  char **argv;
  int fd;
} thread_args_t;

void* recv_thread(void *arg)
{
  int port, rc, on = 1;
  int recv_sockfd = -1;
  char recv_buffer[BUF_SIZE];
  struct sockaddr_in6 addr;
  struct addrinfo hints, *result, *rp;
  struct pollfd fds;
  char hexString[HEX_SIZE];
  thread_args_t *args = (thread_args_t *)arg;
  struct in6_addr ipv6_addr;
  char log_msg[LOG_SIZE];

  recv_sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
  if (recv_sockfd < 0)
  {
    printf("socket() failed: %d\n", errno);
    pthread_exit(NULL);
  }

  rc = setsockopt(recv_sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
  if (rc < 0)
  {
    printf("setsockopt() failed: %d\n", errno);
    close(recv_sockfd);
    pthread_exit(NULL);
  }

  rc = fcntl(recv_sockfd, F_SETFL, O_NONBLOCK);
  if (rc < 0)
  {
    printf("fcntl() failed: %d\n", errno);
    close(recv_sockfd);
    pthread_exit(NULL);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  rc = inet_pton(AF_INET6, args->argv[1], &ipv6_addr);
  if (rc < 0)
  {
    printf("inet_pton() failed: %d\n", errno);
  }
  memcpy(&addr.sin6_addr, &ipv6_addr, sizeof(ipv6_addr));
  port = atoi(args->argv[2]);
  addr.sin6_port = htons(port);

  rc = bind(recv_sockfd, (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0)
  {
    printf("bind() failed: %d\n", errno);
    close(recv_sockfd);
    pthread_exit(NULL);
  }

  sprintf(log_msg, "UDP server started\n");
  pthread_mutex_lock(&lock);
  rc = write(args->fd, log_msg, strlen(log_msg));
  pthread_mutex_unlock(&lock);
  if (rc < 0)
  {
    printf("write() failed: %d\n", errno);
  }

  memset(recv_buffer, 0, sizeof(recv_buffer));

  memset(&fds, 0 , sizeof(fds));
  fds.fd = recv_sockfd;
  fds.events = POLLIN;

  sprintf(log_msg, "Start polling in recv thread\n");
  pthread_mutex_lock(&lock);
  rc = write(args->fd, log_msg, strlen(log_msg));
  pthread_mutex_unlock(&lock);
  if (rc < 0)
  {
    printf("write() failed: %d\n", errno);
  }

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

        if ((rc > 0 && rc < 10) || rc > 120)
        {
          sprintf(log_msg, "Wrong msg size received\n");
          pthread_mutex_lock(&lock);
          rc = write(args->fd, log_msg, strlen(log_msg));
          pthread_mutex_unlock(&lock);
          if (rc < 0)
          {
            printf("write() failed: %d\n", errno);
          }
        }
        else
        {
          memset(hexString, 0, sizeof(hexString));
          stringToHex(recv_buffer, hexString);

          sprintf(log_msg, "UDP RX: (%d) %s\n", strlen(recv_buffer), hexString); 
          pthread_mutex_lock(&lock);
          rc = write(args->fd, log_msg, strlen(log_msg));
          pthread_mutex_unlock(&lock);
          if (rc < 0)
          {
            printf("write() failed: %d\n", errno);
          }

          strcpy(send_buffer, args->argv[6]);
          strcat(send_buffer, recv_buffer);
          send_flag = 1;
          memset(recv_buffer, 0, sizeof(recv_buffer));
        }         
      }
    }
  } while(1);

}


void* send_thread(void *arg)
{
  int rc, on = 1;
  int send_sockfd = -1;
  char recv_buffer[BUF_SIZE];
  struct sockaddr_in6 serv_addr;
  struct addrinfo hints, *result, *rp;
  struct pollfd fds;
  char hexString[HEX_SIZE];
  thread_args_t *args = (thread_args_t *)arg;
  struct in6_addr ipv6_addr;
  char log_msg[LOG_SIZE];
  char *iface;
  struct ifreq ifr;

jump_label:
  send_sockfd = socket(AF_INET6, SOCK_STREAM, 0);
  if (send_sockfd < 0)
  {
    printf("socket() failed: %d\n", errno);
    pthread_exit(NULL);
  }

//Binding for outgoing traffic needs more investigation, both methods fails at the moment..
//  iface = "lo2";
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
//  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "lo2");
//  ioctl(sockfd, SIOCGIFINDEX, &ifr);
//  setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE,  (void*)&ifr, sizeof(struct ifreq));

  rc = fcntl(send_sockfd, F_SETFL, O_NONBLOCK);
  if (rc < 0)
  {
    printf("fcntl() failed: %d\n", errno);
    close(send_sockfd);
    pthread_exit(NULL);
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;

  rc = getaddrinfo(args->argv[3], args->argv[4], &hints, &result);
  if (rc != 0) {
    printf("getaddrinfo() failed: %d\n", errno);
    pthread_exit(NULL);
  }

  rc = -1;
  while(rc < 0) {
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
      rc = connect(send_sockfd, rp->ai_addr, rp->ai_addrlen);
      if(rc < 0)
      {
        continue;
      }
      else
      {
        sprintf(log_msg, "Connected to remote TCP server\n");
        pthread_mutex_lock(&lock);
        rc = write(args->fd, log_msg, strlen(log_msg));
        pthread_mutex_unlock(&lock);
        if (rc < 0)
        {
          printf("write() failed: %d\n", errno);
        }
        break;
      }
    }
  }

  memset(&fds, 0, sizeof(fds));
  fds.fd = send_sockfd;
  fds.events = POLLIN | POLLOUT;

  sprintf(log_msg, "Start polling in send thread\n");
  pthread_mutex_lock(&lock);
  rc = write(args->fd, log_msg, strlen(log_msg));
  pthread_mutex_unlock(&lock);
  if (rc < 0)
  {
    printf("write() failed: %d", errno);
  }

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
      if(!(fds.revents & POLLIN) && !(fds.revents & POLLOUT))
      {
        printf("Wrong revents = %d\n", fds.revents);
        break;
      }

      if(fds.revents & POLLIN)
      {
        sprintf(log_msg, "Got msg from TCP server\n");
        pthread_mutex_lock(&lock);
        rc = write(args->fd, log_msg, strlen(log_msg));
        pthread_mutex_unlock(&lock);
        if (rc < 0)
        {
          printf("write() failed: %d", errno);
        }

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
          sprintf(log_msg, "TCP connection closed, restarting\n");
          pthread_mutex_lock(&lock);
          rc = write(args->fd, log_msg, strlen(log_msg));
          pthread_mutex_unlock(&lock);
          if (rc < 0)
          {
            printf("write() failed: %d\n", errno);
          }

//It's not possible to re-use the same socket again after disconnect, so we should start all over again - re-create socket, etc.
//While usegae of goto is not recommended, it good fits here.
          goto jump_label;
        }
        else
        {
          memset(hexString, 0, sizeof(hexString));
          stringToHex(recv_buffer, hexString);

          sprintf(log_msg, "TCP RX: (%d) %s\n", strlen(recv_buffer), hexString);
          pthread_mutex_unlock(&lock);
          rc = write(args->fd, log_msg, strlen(log_msg));
          pthread_mutex_unlock(&lock);
          if (rc < 0)
          {
            printf("write() failed: %d\n", errno);
          }

          memset(recv_buffer, 0, sizeof(recv_buffer));
        }
      }

      if(fds.revents & POLLOUT)
      {
        if(send_flag == 1)
        {
          rc = send(fds.fd, send_buffer, sizeof(send_buffer), 0);

          if(rc < 0)
          {
            if (errno != EWOULDBLOCK)
            {
              printf("send() failed: %d\n", errno);
              break;
            }
          }
          else
          {
            memset(hexString, 0, sizeof(hexString));
            stringToHex(send_buffer, hexString);

            sprintf(log_msg, "TCP TX: (%d) %s\n", strlen(send_buffer), hexString);
            pthread_mutex_lock(&lock);
            rc = write(args->fd, log_msg, strlen(log_msg));
            pthread_mutex_unlock(&lock);
            if (rc < 0)
            {
              printf("write() failed: %d\n", errno);
            }

            send_flag = 0;
            memset(send_buffer, 0, sizeof(send_buffer));
          }
        }
      }
    }
  } while(1);

  close(send_sockfd);

}



int main (int argc, char **argv)
{
  int rc, fd;
  pthread_t receiving_thread, sending_thread;
  thread_args_t thread_data;
  thread_data.argc = argc;
  thread_data.argv = argv;
  char log_msg[LOG_SIZE];

  if (argc < 7) 
  {
     printf("Usage: %s <udp addr> <udp port> <tcp addr> <tcp port> <logfile> <XXXX>\n", argv[0]);
     return EXIT_FAILURE;
  }

  fd = open(argv[5], O_APPEND|O_CREAT|O_RDWR, S_IWUSR|S_IRUSR); 
  if(fd < 0)
  {
    printf("Logfile open failed\n");
    return EXIT_FAILURE;
  }

  thread_data.fd = fd;

  pthread_create(&receiving_thread, NULL, recv_thread, (void *)&thread_data);

  sprintf(log_msg, "Thread started for recv\n");
  pthread_mutex_lock(&lock);
  rc = write(fd, log_msg, strlen(log_msg));
  pthread_mutex_unlock(&lock);
  if (rc < 0)
  {
    printf("write() failed: %d\n", errno);
  }

  pthread_create(&sending_thread, NULL, send_thread, (void *)&thread_data);

  sprintf(log_msg, "Thread started for send\n");
  pthread_mutex_lock(&lock);
  rc = write(fd, log_msg, strlen(log_msg));
  pthread_mutex_unlock(&lock);
  if (rc < 0)
  {
    printf("write() failed: %d\n", errno);
  }

//It seems that it is good candidate for daemon, will be done later

  printf("Working hard asynchronously forwarding data from udp client to tcp server\n");

  pthread_join(receiving_thread, NULL);
  pthread_join(sending_thread, NULL);

  close(fd);

  return 0;
}

