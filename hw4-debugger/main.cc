#include <getopt.h>

#include <iostream>
#include <tuple>

#include "sdb.hpp"

std::tuple<string, string> argparser(int argc, char* argv[]) {
    int opt;
    std::string script_path = "";
    while ((opt = getopt(argc, argv, "s:")) != -1) {
        if (opt == '?') exit(0);
        switch (opt) {
            case 's':
                script_path = optarg;
                break;
            default:
                break;
        }
    }
    if (argc > optind)
        return {script_path, argv[optind]};

    return {script_path, ""};
}

int main(int argc, char* argv[]) {
std:;
    auto [script_path, program] = argparser(argc, argv);
    Sdb sdb;
    sdb.launch(program, script_path);
    return 0;
}
