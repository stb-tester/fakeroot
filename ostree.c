#include "communicate.h"
#include <sys/types.h>
#include <attr/xattr.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/sysmacros.h>

static int loc_open(file_locator locator, int flags)
{
  int fd = -1;
  if (locator.flags & AT_EMPTY_PATH &&
      strcmp("", locator.pathname) == 0 &&
      locator.dirfd != AT_FDCWD) {
    fd = dup(locator.dirfd);
  } else {
    fd = openat(locator.dirfd, locator.pathname,
                locator.flags | flags | O_CLOEXEC);
  }
  return fd;
}

static int loc_setxattr(file_locator locator,
                        const char *name,
                        const void *value,
                        size_t size,
                        int flags)
{
  /* There is no fsetxattrat so we implement it ourselves here.  We try to avoid
   * the openat path if possible so there's less that can go wrong. */
  if (locator.flags & AT_EMPTY_PATH &&
      strcmp("", locator.pathname) == 0 &&
      locator.dirfd != AT_FDCWD) {
    return fsetxattr(locator.dirfd, name, value, size, flags);
  } else if (locator.dirfd == AT_FDCWD) {
    if (locator.flags & AT_SYMLINK_NOFOLLOW) {
      return lsetxattr(locator.pathname, name, value, size, flags);
    } else {
      return setxattr(locator.pathname, name, value, size, flags);
    }
  }
  else {
    int tmperrno;
    int r;
    int fd = openat(locator.dirfd, locator.pathname, locator.flags | O_CLOEXEC);
    if (fd < 0) {
      perror("FAKEROOT: write_ostreemeta failed: Couldn't open locator");
      return -1;
    }
    r = fsetxattr(fd, name, value, size, flags);
    tmperrno = errno;
    close (fd);
    errno = tmperrno;
    return r;
  }
}

static ssize_t loc_getxattr(file_locator locator,
                            const char *name,
                            void *value,
                            size_t size)
{
  if (fakeroot_debug)
    fprintf(stderr, "loc_getxattr(" LOC_FMT ", %s, ..., %i)\n",
            LOC_FARG(locator), name, (int) size);
  /* There is no fgetxattrat so we implement it ourselves here.  We try to avoid
   * the openat path if possible so there's less that can go wrong. */
  if (locator.flags & AT_EMPTY_PATH &&
      strcmp("", locator.pathname) == 0 &&
      locator.dirfd != AT_FDCWD) {
    return fgetxattr(locator.dirfd, name, value, size);
  } else if (locator.dirfd == AT_FDCWD) {
    if (locator.flags & AT_SYMLINK_NOFOLLOW) {
      return lgetxattr(locator.pathname, name, value, size);
    } else {
      return getxattr(locator.pathname, name, value, size);
    }
  }
  else {
    int r;
    int tmperrno;
    int fd = openat(locator.dirfd, locator.pathname, locator.flags | O_CLOEXEC);
    if (fd < 0) {
      perror("FAKEROOT: write_ostreemeta failed: Couldn't open locator");
      return -1;
    }
    r = fgetxattr(fd, name, value, size);
    tmperrno = errno;
    close (fd);
    errno = tmperrno;
    return r;
  }
}

static int read_ostreemeta(file_locator locator, uint32_t *buf)
{
  unsigned char sbuf[40];
  ssize_t size;

  size = loc_getxattr(locator, "user.ostreemeta", sbuf, sizeof (sbuf));
  if (size > 0 && size >= (ssize_t) 3 * sizeof(uint32_t)) {
    memcpy (buf, sbuf, 3 * sizeof(uint32_t));
    return 0;
  } if (errno == ENODATA) {
    /* pass */
  } else {
    /* TODO: Handle ERANGE with malloc/retry loop */
    perror("FAKEROOT: Failed to read stat from fd");
  }

  memset(buf, 0, 3 * sizeof(uint32_t));
  return -1;
}

void send_get_stat_int(file_locator locator, INT_STRUCT_STAT *st)
{
  uint32_t buf[3];

  if (read_ostreemeta(locator, buf) == 0) {
    st->st_uid = ntohl(buf[0]);
    st->st_gid = ntohl(buf[1]);
    st->st_mode = ntohl(buf[2]);
  } else {
    st->st_uid = 0;
    st->st_gid = 0;
  }
  if (fakeroot_debug)
    fprintf(stderr, "send_get_stat_int(" LOC_FMT ") -> uid: %i, gid: %i, "
            "mode: %o\n", LOC_FARG(locator), (int) st->st_uid, (int) st->st_gid,
            (int) st->st_mode);
}

void send_get_stat(file_locator locator, struct stat *st)
{
  INT_STRUCT_STAT int_st;
  int_st.st_uid = st->st_uid;
  int_st.st_gid = st->st_gid;
  int_st.st_mode = st->st_mode;
  send_get_stat_int(locator, &int_st);
  st->st_uid = int_st.st_uid;
  st->st_gid = int_st.st_gid;
  st->st_mode = int_st.st_mode;
}
void send_get_stat64(file_locator locator, struct stat64 *st)
{
  INT_STRUCT_STAT int_st;
  int_st.st_uid = st->st_uid;
  int_st.st_gid = st->st_gid;
  int_st.st_mode = st->st_mode;
  send_get_stat_int(locator, &int_st);
  st->st_uid = int_st.st_uid;
  st->st_gid = int_st.st_gid;
  st->st_mode = int_st.st_mode;
}

static void save_stat(file_locator locator, INT_STRUCT_STAT *st)
{
  uint32_t buf[3];

  buf[0] = htonl(st->st_uid);
  buf[1] = htonl(st->st_gid);
  buf[2] = htonl(st->st_mode);

  if (loc_setxattr(locator, "user.ostreemeta", buf, 3 * sizeof(uint32_t), 0) < 0)
    perror("FAKEROOT: Failed to update stat on file");
}

static void send_chown(file_locator locator, INT_STRUCT_STAT *orig, uid_t owner, gid_t group)
{
  INT_STRUCT_STAT ost = *orig;
  send_get_stat_int(locator, &ost);

  if (owner != -1)
    ost.st_uid = owner;
  if (group != -1)
    ost.st_gid = group;
  save_stat(locator, &ost);
}

static void send_chmod(file_locator locator, INT_STRUCT_STAT *orig, mode_t mode)
{
  INT_STRUCT_STAT ost = *orig;
  send_get_stat_int(locator, &ost);

  ost.st_mode = (mode & ~S_IFMT) | (ost.st_mode & S_IFMT);
  save_stat(locator, &ost);
}

static void send_mknod(file_locator locator, INT_STRUCT_STAT *orig, mode_t mode, dev_t dev)
{
  int fd, len;
  char buf[] = "XXXXXXXXXX:XXXXXXXXXX";
  INT_STRUCT_STAT ost = *orig;

  if ((fd = loc_open(locator, O_RDWR)) < 0) {
    perror("FAKEROOT: Couldn't write mknod numbers: open failed");
    return;
  }

  len = snprintf(buf, sizeof(buf), "%i:%i", major(dev), minor(dev));
  if (write(fd, buf, len) != len)
     perror("FAKEROOT: Couldn't write mknod numbers: write failed");
  close(fd);

  send_get_stat_int(locator, &ost);
  ost.st_mode = mode;
  save_stat(locator, &ost);
}

static void send_unlink(file_locator locator, INT_STRUCT_STAT *st)
{
  /* noop */
}

const char *env_var_set(const char *env){
  const char *s;

  s=getenv(env);

  if(s && *s)
    return s;
  else
    return NULL;
}
