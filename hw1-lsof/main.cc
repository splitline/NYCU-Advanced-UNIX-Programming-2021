#include <getopt.h>
#include <pwd.h>
#include <sys/stat.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

template <class charT, class traits>
inline std::basic_istream<charT, traits>&
_dummy(std::basic_istream<charT, traits>& strm) {
    strm.ignore(numeric_limits<streamsize>::max(), ' ');
    return strm;
}

enum filter_opt {
    command = 0b100,
    type = 0b010,
    filename = 0b001
};

struct Filter {
    regex command_reg;   // -c
    fs::file_type type;  // -t
    regex filename;      // -f
    int enable = 0b000;
};

struct ProcData {
    string command;
    int pid;
    string username;
    fs::path path;
    ProcData(string cmd, int pid, string user)
        : command{cmd}, pid{pid}, username{user} {
        path = fs::path("/proc") / to_string(pid);
    }
};

struct FileData {
    string fd;
    fs::file_type type = fs::file_type::none;
    int inode = 0;
    string filename;

    error_code ec;

    FileData(string fd, fs::file_type type, string filename, int inode = 0)
        : fd{fd}, type{type}, inode{inode}, filename{filename} {}
    FileData(string fd, fs::path path) : fd{fd} {
        filename = fs::is_symlink(path) ? fs::read_symlink(path, ec) : path;

        if (ec) {
            filename = path;
            type = fs::file_type::unknown;
        } else {
            struct stat info;
            stat(path.c_str(), &info);
            inode = info.st_ino;
            type = fs::status(path).type();
        }

        if (filename.ends_with(" (deleted)")) type = fs::file_type::unknown;
    }
};

map<fs::file_type, string> type_to_string = {
    {fs::file_type::directory, "DIR"},
    {fs::file_type::regular, "REG"},
    {fs::file_type::character, "CHR"},
    {fs::file_type::fifo, "FIFO"},
    {fs::file_type::socket, "SOCK"},
    {fs::file_type::unknown, "unknown"},
};

string trim(string str) {
    return str.substr(str.find_first_not_of("\n\t\r "), str.find_last_not_of("\n\t\r "));
}

Filter argparser(int argc, char* argv[]) {
    int opt;
    Filter filter;
    map<string, fs::file_type> string_to_type;
    for (auto [type, str] : type_to_string) string_to_type[str] = type;

    while ((opt = getopt(argc, argv, "c:t:f:")) != -1) {
        if (opt == '?') exit(0);
        switch (opt) {
            case 'c':
                filter.command_reg = regex(optarg);
                filter.enable |= filter_opt::command;
                break;
            case 't':
                if (string_to_type.contains(optarg)) {
                    filter.type = string_to_type[optarg];
                    filter.enable |= filter_opt::type;
                }
                break;
            case 'f':
                filter.filename = regex(optarg);
                filter.enable |= filter_opt::filename;
                break;
            default:
                break;
        }
    }
    return filter;
}

void print_opened_file(ProcData proc, FileData file, Filter filter) {
    if (((filter.enable & filter_opt::filename) && !regex_search(file.filename, filter.filename)) ||
        ((filter.enable & filter_opt::type) && (filter.type != fs::file_type::none) && !(filter.type == file.type)))
        return;

    cout << proc.command << "\t" << proc.pid << "\t" << proc.username << "\t"
         << file.fd << "\t"
         << type_to_string[file.type] << "\t"
         << (file.inode > 0 ? to_string(file.inode) : " ") << "\t"
         << file.filename;

    if (fs::is_symlink(file.filename) && file.ec == errc::permission_denied)
        cout << " (readlink: Permission denied)";
    else if (fs::is_directory(file.filename) && file.ec == errc::permission_denied)
        cout << " (opendir: Permission denied)";

    cout << endl;
}

vector<FileData> parse_maps(fs::path path) {
    vector<FileData> files;
    ifstream maps_file(path / "maps");
    string line;
    set<int> occured;
    while (getline(maps_file, line)) {
        string filename, deleted;
        int inode;
        std::istringstream iss(line);

        if ((iss >> _dummy >> _dummy >> _dummy >> _dummy >> inode) && inode != 0) {
            getline(iss, filename);
            if (occured.contains(inode)) continue;
            filename = trim(filename);
            bool deleted = filename.ends_with(" (deleted)");
            files.push_back(FileData(deleted ? "del" : "mem",
                                     deleted ? fs::file_type::unknown : fs::file_type::regular,
                                     filename, inode));
            occured.insert(inode);
        }
    }
    return files;
}

vector<FileData> dump_fd(fs::path path) {
    vector<FileData> files;
    fs::path fd_path(path / "fd");

    error_code ec;
    for (auto& fd : fs::directory_iterator(fd_path, ec)) {
        string fd_name = fd.path().filename();
        fs::perms perm = fd.status().permissions();
        if ((perm & fs::perms::owner_read & fs::perms::owner_write) != fs::perms::none)
            fd_name += "u";
        else if ((perm & fs::perms::owner_read) != fs::perms::none)
            fd_name += "r";
        else if ((perm & fs::perms::owner_write) != fs::perms::none)
            fd_name += "w";

        files.push_back(FileData(fd_name, fd));
    }

    if (ec) {
        FileData file_data("NOFD", fs::file_type::none, fd_path);
        file_data.ec = ec;
        files.push_back(file_data);
    }
    return files;
}

vector<ProcData> get_procs(Filter filter) {
    vector<ProcData> processes;
    for (auto& p : fs::directory_iterator("/proc")) {
        fs::path proc_base = p.path();
        string filename = proc_base.filename();
        if (isdigit(filename[0])) {
            int pid = stoi(filename);

            string command;
            getline(ifstream(proc_base / "comm"), command);
            if ((filter.enable & filter_opt::command) && !regex_search(command, filter.command_reg)) continue;

            struct stat info;
            stat(proc_base.c_str(), &info);
            string username = getpwuid(info.st_uid)->pw_name;

            processes.push_back(ProcData(command, pid, username));
        }
    }
    return processes;
}

int main(int argc, char* argv[]) {
    Filter filter = argparser(argc, argv);
    vector<ProcData> processes = get_procs(filter);
    cout << "COMMAND\tPID\tUSER\tFD\tTYPE\tNODE\tNAME" << endl;
    for (auto proc : processes) {
        vector<FileData> files;
        for (auto special_file : {"cwd", "root", "exe"})
            files.push_back(FileData(special_file, proc.path / special_file));

        auto maps_files = parse_maps(proc.path);
        auto fd_files = dump_fd(proc.path);

        files.reserve(maps_files.size() + fd_files.size());
        files.insert(files.end(), maps_files.begin(), maps_files.end());
        files.insert(files.end(), fd_files.begin(), fd_files.end());

        for (auto of : files) print_opened_file(proc, of, filter);
    }
}