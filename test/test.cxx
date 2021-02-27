#include <fcntl.h>
#include <io.h>
#include <assert.h>

#include <memory> // Required?

extern "C" int printf(const char *f, ...);


using namespace std;

struct error {};
struct failed {};

static __declspec(noinline) bool fdclose(int fd) noexcept {
  printf("fdclose: %d\n", fd);
  return _close(fd) == 0;
}

struct auto_fd {
  explicit auto_fd(int fd = -1) noexcept : fd_(fd) {}

  auto_fd(auto_fd &&fd) noexcept : fd_(fd.release()) {}

  auto_fd(const auto_fd &) = delete;
  auto_fd &operator=(const auto_fd &) = delete;

  auto_fd &operator=(auto_fd &&fd) noexcept;

  ~auto_fd() noexcept {
    if (fd_ >= 0) {
      fdclose(fd_);
    }

    fd_ = -1;
  }

  int release() noexcept {
    int r = fd_;
    fd_ = -1;
    return r;
  }

  __declspec(noinline) void close() {
    if (fd_ >= 0) {
      bool r(fdclose(fd_));
      fd_ = -1;
      assert(r);
    }
  }

  int fd_;
};

static __declspec(noinline) void move_and_close(auto_fd fd) { fd.close(); }

struct HasDtor {
  HasDtor() {}
  __declspec(noinline) ~HasDtor() { printf("~HasDtor\n"); }
};

static void __declspec(noinline) move_and_throw(auto_fd fd) {
  printf("throw 1\n");
  throw failed();
}

void run_pipe() {
  int pd[2];
  int r = _pipe(pd, 1024, _O_TEXT);
  assert(r != -1 && "pipe failed");

  auto_fd in(pd[0]);
  auto_fd out(pd[1]);

  printf("pipe: %d %d\n", in.fd_, out.fd_);

  HasDtor cleanup;

  try {
    move_and_close(move(out));
    move_and_throw(move(in));
  } catch (const error &) {
    printf("throw 2\n");
    throw failed();
  }
}

int main() {
  int r(0);

  try {
    run_pipe();
  } catch (const failed &) {
    r = 1;
  }

  printf("exit\n");
  return r;
}
