#include <io.h>
#include <fcntl.h>

#include <string>
#include <iostream>
#include <functional>
#include <algorithm>

using namespace std;

struct error {};
struct failed {};

bool
fdclose (int fd) noexcept
{
  cerr << "fdclose: " << fd << endl;
  return _close (fd) == 0;
}

class auto_fd
{
public:
  explicit
  auto_fd (int fd = -1) noexcept: fd_ (fd) {}

  auto_fd (auto_fd&& fd) noexcept: fd_ (fd.release ()) {}

  auto_fd (const auto_fd&) = delete;
  auto_fd& operator= (const auto_fd&) = delete;

  auto_fd&
  operator= (auto_fd&& fd) noexcept
  {
    reset (fd.release ());
    return *this;
  }

  ~auto_fd () noexcept {reset ();}

  int
  get () const noexcept {return fd_;}

  void
  reset (int fd = -1) noexcept
  {
    if (fd_ >= 0)
      fdclose (fd_);

    fd_ = fd;
  }

  int
  release () noexcept
  {
    int r (fd_);
    fd_ = -1;
    return r;
  }

  void
  close ()
  {
    if (fd_ >= 0)
    {
      bool r (fdclose (fd_));
      fd_ = -1;

      if (!r)
        throw failed ();
    }
  }

private:
  int fd_;
};

struct fdpipe
{
  auto_fd in;
  auto_fd out;

  void
  close ()
  {
    in.close ();
    out.close ();
  }
};

fdpipe
fdopen_pipe ()
{
  int pd[2];
  if (_pipe (pd, 1024, _O_TEXT) == -1)
    throw failed ();

  return fdpipe {auto_fd (pd[0]), auto_fd (pd[1])};
}

void
cmd (auto_fd fd)
{
  fd.close ();
}

struct callbacks
{
  using func = void ();

  function<func> fn;

  explicit
  callbacks (function<func> f = {}): fn (move (f)) {}
};

void
run_pipe (bool first, auto_fd)
{
  if (!first)
    throw failed ();

  fdpipe ofd;

  ofd = fdopen_pipe ();
  cerr << "pipe: " << ofd.in.get () << " " << ofd.out.get () << endl;

  callbacks cs;

  try
  {
    cmd (move (ofd.out));

    run_pipe (false, move (ofd.in));
  }
  catch (const error&)
  {
    throw failed ();
  }
}

int
main ()
{
  int r (0);

  try
  {
    run_pipe (true, auto_fd ());
  }
  catch (const failed&)
  {
    r = 1;
  }

  cerr << "exit";
  return r;
}
