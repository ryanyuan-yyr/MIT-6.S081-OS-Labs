#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

char* parse_arg_line(int in_fd, char* buf, int buf_size) {
  char c;
  while (1) {
    int read_ret = read(in_fd, &c, 1);
    if (read_ret == -1) {
      fprintf(2, "Error reading from the standard input\n");
      return 0;
    }
    if (read_ret == 1 && c != '\n' && buf_size >= 1) {
      *buf++ = (c == ' ' ? 0 : c);
      buf_size--;
    } else {
      break;
    }
  }
  *buf = 0;
  return buf;
}

char* find(char* str, char c) {
  for (int i = 0; i < strlen(str); i++) {
    if (str[i] == c) {
      return str + i;
    }
  }
  return 0;
}

int build_argv(char** argv, char* start, char* end, int max_arg_num) {
  char* cur = start;
  int argc = 0;
  while (argc < max_arg_num && cur < end) {
    argv[argc++] = cur;
    cur += strlen(cur) + 1;
  }
  return argc;
}

int xargs(int in_fd, char* exe_name, char** default_argv, int default_argc) {
  char line[512];
  char* line_end;
  char* args[MAXARG + 1] = {exe_name};
  char** argv = args + 1;
  for (int i = 0; i < default_argc; i++) {
    argv[i] = default_argv[i];
  }

  while ((line_end = parse_arg_line(in_fd, line,
                                    sizeof(line) - (line - line))) != line) {
    if (line_end == 0) {
      fprintf(2, "line too long\n");
      return -1;
    }
    int arg_num = default_argc + build_argv(argv + default_argc, line, line_end,
                                            MAXARG - default_argc);
    argv[arg_num] = 0;
    // printf("Going to fork, %s\n", exe_name);
    // for (int i = 0; i <= arg_num; i++) {
    //   printf("%d: %s\n", i, args[i]);
    // }

    int pid = fork();
    if (pid == 0) {
      exec(exe_name, args);
    } else {
      wait(0);
    }
  }
  return 0;
}

int main(int argc, char* argv[]) {
  xargs(0, argv[1], argv + 2, argc - 2);
  exit(0);
}