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

namespace build {

    using std::string;
    using std::vector;
    using fs::path;

    #define CMD(...) Cmd((vector<string>) {__VA_ARGS__})
    #define go_rebuild_yourself(argc, argv) build::_go_rebuild_yourself(argc, argv, __FILE__)

    struct Unit {};

    template<typename T, typename E>
    union ResultValue {
        T success;
        E error;

        ResultValue() {}
        ~ResultValue() {}
    };

    template<typename T>
    struct Ok {
        T value;
        Ok(const T& val) : value(val) {}
        Ok(T&& val) : value(std::forward<T>(val)) {}
    };

    template<typename E>
    struct Err {
        E error;
        Err(const E& err) : error(err) {}
        Err(E&& err) : error(std::forward<E>(err)) {}
    };

    template<typename T, typename E>
    class Result {
        ResultValue<T, E> value;
        bool status; // true = success, false = error
    public:
        Result(const Ok<T>& ok)   : status(true)  { new (&value.success) T(ok.value); }
        Result(const Err<E>& err) : status(false) { new (&value.error)   E(err.error); }
        Result(Ok<T>&& ok)        : status(true)  { new (&value.success) T(std::move(ok.value)); }
        Result(Err<E>&& err)      : status(false) { new (&value.error)   E(std ::move(err.error)); }

        ~Result() {
            if (status) value.success.~T();
            else value.error.~E();
        }

        bool is_ok() const { return status; }
        bool is_err() const { return !status; }

        Ok<T> get_ok() {
            if (!status) throw std::logic_error("Called `get_ok` on an error Result");
            return value.success;
        }

        Err<T> get_err() {
            if (status) throw std::logic_error("Called `get_err` on a success Result");
            return value.error;
        }

        const T& unwrap() const {
            if (!status) throw std::logic_error("Called `unwrap` on an error Result");
            return value.success;
        }

        const E& unwrap_err() const {
            if (status) throw std::logic_error("Called `unwrap_err` on a success Result");
            return value.error;
        }
    };

    class Cmd {
        vector<string> parts;

    public:
        Cmd() = default;

        Cmd (vector<string> &&parts) : parts(std::move(parts)) {}

        void push(const string &part) {
            parts.push_back(part);
        }

        void push_many(const vector<string> &parts) {
            for (const auto &part : parts) {
                this->parts.push_back(part);
            }
        }

        string render() const {
            string result;
            for (const auto &part : parts) {
                if (!result.empty()) result += " ";
                result += part;
            }
            return result;
        }

        int run(path root = "") const {
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

                // Set the current working directory if specified
                if (!root.empty()) {
                    if (chdir(root.c_str()) < 0) {
                        std::cerr << "Could not change directory to " << root << ": " << strerror(errno) << std::endl;
                        exit(EXIT_FAILURE);
                    }
                }

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

    Result<path, Unit> inline find_root(string marker_file) {
        fs::path git_root = fs::current_path();
        while (git_root != git_root.root_path()) {
            if (fs::exists(git_root / marker_file)) {
                return Ok(git_root);
            }
            git_root = git_root.parent_path();
        }
        return Err(Unit());
    }

    Result<path, string> inline git_root() {
        auto res = find_root(".git");
        if (res.is_ok()) return res.get_ok();
        else return Err<string>("You are not in a git repository.");
    }

    inline void log(string msg) {
        std::cout << msg << std::endl;
    }

    [[noreturn]]
    inline void panic(string msg) {
        std::cerr << "[ERROR] " << msg << std::endl;
        exit(1);
    }

    // Create the directory if it does not exist. Returns the absolute path of the directory.
    inline path dir(path dir) {
        if (!fs::exists(dir)) {
            log("Creating directory: " + dir.string());
            if (!fs::create_directories(dir)) {
                std::cerr << "Failed to create directory: " << dir << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        return fs::absolute(dir);
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
        path header_path     = __FILE__;

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

}
