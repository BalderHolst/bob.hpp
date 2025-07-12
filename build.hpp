#include <cstring>
#include <filesystem>
#include <iostream>
#include <cassert>
#include <unistd.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using std::string;
using std::vector;
using fs::path;

inline void log(string msg) {
    std::cout << msg << std::endl;
}

inline void rebuild_yourself(fs::path bin, fs::path src) {

    string src_str = src.string();
    string bin_str = bin.string();

    const char * args[] = {
        (char *) "g++",
        src_str.c_str(),
        (char *) "-o",
        bin_str.c_str(),
        NULL,
    };

    std::cout << "CMD: ";
    for (auto& arg : args) std::cout << arg << " ";
    std::cout << std::endl;
    char const* program = "g++";

    pid_t cpid = fork();
    if (cpid < 0) {
        std::cout << "Could not fork process: " << strerror(errno) << std::endl;
        return;
    }

    if (cpid == 0) {
        if (execvp(program, (char * const*) args) < 0) {
            std::cerr << "Could not exec child process for " << program << ": " << strerror(errno) << std::endl;
            return;
        }
    }
}

inline void _go_rebuild_yourself(int argc, char* argv[], path source_file_name) {
    assert(argc > 0 && "No arguments provided");

    path root = fs::canonical(argv[0]).parent_path();

    path executable_path = fs::canonical(argv[0]);
    path source_path     = root / source_file_name;
    path header_path     = root / __FILE__;

    std::cout << "Exe: " << executable_path << std::endl;
    std::cout << "Src: " << source_path     << std::endl;
    std::cout << "Hdr: " << header_path     << std::endl;

    bool rebuild_needed = false;

    assert(fs::exists(executable_path) && "Executable should exist, is running right now!");

    if (rebuild_needed) {
        log("Rebuilding the executable...");
        rebuild_yourself(executable_path, source_path);
    }

}

#define go_rebuild_yourself(argc, argv) _go_rebuild_yourself(argc, argv, __FILE__)

