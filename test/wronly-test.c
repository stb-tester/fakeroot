#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>


static void check_fopen_mode(mode_t mode, int flags);
static void check_fchmod_mode(mode_t creat_mode, int flags, mode_t newmode);

int main(int argc, char* argv[])
{
  check_fopen_mode(0644, O_WRONLY|O_CREAT|O_EXCL);
  check_fopen_mode(0644, O_RDWR|O_CREAT);
  check_fopen_mode(0755, O_WRONLY|O_CREAT|O_EXCL);
  check_fopen_mode(0755, O_RDWR|O_CREAT);
  check_fopen_mode(0, O_WRONLY|O_CREAT|O_EXCL);
  check_fopen_mode(0, O_RDWR|O_CREAT);

  check_fchmod_mode(0644, O_WRONLY|O_CREAT|O_EXCL, 0644);
  check_fchmod_mode(0644, O_RDWR|O_CREAT, 0644);
  check_fchmod_mode(0755, O_WRONLY|O_CREAT|O_EXCL, 0755);
  check_fchmod_mode(0755, O_RDWR|O_CREAT, 0755);
  check_fchmod_mode(0, O_WRONLY|O_CREAT|O_EXCL, 0);
  check_fchmod_mode(0, O_RDWR|O_CREAT, 0);

  check_fchmod_mode(0, O_WRONLY|O_CREAT, 0644);
  check_fchmod_mode(0, O_RDWR|O_CREAT, 0644);
  check_fchmod_mode(0, O_RDONLY|O_CREAT, 0644);
}

static void check_fopen_mode(mode_t mode, int flags)
{
  int fd, err;
  struct stat buf;

  fd = open("test.tmp", flags, mode);
  assert(fd >= 0);

  err = fstat(fd, &buf);
  assert(!err);

  close(fd);
  unlink("test.tmp");

  printf("check_fopen_mode(%04o, %i) = %o\n", mode, flags, buf.st_mode);

  assert((buf.st_mode & 07777) == mode);
}

static void check_fchmod_mode(mode_t creat_mode, int flags, mode_t newmode)
{
  int fd, err;
  struct stat buf;

  printf("\n=========================\n\n");

  fd = open("test.tmp", flags, creat_mode);
  assert(fd >= 0);

  err = fchmod(fd, newmode);
  assert(!err);

  printf("\n");

  err = fstat(fd, &buf);
  assert(!err);

  close(fd);
  unlink("test.tmp");

  printf("fchmod_test(%04o, %i, %04o) = %o\n", creat_mode, flags, newmode, buf.st_mode);
  assert((buf.st_mode & 07777) == newmode);
}
