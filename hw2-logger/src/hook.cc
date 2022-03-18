#include "hook.h"

#include <dlfcn.h>
#include <sys/types.h>
#include <unistd.h>

#include "log_utils.h"

template <class FuncType, class... ArgsType>
auto call_libc_func(FuncType __type, const char *symbol, ArgsType... args) {
    static FuncType orig_func = NULL;
    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    orig_func = (FuncType)dlsym(handle, symbol);
    return orig_func(args...);
}

extern "C" {
HOOK2(int, chmod, path_str, pathname, mode_t, mode);
HOOK3(int, chown, path_str, pathname, uid_t, owner, gid_t, group);
HOOK1(fd_int, close, fd_int, fd);
HOOK2(fd_int, creat, path_str, pathname, mode_t, mode);
HOOK2(fd_int, creat64, path_str, pathname, mode_t, mode);
HOOK1(int, fclose, FILE *, stream);
HOOK2(FILE *, fopen, path_str, pathname, const char *, mode);
HOOK2(FILE *, fopen64, path_str, pathname, const char *, mode);
HOOK4(size_t, fread, void *, ptr, size_t, size, size_t, nmemb, FILE *, stream);
HOOK4(size_t, fwrite, const void *, ptr, size_t, size, size_t, nmemb, FILE *, stream);
HOOK3(fd_int, open, path_str, pathname, open_flag, flags, mode_t, mode);
HOOK3(fd_int, open64, path_str, pathname, open_flag, flags, mode_t, mode);
HOOK3(ssize_t, read, fd_int, fd, void *, buf, size_t, count);
HOOK1(int, remove, path_str, pathname);
HOOK2(int, rename, path_str, oldpath, path_str, newpath);
HOOK0(FILE *, tmpfile);
HOOK0(FILE *, tmpfile64);
HOOK3(ssize_t, write, fd_int, fd, const void *, buf, size_t, count);
}
