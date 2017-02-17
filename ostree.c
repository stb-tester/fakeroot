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

/*
 * Useful for debugging
 */
static void fprint_file_locator(FILE *stream, file_locator locator)
{
  int saved_errno = errno;
  char buf[PATH_MAX+1] = {0};
  char fd_path[] = "/proc/self/fd/4000000000";

  if (locator.flags & AT_EMPTY_PATH &&
      strcmp("", locator.pathname) == 0 &&
      locator.dirfd != AT_FDCWD) {
    sprintf(fd_path, "/proc/self/fd/%i", locator.dirfd);
    if (readlink(fd_path, buf, PATH_MAX) > 0)
      fprintf(stream, "floc(%i (\"%s\"))", locator.dirfd, buf);
    else
      fprintf(stream, "floc(%i (\?\?\?))", locator.dirfd);
  } else if (locator.dirfd == AT_FDCWD) {
    if (locator.flags & AT_SYMLINK_NOFOLLOW) {
      fprintf(stream, "lloc(\"%s\")", locator.pathname);
    } else {
      fprintf(stream, "loc(\"%s\")", locator.pathname);
    }
  }
  else {
    sprintf(fd_path, "/proc/self/fd/%i", locator.dirfd);
    if (readlink(fd_path, buf, PATH_MAX) > 0)
      fprintf(stream, "flocat(%i (\"%s\"), \"%s\", %x)", locator.dirfd, buf,
          locator.pathname, locator.flags);
    else
      fprintf(stream, "flocat(%i (\?\?\?), \"%s\", %x)", locator.dirfd, buf,
          locator.pathname, locator.flags);
  }
}

static int read_ostreemeta(file_locator locator, uint32_t *buf)
{
  unsigned char sbuf[400];
  ssize_t size;

  size = loc_getxattr(locator, "user.ostreemeta", sbuf, sizeof (sbuf));
  if (size > 0) {
    if (size >= (ssize_t) 3 * sizeof(uint32_t)) {
      memcpy (buf, sbuf, 3 * sizeof(uint32_t));
      return 0;
    } else {
      fprintf(stderr, "FAKEROOT: Warning: File ");
      fprint_file_locator(stderr, locator);
      fprintf(stderr, " has user.ostreemeta xattr but it is only %i bytes.  "
          "Minimum size to hold uid, gid and mode is %i bytes.\n",
          (int) size, 3 * sizeof(uint32_t));
    }
  } else if (errno == ENODATA || errno == ENOTSUP) {
    /* pass */
  } else {
    /* TODO: Handle ERANGE with malloc/retry loop */
    int saved_errno = errno;
    fprintf(stderr, "FAKEROOT: Failed to read stat on file ");
    fprint_file_locator(stderr, locator);
    fprintf(stderr, ": %s\n", strerror(saved_errno));
  }

  memset(buf, 0, 3 * sizeof(uint32_t));
  return -1;
}

void send_get_stat_int(file_locator locator, INT_STRUCT_STAT *st)
{
  uint32_t buf[3];

  if (st->st_mode & S_IRUSR && read_ostreemeta(locator, buf) == 0) {
    /* We need read permissions to call getxattr, but it's ok if we don't have
     * them, we almost certainly haven't attached our xattrs to the inode if we
     * don't as our chmod always | 0600 */
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

  if (loc_setxattr(locator, "user.ostreemeta", buf, 3 * sizeof(uint32_t), 0) < 0) {
    int saved_errno = errno;
    fprintf(stderr, "FAKEROOT: Failed to update stat on file ");
    fprint_file_locator(stderr, locator);
    fprintf(stderr, " to (uid: %u, gid: %u, mode: %o): %s\n",
        buf[0], buf[1], (int) buf[2], strerror(saved_errno));
  }
}

static void send_chown(file_locator locator, INT_STRUCT_STAT *orig, uid_t owner, gid_t group)
{
  INT_STRUCT_STAT before = *orig, updated;
  send_get_stat_int(locator, &before);
  updated = before;

  if (owner != -1)
    updated.st_uid = owner;
  if (group != -1)
    updated.st_gid = group;

  /* Only save if changed */
  if (before.st_uid != updated.st_uid || before.st_gid != updated.st_gid)
    save_stat(locator, &updated);
}

static void send_chmod(file_locator locator, INT_STRUCT_STAT *orig, mode_t mode)
{
  INT_STRUCT_STAT ost = *orig;
  int tmperrno = errno;
  send_get_stat_int(locator, &ost);

  ost.st_mode = (mode & ~S_IFMT) | (ost.st_mode & S_IFMT);
  if (ost.st_mode != orig->st_mode)
    save_stat(locator, &ost);
  errno = tmperrno;
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
