#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void worker(int in_fd);

int main(int argc, const char* argv[]) {
  int max = 35;
  if (argc == 2) {
    max = atoi(argv[1]);
  }
  int cal_pipe_fd[2];
  pipe(cal_pipe_fd);
  int pid = fork();
  if (pid == 0) {
    close(cal_pipe_fd[1]);
    worker(cal_pipe_fd[0]);
  } else if (pid > 0) {
    close(cal_pipe_fd[0]);
    for (int i = 2; i <= max; i++) {
      write(cal_pipe_fd[1], &i, sizeof(int));
    }
    close(cal_pipe_fd[1]);
  } else {
    fprintf(2, "Error forking\n");
  }
  wait(0);
  exit(0);
}

void worker(int in_fd_arg) {
  int prime;
  int cal_pipe_fd[2];
  int next;
  int in_fd = in_fd_arg;
start:
  if (read(in_fd, &prime, sizeof(int)) != sizeof(int)) {
    fprintf(2, "%d Partial read prime %d\n", getpid(), -1);
    exit(0);
  }
  printf("prime %d\n", prime);
  cal_pipe_fd[0] = -1, cal_pipe_fd[1] = -1;
  int read_len;
  while ((read_len = read(in_fd, &next, sizeof(int))) == sizeof(int)) {
    if (next % prime == 0) {
      continue;
    }
    if (cal_pipe_fd[0] != -1 && cal_pipe_fd[1] != -1) {
      write(cal_pipe_fd[1], &next, sizeof(int));
    } else {
      pipe(cal_pipe_fd);
      int pid = fork();
      if (pid == 0) {
        close(cal_pipe_fd[1]);
        in_fd = cal_pipe_fd[0];
        goto start;
      } else if (pid > 0) {
        close(cal_pipe_fd[0]);
        if (write(cal_pipe_fd[1], &next, sizeof(int)) != sizeof(int)) {
          printf("%d error writing\n", getpid());
        }
      } else {
        fprintf(2, "Error forking\n");
      }
    }
  }
  if (read_len != 0) {
    printf("%d Partial read: %d read, -1 %d\n", getpid(), read_len, -1);
  }
  if (cal_pipe_fd[0] != -1 && cal_pipe_fd[1] != -1) {
    close(cal_pipe_fd[1]);
    wait(0);
  }
  exit(0);
}