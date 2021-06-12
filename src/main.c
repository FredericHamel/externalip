#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/wait.h>

#include <netdb.h>
#include <unistd.h>

#include <fcntl.h>

#include <openssl/ssl.h>

int connect_host(const char *hostname, int nport) {
  char port[6];
  int rc, sockfd;
  struct addrinfo hint;
  struct addrinfo *result, *rp;
  memset(&hint, 0, sizeof(struct addrinfo));
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;
  snprintf(port, 5, "%d", nport);

  rc = getaddrinfo(hostname, port, &hint, &result);
  if (rc) {
    fprintf(stderr, " getaddrinfo(): %s\n", gai_strerror(rc));
    return -1;
  }
  for(rp = result; rp; rp = rp->ai_next) {
    sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if(sockfd < 0) {
      continue;
    }
    if(connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
      printf("connecting to %s...\n", hostname);
      break;
    }
    close(sockfd);
  }
  freeaddrinfo(result);
  return sockfd;
}

ssize_t socket_getline(char **buffer, size_t *bufsize, int fd) {
  char c;
  char *buf = NULL;
  int buflen = 32;
  int len = 0;
  int repeat = 5;
  int rc;
  struct pollfd fds = { .fd = fd, .events = POLLIN, .revents = 0 };
  int timeout = 1000; // 1 sec.

  if (*buffer && *bufsize > 0) {
    buf = *buffer;
    buflen = *bufsize;
  } else {
    buf = malloc(buflen);
  }

  do {
    rc = poll(&fds, 1, timeout);
    if (rc < 0) {
      perror(" poll() error");
      break;
    }
    if (rc == 0) {
      repeat--;
      if (repeat) {
        perror(" poll timeout()");
        continue;
      }
      len = -1;
      break;
    }
    if (fds.revents & POLLIN) {
      rc = recv(fd, &c, 1, 0);
      if (rc > 0) {
        buf[len] = c;
        len++;
        if (len >= (buflen >> 1)) {
          buflen <<= 1;
          char *tmp = realloc(buf, buflen);
          if (tmp) {
            buf = tmp;
          } else {
            len = -1;
            break;
          }
        }
      } else {
        fprintf(stderr, "Connection closed by peer\n");
        len = -1;
        break;
      }
    }
  } while (repeat > 0 && c != '\n' && c != '\0');

  if (len < 0) {
    free(buf);
    buf = NULL;
  }
  *buffer = buf;
  *bufsize = buflen; // change
  if(buf && len > 0) {
    // terminate string.
    buf[len] = '\0';
  }
  return len;
}

int get_ip(char **ip, size_t *out_bufsize) {
  int fd = connect_host("ipv4.icanhazip.com", 80);
  //int fd = connect_host("localhost", 9000);

  if (fd < 0) {
    perror(" socket() error");
    return 1;
  }
  {
    int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }

  int timeout = 1000;
  struct pollfd fds = {
    .fd = fd,
    .events = POLLIN | POLLOUT,
    .revents = 0 };
  
  

  const char GET_MSG[] = "GET / HTTP/1.1\r\nHost: ipv4.icanhazip.com\r\nUser-Agent: SSL DUC/0.1 Linux64\r\nAccept: */*\r\nConnection: close\r\n\r\n";
  //const char GET_MSG[] = "GET / HTTP/1.1\r\nHost: localhost\r\nUser-Agent: SSL DUC/0.1 Linux64\r\nAccept: */*\r\nConnection: close\r\n\r\n";
  {
    int sent = 0;
    int ret_code;
    fds.events = POLLOUT;
    do {
      ret_code = poll(&fds, 1, timeout);
      if (ret_code < 0) {
        perror(" poll() error");
        break;
      }
      if (ret_code == 0) {
        continue;
      }
      if (fds.revents & POLLOUT) {
        ret_code = send(fd, GET_MSG+sent, sizeof(GET_MSG)-sent, 0);
        if (ret_code < 0) {
          perror(" send() error\n");
          break;
        }
        sent += ret_code;
      }
    } while(sent < sizeof(GET_MSG));
  }

  // Process HTTP header
  char *response = NULL, *tok;
  size_t bufsize;
  ssize_t len;
  int content_length = -1;
  len = socket_getline(&response, &bufsize, fd);
  if(len > 0 && !strcmp(response, "HTTP/1.1 200 OK\r\n")) {
    while((len = socket_getline(&response, &bufsize, fd)) > 0 && !(response[0] == '\r' && response[1] == '\n')) {
      printf("%s", response);
      tok = strtok(response, " \n");
      if (tok) {
        if (!strcmp(tok, "Content-Length:")) {
          tok = strtok(NULL, "\n");
          content_length = atoi(tok);
        }
      }
    }
  } else {
    if (len > 0)  {
      printf("Server response status: %s", response);
      free(response);
    }
    close(fd);
    return 1;
  }
  
  if (content_length < 0) {
    fprintf(stderr, "Content-Length not specify\n");
    len = socket_getline(&response, &bufsize, fd);
    len = socket_getline(&response, &bufsize, fd);
    if (len > 0) {
      printf("ip(%lu): %s\n", len, response);
    }
    close(fd);
    free(response);
    return 1;
  }
  if (content_length >= bufsize) {
    response = realloc(response, content_length + 1);
    *out_bufsize = content_length + 1;
  } else {
    *out_bufsize = bufsize;
  }
  response[content_length] = '\0';

  recv(fd, response, content_length, 0);
  close(fd);

  *ip = response;
  return 0;
}

int main(int argc, char **argv) {
  char *ip = NULL;
  size_t ip_len;
  int rc = 1;




  if(get_ip(&ip, &ip_len)) {
    goto ret0;
  }
  
  fprintf(stdout, "My ip is: %s\n", ip);
ret0:
  free(ip);
  return 0;
  pid_t pid = fork();
  if (pid < 0) {
    fprintf(stderr, "Failed to fork\n");
    goto ret1;
  }
  if (pid == 0) {
    execl("/usr/bin/ssh", "ssh", "hamelfre@arcade.iro.umontreal.ca", "./update-ip", ip, NULL);
    exit(1);
  }
  wait(&rc);
  if (rc != 0) {
    printf("Something went wrong\n");
  }
ret1:
  return rc;
}
