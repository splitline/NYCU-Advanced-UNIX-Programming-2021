#include <unistd.h>

#include <algorithm>
#include <climits>
#include <filesystem>
#include <string>

int __dup_stderr = dup(STDERR_FILENO);

#define outfd (lseek(STDERR_FILENO, 0, SEEK_CUR) == -1 && errno == EBADF) ? __dup_stderr : STDERR_FILENO

namespace logger {
using namespace std;
namespace fs = std::filesystem;

static bool logging = true;

string quote(string s) {
    replace_if(
        s.begin(), s.end(), [](auto c) { return !isprint(c); }, '.');
    return "\"" + s + "\"";
}

string fd_to_path(int fd) {
    string proc_fd = "/proc/self/fd/" + to_string(fd);
    if (fs::exists(proc_fd) && fs::is_symlink(proc_fd))
        return fs::read_symlink(proc_fd).string();
    return proc_fd;
}

string arg_stringify(const string type, FILE *arg) {
    return quote(fd_to_path(arg->_fileno));
}

string arg_stringify(const string type, const char *arg) {
    if (type == "path_str") {
        char buf[PATH_MAX] = {0};
        if (realpath(arg, buf) != NULL) return quote(buf);
        return quote(arg);
    }
    return quote(string(arg).substr(0, 0x20));
}

string arg_stringify(const string type, const void *arg) {
    return quote(string((const char *)arg).substr(0, 0x20));
}

string arg_stringify(const string type, int arg) {
    if (type == "fd_int") return quote(fd_to_path(arg));
    if (type == "mode_t" || type == "open_flag") {
        char buf[0x20] = {0};
        sprintf(buf, "%03o", arg);
        return buf;
    }
    return to_string(arg);
}

void start_log(string function_name) {
    logging = false;
    dprintf(outfd, "[logger] %s(", function_name.c_str());
    logging = true;
}

void log_argument(const string type, auto &arg, bool final = false) {
    logging = false;
    dprintf(outfd, "%s", arg_stringify(type, arg).c_str());
    if (!final) dprintf(outfd, ", ");
    logging = true;
}

void log_return(const string type, auto arg) {
    logging = false;
    if (type[type.length() - 1] == '*')  // is a pointer
        dprintf(outfd, ") = %p\n", arg);
    else
        dprintf(outfd, ") = %d\n", arg);
    logging = true;
}
}  // namespace logger
