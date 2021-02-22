#include <iostream>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string>
#include <string.h>
#include <sstream>
#include <vector>
#include <pthread.h>
#include <unordered_map>
#include <unordered_set>

#define BUFFER_SIZE 20000

std::unordered_map<std::string, std::unordered_set<int>> chatrooms;

struct args
{
  int newsockfd;
};

void error(const char *msg)
{
  printf("%s", msg);
  // exit(0);
}

void sendMessage(const std::string &room, const std::string &msg)
{
  for (auto it = chatrooms[room].begin(); it != chatrooms[room].end(); it++)
  {
    if (write(*it, msg.c_str(), msg.size()) < 0)
    {
      error("Error writing message");
    }
  }
}

void clientLeft(const std::string &room, const std::string &name,
                const int &newsockfd)
{
  printf("Client disconnected\n");
  chatrooms[room].erase(newsockfd);
  close(newsockfd);
  if (chatrooms.find(room) != chatrooms.end())
    sendMessage(room, name + " has left.\n");
  if (chatrooms[room].size() == 0)
  {
    chatrooms.erase(room);
  }
}

void clientError(const char *msg, int newsockfd)
{
  int n = write(newsockfd, msg, strlen(msg));
  if (n < 0)
    error("ERROR writing to socket");
  close(newsockfd);
}

void *handleClient(void *vargp)
{
  int n;
  int newsockfd = *(int *)vargp;
  char buffer[BUFFER_SIZE];
  if (newsockfd < 0)
    error("ERROR on accept");
  bzero(buffer, BUFFER_SIZE);
  n = read(newsockfd, buffer, BUFFER_SIZE);
  if (n < 0)
  {
    perror("Error reading from socket\n");
    return NULL;
  }
  std::string req = buffer;
  std::stringstream ss(req);
  std::string item;
  std::vector<std::string> args;
  while (std::getline(ss, item, ' '))
  {
    args.push_back(item);
  }
  for (int i = 0; i < args[0].size(); i++)
  {
    args[0][i] = tolower(args[0][i]);
  }
  if (args.size() != 3 || args[0] != "join")
  {
    clientError("Invalid join format.\n", newsockfd);
    return NULL;
  }

  // add to room
  std::string room = args[1];
  std::string name = args[2];
  name.erase(std::remove_if(name.begin(), name.end(), isspace), name.end());
  chatrooms[room].insert(newsockfd);
  // send username as joined to all clients in room
  std::string join_msg = name + " has joined.\n";
  sendMessage(room, join_msg);
  while (1)
  {
    bzero(buffer, BUFFER_SIZE);
    n = read(newsockfd, buffer, BUFFER_SIZE);
    if (n == 0)
    {
      clientLeft(room, name, newsockfd);
      return NULL;
    }
    std::string msg = buffer;
    msg = name + ": " + msg;
    sendMessage(room, msg);
    if (n < 0)
    {
      clientLeft(room, name, newsockfd);
      return NULL;
    }
  }
  return NULL;
}

int main(int argc, char const *argv[])
{
  int sockfd, portno = 1234;
  struct sockaddr_in serv_addr, cli_addr;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");
  bzero((char *)&serv_addr, sizeof(serv_addr));
  if (argc > 1)
  {
    portno = atoi(argv[1]);
  }
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *)&serv_addr,
           sizeof(serv_addr)) < 0)
    error("ERROR on binding");
  listen(sockfd, 5);
  while (1)
  {
    socklen_t clilen = sizeof(cli_addr);
    int newsockfd = accept(sockfd,
                           (struct sockaddr *)&cli_addr,
                           &clilen);
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, handleClient, &newsockfd);
  }
  close(sockfd);
  return 0;
}