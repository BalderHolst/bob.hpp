#include <cstring>
#include <filesystem>
#include <iostream>
#include <cassert>
#include <unistd.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>
#include <queue>

namespace fs = std::filesystem;

namespace bob {

    using std::string;
    using std::vector;
    using fs::path;

    #define CMD(...) Cmd((vector<string>) {__VA_ARGS__})
    #define go_rebuild_yourself(argc, argv) bob::_go_rebuild_yourself(argc, argv, __FILE__)

    inline void log(string msg) {
        std::cout << msg << std::endl;
    }

    [[noreturn]]
    inline void panic(string msg) {
        std::cerr << "[ERROR] " << msg << std::endl;
        exit(1);
    }

    inline string I(path p) { return "-I" + p.string(); }

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

    struct CmdFuture {
        pid_t cpid;
        bool done;
        int exit_status;

        CmdFuture() : cpid(-1), done(false), exit_status(-1) {}

        int await() {
            int status;
            if (!done) {
                waitpid(cpid, &status, 0);
                if (WIFEXITED(status)) {
                    exit_status = WEXITSTATUS(status);
                } else {
                    panic("Child process did not terminate normally.");
                }
            }

            done = true;
            return exit_status;
        }

        bool poll() {
            if (done) return true;
            int status;
            pid_t result = waitpid(cpid, &status, WNOHANG);

            if (result == -1) panic("Error while polling child process: " + string(strerror(errno)));

            if (result == 0) return false;

            done = true;
            if (WIFEXITED(status)) exit_status = WEXITSTATUS(status);
            else panic("Child process did not terminate normally.");
            return true;
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

        CmdFuture run_async(path root = "") const {
            if (parts.empty() || parts[0].empty()) {
                panic("No command to run.");
            }

            std::cout << "CMD: " << render() << std::endl;

            pid_t cpid = fork();
            if (cpid < 0) {
                panic("Could not fork process: " + string(strerror(errno)));
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

            CmdFuture future;

            return future;
        }

        int run(path root = "") const {
            return run_async(root).await();
        }

        void clear() {
            parts.clear();
        }
    };

    class CmdRunner {
        std::queue<Cmd, std::deque<Cmd>> cmd_queue;
        size_t process_count;
        vector<CmdFuture> slots;

        bool populate_slots() {
            bool did_work = false;
            for (size_t i = 0; i < process_count; ++i) {
                bool fut_done = slots[i].poll();
                if (!fut_done) continue;

                if (cmd_queue.empty()) continue;

                // Populate slot with a new command
                auto cmd = cmd_queue.front();
                cmd_queue.pop();
                slots[i] = cmd.run_async();
                did_work = true;
            }
            return did_work;
        }

        void await_slots() {
            for (CmdFuture &slot : slots) {
                slot.await();
            }
        }

    public:
        CmdRunner(size_t process_count):
            process_count(process_count),
            slots(vector<CmdFuture>(process_count))
        {
            for (CmdFuture &slot : slots) slot.done = true;
            assert(process_count > 0 && "Process count must be greater than 0");
        };

        CmdRunner(vector<Cmd> cmds) :
            process_count(sysconf(_SC_NPROCESSORS_ONLN)),
            slots(vector<CmdFuture>(process_count))
        {
            for (CmdFuture &slot : slots) slot.done = true;
            if (process_count == 0) process_count = 1; // Fallback
            push_many(cmds);
        }

        CmdRunner():
            process_count(sysconf(_SC_NPROCESSORS_ONLN)),
            slots(vector<CmdFuture>(process_count))
        {
            for (CmdFuture &slot : slots) slot.done = true;
            if (process_count == 0) process_count = 1; // Fallback
        }

        void push(const Cmd &cmd) {
            cmd_queue.push(cmd);
        }

        void push_many(const vector<Cmd> &cmds) {
            for (const auto &cmd : cmds) {
                this->cmd_queue.push(cmd);
            }
        }

        void run() {
            populate_slots();
            while (!cmd_queue.empty()) {
                if (populate_slots()) continue;
                usleep(10000);
            }
            await_slots();
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
