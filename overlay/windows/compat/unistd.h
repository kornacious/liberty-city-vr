#pragma once
#include "RuntimeCompat.h"
inline int chdir(const char *path) { return _chdir(path); }
inline char *getcwd(char *out, size_t size) { return _getcwd(out, int(size)); }
inline int close(int fd) { return _close(fd); }
inline int read(int fd, void *out, unsigned size) { return _read(fd,out,size); }
inline int write(int fd, const void *data, unsigned size) { return _write(fd,data,size); }
inline int unlink(const char *path) { return _unlink(path); }
inline int access(const char *path, int mode) { return _access(path,mode); }
inline int usleep(unsigned us) { Sleep((us+999)/1000); return 0; }
inline int fsync(int fd) { return _commit(fd); }
