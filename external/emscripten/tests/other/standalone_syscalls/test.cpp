#include <assert.h>
#include <emscripten/html5.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
  // open of standard streams works.
  int fd_in = open("/dev/stdin", 0);
  assert(fd_in == 0);
  int fd_out = open("/dev/stdout", 0);
  assert(fd_out == 1);
  int fd_err = open("/dev/stderr", 0);
  assert(fd_err == 2);
  // ioctl and fcntl fail
  int i = ioctl(1, 0);
  assert(i < 0);
  int f = fcntl(1, 0);
  assert(f < 0);
  // write to standard streams works.
  write(1, "hello, world!", 5);
  write(1, "\n", 1);
  // emscripten_log API works
  emscripten_console_log("log");
  // warnings/errors go to stderr, and are not noticed in this test
  emscripten_console_warn("warn");
  emscripten_console_error("error");
  emscripten_console_log("log2");
}
