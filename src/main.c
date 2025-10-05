#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

static void sigusr1(int __attribute__((unused)) signum) {}

int main(void) {
  if (signal(SIGUSR1, sigusr1) == SIG_ERR) {
    warn("Failed to set signal handler");
  }

  time_t t;
  struct tm tm_struct;
  char time_str[20];
  struct timespec sleep_time = {.tv_sec = 5, .tv_nsec = 0};

  while (1) {
    if (((t = time(NULL)) == (time_t)-1)) {
      perror("Failed get time");
      return 1;
    }
    if (localtime_r(&t, &tm_struct) == NULL) {
      perror("Failed convert time");
      return 1;
    }
    if (strftime(time_str, sizeof(time_str), "%m-%d %H:%M", &tm_struct) == 0) {
      fprintf(stderr, "Failed to format time");
      return 1;
    }
    printf("%s\n", time_str);
    nanosleep(&sleep_time, NULL);
  }
}
