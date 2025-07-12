#include <cstring>
#include <filesystem>
#include <iostream>
#include <cassert>
#include <unistd.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>

namespace fs = std::filesystem;

using std::string;
using std::vector;
using fs::path;

#define CMD(...) Cmd((vector<string>) {__VA_ARGS__})

class Cmd {
    vector<string> parts;

public:
    Cmd() = default;

    Cmd(const string &cmd) {

        // Iterate through the string and split by spaces
        for (size_t start = 0, end = 0; end != string::npos; start = end + 1) {
            end = cmd.find(' ', start);
            parts.push_back(cmd.substr(start, end - start));
        }

    }

    Cmd (vector<string> &&parts) : parts(std::move(parts)) {}

    Cmd &operator<<(const string &part) {
        parts.push_back(part);
        return *this;
    }

    string render() const {
        string result;
        for (const auto &part : parts) {
            if (!result.empty()) result += " ";
            result += part;
        }
        return result;
    }

    int run() const {
        if (parts.empty() || parts[0].empty()) {
            std::cerr << "No command to run." << std::endl;
            return -1;
        }

        std::cout << "CMD: " << render() << std::endl;

        pid_t cpid = fork();
        if (cpid < 0) {
            std::cerr << "Could not fork process: " << strerror(errno) << std::endl;
            return -1;
        }

        if (cpid == 0) {
            vector<char *> args;
            for (auto &part : parts) {
                args.push_back(const_cast<char *>(part.c_str()));
            }
            args.push_back(nullptr);

            if (execvp(args[0], args.data()) < 0) {
                std::cerr << "Could not exec child process: " << strerror(errno) << std::endl;
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
        }

        int status;
        waitpid(cpid, &status, 0);

        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            return exit_status;
        } else {
            std::cerr << "Child process did not terminate normally." << std::endl;
            return -1;
        }

    }

    void clear() {
        parts.clear();
    }
};

inline void log(string msg) {
    std::cout << msg << std::endl;
}

inline void rebuild_yourself(fs::path bin, fs::path src) {

    string src_str = src.string();
    string bin_str = bin.string();

    auto compile_cmd = CMD("g++", src_str, "-o", bin_str);

    int status = compile_cmd.run();

    if (status != 0) {
        std::cerr << "Rebuild failed with status: " << status << std::endl;
        exit(EXIT_FAILURE);
    }
}

inline int run_yourself(fs::path bin) {
    fs::path bin_path = fs::relative(bin);
    auto run_cmd = CMD("./" + bin_path.string());
    std::cout << std::endl;
    log("Running...");
    return run_cmd.run();
}

inline void _go_rebuild_yourself(int argc, char* argv[], path source_file_name) {
    assert(argc > 0 && "No program provided via argv[0]");

    path root = fs::current_path();

    path executable_path = fs::relative(argv[0], root);
    path source_path     = fs::relative(source_file_name, root);
    path header_path     = fs::relative(source_file_name.replace_extension(".hpp"), root);

    bool rebuild_needed = false;

    assert(fs::exists(executable_path) && "Executable should exist, is running right now!");

    struct stat stats;

    if (stat(executable_path.c_str(), &stats) != 0) {
        std::cerr << "Could not stat executable: " << strerror(errno) << std::endl;
        return;
    }

    time_t exe_mtime = stats.st_mtime;

    if (stat(source_path.c_str(), &stats) != 0) {
        std::cerr << "Could not stat source file: " << strerror(errno) << std::endl;
        return;
    }

    time_t src_mtime = stats.st_mtime;

    rebuild_needed |= exe_mtime < src_mtime;

    if (stat(header_path.c_str(), &stats) != 0) {
        std::cerr << "Could not stat header file: " << strerror(errno) << std::endl;
        return;
    }

    time_t hdr_mtime = stats.st_mtime;

    rebuild_needed |= exe_mtime < hdr_mtime;

    if (rebuild_needed) {
        log("Rebuilding the executable...");
        rebuild_yourself(executable_path, source_path);
        exit(run_yourself(executable_path));
    }

}

#define go_rebuild_yourself(argc, argv) _go_rebuild_yourself(argc, argv, __FILE__)

