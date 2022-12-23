#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, const char* argv[]) {
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    fprintf(2, "Error creating pipe\n");
    exit(0);
  }

  int pid = fork();
  if (pid > 0) {
    char byte = 'a';
    write(pipefd[1], &byte, 1);
    wait(0);
    char receive;
    read(pipefd[0], &receive, 1);
    fprintf(2, "%d: received pong\n", getpid(), receive);
    wait(0);
  } else if (pid == 0) {
    char byte = 'b';
    char receive;
    read(pipefd[0], &receive, 1);
    fprintf(2, "%d: received ping\n", getpid(), receive);
    write(pipefd[1], &byte, 1);
  } else {
    fprintf(2, "fork error\n");
  }

  exit(0);
}