#ifndef BOB_H_
#define BOB_H_

#include <cstring>
#include <filesystem>
#include <iostream>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <functional>
#include <sys/ioctl.h>
#include <stdio.h>
#include <pty.h>
#include <thread>

namespace fs = std::filesystem;

//! Contains the core functionality of the Bob build system.
namespace bob {
    using std::string;
    using std::vector;
    using fs::path;

    struct Unit {};

    //! Macro to rebuild and rerun the current executable from its source code if needed.
    //! The new executable  will be run with the same arguments as the current one.
    //! This function is best used at the beginning of the `main` function.
    #define GO_REBUILD_YOURSELF(argc, argv) bob::_go_rebuild_yourself(argc, argv, __FILE__)

    //! Rebuilds the current executable from its source file. After building the new executable,
    //! it will run the new executable with the same arguments as the current one. This function
    //! is called with `GO_REBUILD_YOURSELF(argc, argv)` macro which provides the `source_file_name`
    //! argument automatically.
    void _go_rebuild_yourself(int argc, char* argv[], path source_file_name);

    //! Runs the current executable and returns the exit status.
    //! This is used in the `_go_rebuild_yourself(int argc, char * argv[], path source_file_name)` function.
    int run_yourself(fs::path bin, int argc, char* argv[]);

    //! Checks if the output file needs to be rebuilt based on the modification times of the input and output files.
    bool file_needs_rebuild(path input, path output);

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
        Result(const Ok<T>& ok);
        Result(const Err<E>& err);
        Result(Ok<T>&& ok);
        Result(Err<E>&& err);
        ~Result();

        bool is_ok() const;
        bool is_err() const;

        Ok<T> get_ok();
        Err<T> get_err();
        const T& unwrap() const;
        const E& unwrap_err() const;
    };

    #define PANIC(msg) bob::_panic(__FILE__, __LINE__, msg)
    #define WARNING(msg) bob::_warning(__FILE__, __LINE__, msg)

    [[noreturn]] void _panic(path file, int line, string msg);
    void _warning(path file, int line, string msg);

    //! Create a directory and its parents if it does not exist.
    path mkdirs(path dir);

    //! Returns a string that can be used as an include path for the given directory.
    //! E.g. `I("/usr/include")` will return `"-I/usr/include"`.
    string I(path p);

    //! Searches for a binary in the system PATH.
    path search_path(const string &bin_name);

    //! Prints a checklist of items with their statuses.
    void checklist(const vector<string> &items, const vector<bool> &statuses);

    //! Ensures that the given packages are installed in the system PATH.
    //! If any package is not found, it will print a checklist and exit the program.
    void ensure_installed(vector<string> packages);

    //! Finds the root directory of the project by looking for a marker file.
    Result<path, Unit> find_root(string marker_file);

    //! Finds the root directory of the git repository.
    Result<path, string> git_root();

    bool read_fd(int fd);

    struct CmdFuture {
        pid_t cpid;
        bool done;
        int exit_code;
        int stdout_fd;
        bool silent = false;
        CmdFuture();
        int await(string * output = nullptr);
        bool poll(string * output = nullptr);
        bool kill();
    };

    //! Represents a command to be executed in the operating system shell.
    class Cmd {
        vector<string> parts;
    public:
        //! If true, the command's output will be captured
        bool capture_output = false;

        //! If true, the command will not print its output to stdout.
        bool silent = false;
        string stdout_str;
        string error_output;
        path root;
        Cmd() = default;
        Cmd (vector<string> &&parts, path root = ".");
        Cmd& push(const string &part);
        Cmd& push_many(const vector<string> &parts);
        Cmd& push_many(const vector<path> &parts);

        //! Returns the command as a printable string.
        string render() const;

        //! Runs the command asynchronously and returns a `CmdFuture` object.
        //!
        //! This future can be awaited and polled until the command completes.
        CmdFuture run_async() const;

        //! Runs the command and returns the exit status.
        int run();

        //! Runs the command and checks that it exited with status 0.
        void check();

        //! Clears the command's parts to be reused for another command.
        void clear();

        //! Polls the command's future to check if it has completed
        //! and captures its output if available.
        bool poll_future(CmdFuture &fut);

        //! Awaits the command's future to complete and captures its output.
        int await_future(CmdFuture &fut);
    };

    struct CmdRunnerSlot {
        CmdFuture fut;
        int index;
        CmdRunnerSlot() : index{-1} {}
    };

    class CmdRunner {
        size_t slot_cursor = 0;
        size_t process_count;
        vector<CmdRunnerSlot> slots;
        bool populate_slots();
        void await_slots();
        void set_exit_code(CmdRunnerSlot &slot);
        bool any_waiting();
    public:
        vector<Cmd> cmds;
        vector<int> exit_codes;
        CmdRunner(size_t process_count);
        CmdRunner(vector<Cmd> cmds);
        CmdRunner();
        size_t size();
        void push(Cmd cmd);
        void push_many(vector<Cmd> cmds);
        void clear();
        bool run();
        bool any_failed();
        void capture_output(bool capture = true);
    };

    typedef std::vector<fs::path> Paths;
    typedef std::function<void(const vector<path>&, const vector<path>&)> RecipeFunc;

    class Recipe {
    public:
        Paths inputs;
        Paths outputs;
        RecipeFunc func;

        Recipe(const Paths &outputs, const Paths &inputs, RecipeFunc func);
        bool needs_rebuild() const;
        void build() const;
    };

    enum class CliFlagType {
        Bool,
        Value
    };

    struct CliArg {
        char    short_name  = '\0';
        string  long_name   = "";
        string  description = "";
        bool    set         = false; // Only used for boolean flags
        string  value       = "";    // Only used for options with values
        CliFlagType type;

        CliArg(const string &long_name, CliFlagType type, string description = "");
        CliArg(char short_name, CliFlagType type, string description = "");
        CliArg(char short_name, const string &long_name, CliFlagType type, string description = "");
        bool is_flag() const;
        bool is_option() const;
    };

    typedef std::vector<CliArg> CliArgs;

    void print_cli_args(const CliArgs &args);

    class CliCommand;
    typedef std::function<int(CliCommand&)> CliCommandFunc;

    class CliCommand {
    public:
        vector<string> path; // Path to the command, e.g. {"./bob", "test", "run"}
        vector<string> args; // Arguments that are not flags
        string name;         // TODO: Make this function
        CliCommandFunc func;
        string description;
        vector<CliArg> flags;
        vector<CliCommand> commands;

    private:
        [[noreturn]]
        void cli_panic(const string &msg);
        string parse_args(int argc, string argv[]);
        int call_func();

    public:
        CliCommand(const string &name, CliCommandFunc func, const string &description = "");
        CliCommand(const string &name, const string &description = "");
        bool is_menu() const;
        void usage() const;
        CliArg* find_short(char name);
        CliArg* find_long(string name);
        void handle_help();
        int run(int argc, string argv[]);
        void set_description(const string &desc);
        void set_default_command(CliCommandFunc f);
        CliCommand& add_command(CliCommand command);
        CliCommand& add_command(const string &name, string description, CliCommandFunc func);
        CliCommand& add_command(const string &name, string description);
        CliCommand& add_command(const string &name, CliCommandFunc func);
        CliCommand& add_command(const string &name);
        CliCommand& add_arg(const CliArg &arg);
        CliCommand& add_arg(char short_name, CliFlagType type, string description = "");
        CliCommand& add_arg(const string &long_name, CliFlagType type, string description = "");
        CliCommand& add_arg(char short_name, const string &long_name, CliFlagType type, string description = "");
        CliCommand& add_arg(const string &long_name, char short_name, CliFlagType type, string description = "");
        CliCommand alias(const string &name, const string &description = "");
    };

    class Cli : public CliCommand {
        vector<string> raw_args;
        void set_defaults(int argc, char* argv[]);
    public:
        Cli(int argc, char* argv[]);
        Cli(string title, int argc, char* argv[]);
        int serve();
    };

    //! Terminal related constants and functions.
    namespace term {

        // Reset Style
        const std::string RESET       = "\033[0m";

        // Regular Colors
        const std::string BLACK       = "\033[30m";
        const std::string RED         = "\033[31m";
        const std::string GREEN       = "\033[32m";
        const std::string YELLOW      = "\033[33m";
        const std::string BLUE        = "\033[34m";
        const std::string MAGENTA     = "\033[35m";
        const std::string CYAN        = "\033[36m";
        const std::string WHITE       = "\033[37m";

        // Bright Colors
        const std::string BRIGHT_BLACK   = "\033[90m";
        const std::string BRIGHT_RED     = "\033[91m";
        const std::string BRIGHT_GREEN   = "\033[92m";
        const std::string BRIGHT_YELLOW  = "\033[93m";
        const std::string BRIGHT_BLUE    = "\033[94m";
        const std::string BRIGHT_MAGENTA = "\033[95m";
        const std::string BRIGHT_CYAN    = "\033[96m";
        const std::string BRIGHT_WHITE   = "\033[97m";

        // Background Colors
        const std::string BG_BLACK       = "\033[40m";
        const std::string BG_RED         = "\033[41m";
        const std::string BG_GREEN       = "\033[42m";
        const std::string BG_YELLOW      = "\033[43m";
        const std::string BG_BLUE        = "\033[44m";
        const std::string BG_MAGENTA     = "\033[45m";
        const std::string BG_CYAN        = "\033[46m";
        const std::string BG_WHITE       = "\033[47m";

        // Bright Backgrounds
        const std::string BG_BRIGHT_BLACK   = "\033[100m";
        const std::string BG_BRIGHT_RED     = "\033[101m";
        const std::string BG_BRIGHT_GREEN   = "\033[102m";
        const std::string BG_BRIGHT_YELLOW  = "\033[103m";
        const std::string BG_BRIGHT_BLUE    = "\033[104m";
        const std::string BG_BRIGHT_MAGENTA = "\033[105m";
        const std::string BG_BRIGHT_CYAN    = "\033[106m";
        const std::string BG_BRIGHT_WHITE   = "\033[107m";

        // Text styles
        const std::string BOLD        = "\033[1m";
        const std::string DIM         = "\033[2m";
        const std::string UNDERLINE   = "\033[4m";
        const std::string BLINK       = "\033[5m";
        const std::string INVERT      = "\033[7m";
        const std::string HIDDEN      = "\033[8m";

        struct TermSize { size_t w, h; };
        TermSize size();
    }
}

#endif // BOB_H_

#ifdef BOB_IMPLEMENTATION

namespace bob {

    typedef std::vector<fs::path> Paths;

    int run_yourself(fs::path bin, int argc, char* argv[]) {
        fs::path bin_path = fs::relative(bin);
        auto run_cmd = Cmd({"./" + bin_path.string()});
        for (int i = 1; i < argc; ++i) run_cmd.push(argv[i]);
        std::cout << std::endl;
        return run_cmd.run();
    }

    bool file_needs_rebuild(path input, path output) {

        assert(!output.empty());
        assert(!input.empty());

        if (!fs::exists(output)) return true;

        struct stat stats;
        if (stat(output.c_str(), &stats) != 0) {
            PANIC("Could not stat output file '" + output.string() + "': " + strerror(errno));
        }
        time_t output_mtime = stats.st_mtime;

        if (stat(input.c_str(), &stats) != 0) {
            PANIC("Could not stat input file '" + input.string() + "': " + strerror(errno));
        }
        time_t input_mtime = stats.st_mtime;

        return output_mtime < input_mtime;
    }

    void _go_rebuild_yourself(int argc, char* argv[], path source_file_name) {
        assert(argc > 0 && "No program provided via argv[0]");

        path root = fs::current_path();

        path binary_path = fs::relative(argv[0], root);
        path source_path = fs::relative(source_file_name, root);
        path header_path = __FILE__;

        if (source_path.has_parent_path()) {
            WARNING("Source file is not next to executable. This may cause issues.");
        }

        if (source_path.empty() || header_path.empty() || binary_path.empty()) {
            PANIC("Failed to determine paths for source, header, or binary.\n\n"
                  "The bob executable must be compiled and run from the same directory as the source file.");
        }

        bool rebuild_needed = false;

        auto rebuild_yourself = Recipe(
                {binary_path},
                {source_path, header_path},
                [binary_path, source_path](Paths, Paths) {
                    Cmd({"g++", "-o", binary_path.string(), source_path.string()}).check();
                }
        );

        if (rebuild_yourself.needs_rebuild()) {
            rebuild_yourself.build();
            exit(run_yourself(binary_path, argc, argv));
        }

    }

    Result<path, Unit> find_root(string marker_file) {
        fs::path git_root = fs::current_path();
        while (git_root != git_root.root_path()) {
            if (fs::exists(git_root / marker_file)) {
                return Ok(git_root);
            }
            git_root = git_root.parent_path();
        }
        return Err(Unit());
    }

    Result<path, string> git_root() {
        auto res = find_root(".git");
        if (res.is_ok()) return res.get_ok();
        else return Err<string>("You are not in a git repository.");
    }

    // Create the directory if it does not exist. Returns the absolute path of the directory.
    path mkdirs(path dir) {
        if (!fs::exists(dir)) {
            std::cout << "Creating directory: " + dir.string() << std::endl;
            if (!fs::create_directories(dir)) {
                std::cerr << "Failed to create directory: " << dir << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        return fs::absolute(dir);
    }

    void _warning(path file, int line, string msg) {
        std::cerr << term::YELLOW << "[WARNING] " << file.string() << ":" << line << ": " << msg << term::RESET << std::endl;
    }

    [[noreturn]]
    void _panic(path file, int line, string msg) {
        std::cerr << term::RED << "[ERROR] " << file.string() << ":" << line << ": " << msg << term::RESET << std::endl;
        exit(1);
    }

    string I(path p) { return "-I" + p.string(); }

    // Check if a binary is in the system PATH
    path search_path(const string &bin_name) {
        string path_env = getenv("PATH");
        if (path_env.empty()) PANIC("PATH environment variable is not set.");

        // Iterate through each directory in PATH seperated by ':'
        size_t start = 0;
        size_t end   = path_env.find(':');

        while (end != string::npos) {
            string dir = path_env.substr(start, end - start);
            path bin = fs::path(dir) / bin_name;
            if (fs::exists(bin)) {
                return bin;
            }
            start = end + 1;
            end = path_env.find(':', start);
        }

        return "";
    }

    void checklist(const vector<string> &items, const vector<bool> &statuses) {
        if (items.size() != statuses.size()) {
            PANIC("Checklist items and statuses must have the same length.");
        }

        size_t max_length = 0;
        for (const auto &item : items) {
            max_length = std::max(max_length, item.length());
        }

        std::cout << std::endl;
        for (size_t i = 0; i < items.size(); ++i) {
            if (statuses[i]) std::cout << term::GREEN;
            else             std::cout << term::RED;

            std::cout << "    [" << (statuses[i] ? "✓" : "✗") << "] " << items[i];
            for (size_t j = items[i].length(); j < max_length; ++j) {
                std::cout << " ";
            }
            std::cout << term::RESET << std::endl;
        }
        std::cout << std::endl;
    }

    void ensure_installed(vector<string> packages) {
        bool all_installed = true;
        vector<path> installed_path(packages.size(), "");
        vector<bool> installed(packages.size(), false);

        for (int i = 0; i < packages.size(); ++i) {
            const string &pkg = packages[i];
            installed_path[i] = search_path(pkg);
            installed[i] = !installed_path[i].empty();
            all_installed &= installed[i];
        }

        if (all_installed) return;

        checklist(packages, installed);

        exit(EXIT_FAILURE);
    }

    template<typename T, typename E>
    Result<T, E>::Result(const Ok<T>& ok)   : status(true)  { new (&value.success) T(ok.value); }

    template<typename T, typename E>
    Result<T, E>::Result(const Err<E>& err) : status(false) { new (&value.error)   E(err.error); }

    template<typename T, typename E>
    Result<T, E>::Result(Ok<T>&& ok)        : status(true)  { new (&value.success) T(std::move(ok.value)); }

    template<typename T, typename E>
    Result<T, E>::Result(Err<E>&& err)      : status(false) { new (&value.error)   E(std ::move(err.error)); }

    template<typename T, typename E>
    Result<T, E>::~Result() {
        if (status) value.success.~T();
        else value.error.~E();
    }

    template<typename T, typename E>
    bool Result<T, E>::is_ok() const { return status; }

    template<typename T, typename E>
    bool Result<T, E>::is_err() const { return !status; }

    template<typename T, typename E>
    Ok<T> Result<T, E>::get_ok() {
        if (!status) throw std::logic_error("Called `get_ok` on an error Result");
        return value.success;
    }

    template<typename T, typename E>
    Err<T> Result<T, E>::get_err() {
        if (status) throw std::logic_error("Called `get_err` on a success Result");
        return value.error;
    }

    template<typename T, typename E>
    const T& Result<T, E>::unwrap() const {
        if (!status) throw std::logic_error("Called `unwrap` on an error Result");
        return value.success;
    }

    template<typename T, typename E>
    const E& Result<T, E>::unwrap_err() const {
        if (status) throw std::logic_error("Called `unwrap_err` on a success Result");
        return value.error;
    }

    bool read_fd(int fd, string * target) {
        bool got_data = false;
        for (;;) {
            char buf[1024];
            int n = read(fd, buf, sizeof(buf) - 1);
            if (n <= 0) return got_data;
            got_data = true;
            target->append(buf, n);
        }
        return got_data;
    }


    CmdFuture::CmdFuture() : cpid(-1), done(false), exit_code(-1) {}

    int CmdFuture::await(string * output) {
        while (!done) {
            done = poll(output);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return exit_code;
    }

    bool CmdFuture::poll(string * output) {
        if (done) return true;

        string new_output = "";
        bool got_data = read_fd(stdout_fd, &new_output);
        if (got_data && !silent) {
            std::cout << new_output;
        }

        if (output && got_data) {
            *output += new_output;
        }

        int status;
        pid_t result = waitpid(cpid, &status, WNOHANG);

        if (result == -1) PANIC("Error while polling child process: " + string(strerror(errno)));

        if (result == 0) return false;

        done = true;
        if (WIFEXITED(status)) exit_code = WEXITSTATUS(status);
        else PANIC("Child process did not terminate normally.");
        return true;
    }

    bool CmdFuture::kill() {
        if (cpid < 0) return false;
        if (::kill(cpid, SIGKILL) < 0) {
            std::cerr << "Failed to kill child process: " << strerror(errno) << std::endl;
            return false;
        }
        // Reset the state
        cpid = -1;
        done = true;
        exit_code = -1;

        return true;
    }

    Cmd::Cmd(vector<string> &&parts, path root) : parts(std::move(parts)), root(root) {}

    Cmd& Cmd::push(const string &part) {
        parts.push_back(part);
        return *this;
    }

    Cmd& Cmd::push_many(const vector<string> &parts) {
        for (const auto &part : parts) {
            this->parts.push_back(part);
        }
        return *this;
    }

    Cmd& Cmd::push_many(const vector<path> &parts) {
        for (const auto &part : parts) {
            this->parts.push_back(part);
        }
        return *this;
    }


    string Cmd::render() const {
        string result;
        for (const auto &part : parts) {
            if (!result.empty()) result += " ";
            result += part;
        }
        if (root != ".") {
            result = "[from '" + root.string() + "'] " + result;
        }
        return result;
    }

    CmdFuture Cmd::run_async() const {
        if (parts.empty() || parts[0].empty()) {
            PANIC("No command to run.");
        }

        std::cout << "CMD: " << render() << std::endl;

        // Pseudo-terminal for line-buffered output
        int stdout_fd;
        pid_t cpid = forkpty(&stdout_fd, nullptr, nullptr, nullptr);
        if (cpid < 0) {
            PANIC("Could not forkpty: " + std::string(strerror(errno)));
        }

        if (cpid == 0) {
            // --- Child process ---

            std::vector<char *> args;
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

            // Note: With forkpty, stdout/stderr are already connected to the PTY.
            // No need for manual dup2 redirection.

            if (execvp(args[0], args.data()) < 0) {
                std::cerr << "Could not exec child process: " << strerror(errno) << std::endl;
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
        }

        // --- Parent process ---

        // Set master PTY file descriptor to non-blocking mode
        fcntl(stdout_fd, F_SETFL, O_NONBLOCK);

        CmdFuture future;
        future.cpid = cpid;
        future.stdout_fd = stdout_fd;
        future.done = false;
        future.silent = silent;

        return future;
    }

    int Cmd::run() {
        CmdFuture fut = run_async();
        return await_future(fut);
    }

    void Cmd::check() {
        int exit_code = run();
        if (exit_code != 0) PANIC("Command '" + render() + "' failed with exit status: " + std::to_string(exit_code));
    }

    void Cmd::clear() {
        parts.clear();
    }

    bool Cmd::poll_future(CmdFuture &fut) {
        bool done = fut.poll(&stdout_str);
        return done;
    }

    int Cmd::await_future(CmdFuture &fut) {
        bool done = false;
        while (!done) {
            done = poll_future(fut);
        }
        return fut.exit_code;
    }

    bool CmdRunner::populate_slots() {
        bool did_work = false;
        for (size_t i = 0; i < process_count; ++i) {
            auto &slot = slots[i];

            if (slot.index >= 0) {
                bool fut_done = cmds[slot.index].poll_future(slot.fut);
                if (!fut_done) continue;
            }

            // Slot is ready!

            set_exit_code(slot);

            if (!any_waiting()) continue;

            // Populate slot with a new command
            size_t index = slot_cursor++;
            Cmd cmd = cmds[index];

            slot.fut = cmd.run_async();
            slot.index = index;

            did_work = true;
        }
        return did_work;
    }

    void CmdRunner::await_slots() {
        bool all_done = false;
        while (!all_done) {
            all_done = true;
            for (auto &slot : slots) {
                if (slot.index < 0) continue;
                bool done = cmds[slot.index].poll_future(slot.fut);
                if (done) {
                    set_exit_code(slot);
                }
                all_done &= done;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    void CmdRunner::set_exit_code(CmdRunnerSlot &slot) {
        if (slot.index < 0) return;
        exit_codes[slot.index] = slot.fut.exit_code;
    }

    bool CmdRunner::any_waiting() {
        return slot_cursor < cmds.size();
    }

    CmdRunner::CmdRunner(size_t process_count):
        process_count(process_count),
        slots(vector<CmdRunnerSlot>(process_count))
    {
        for (auto &slot : slots) {
            slot.fut.done = true;
            slot.fut.stdout_fd = -1;
        }
        assert(process_count > 0 && "Process count must be greater than 0");
    };

    CmdRunner::CmdRunner(vector<Cmd> cmds) :
        process_count(sysconf(_SC_NPROCESSORS_ONLN)),
        slots(vector<CmdRunnerSlot>(process_count))
    {
        for (auto &slot : slots) {
            slot.fut.done = true;
            slot.fut.stdout_fd = -1;
        }
        if (process_count == 0) process_count = 1; // Fallback
        push_many(cmds);
    }

    CmdRunner::CmdRunner():
        process_count(sysconf(_SC_NPROCESSORS_ONLN)),
        slots(vector<CmdRunnerSlot>(process_count))
    {
        for (auto &slot : slots) slot.fut.done = true;
        if (process_count == 0) process_count = 1; // Fallback
    }

    size_t CmdRunner::size() {
        return cmds.size();
    }

    void CmdRunner::push(Cmd cmd) {
        cmd.silent = true;
        cmds.push_back(cmd);
    }

    void CmdRunner::push_many(vector<Cmd> cmds) {
        for (const auto &cmd : cmds) {
            push(cmd);
        }
    }

    void CmdRunner::clear() {
        cmds.clear();
        exit_codes.clear();
    }

    bool CmdRunner::run() {
        exit_codes.resize(cmds.size(), -1);
        slot_cursor = 0;
        populate_slots();
        while (any_waiting()) {
            if (populate_slots()) continue;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        await_slots();
        return !any_failed();
    }

    bool CmdRunner::any_failed() {
        for (auto &exit_code : exit_codes) {
            if (exit_code != 0) return true;
        }
        return false;
    }

    void CmdRunner::capture_output(bool capture) {
        for (auto &cmd : cmds) {
            cmd.capture_output = capture;
        }
    }

    Recipe::Recipe(const Paths &outputs, const Paths &inputs, RecipeFunc func)
            : inputs(inputs), outputs(outputs), func(func) {}

    bool Recipe::needs_rebuild() const {
        for (const auto &input : inputs) {
            for (const auto &output : outputs) {
                if (file_needs_rebuild(input, output)) {
                    return true;
                }
            }
        }
        return false;
    }

    void Recipe::build() const {
        {
            vector<bool> exists(inputs.size(), true);
            bool any_missing = false;
            for (int i = 0; i < inputs.size(); ++i) {
                if (!fs::exists(inputs[i])) {
                    exists[i] = false;
                    any_missing = true;
                }
            }
            if (any_missing) {
                std::cerr << "[ERROR] Recipe inputs are missing:" << std::endl;
                vector<string> input_strings;
                for (auto &input : inputs) input_strings.push_back(input.string());
                checklist(input_strings, exists);
                exit(EXIT_FAILURE);
            }
        }

        if (!needs_rebuild()) return;

        func(inputs, outputs);

        {
            vector<bool> exists(outputs.size(), true);
            bool any_missing = false;
            for (int i = 0; i < outputs.size(); ++i) {
                if (!fs::exists(outputs[i])) {
                    exists[i] = false;
                    any_missing = true;
                }
            }
            if (any_missing) {
                std::cerr << "[ERROR] Recipe did not produce expected outputs:" << std::endl;
                vector<string> output_strings;
                for (auto &input : outputs) output_strings.push_back(input.string());
                checklist(output_strings, exists);
                exit(EXIT_FAILURE);
            }
        }
    }

    void print_cli_args(const CliArgs &args) {
        auto arg_len = [](const CliArg &arg) {
            size_t len = 0;
            if (arg.short_name != '\0') len += 2;                           // "-x"
            if (!arg.long_name.empty()) len += 2 + arg.long_name.length();  // "--long"
            if (arg.short_name != '\0' && !arg.long_name.empty()) len += 2; // ", "
            return len;
        };

        size_t max_arg_length = 0;
        for (const auto &arg : args) {
            max_arg_length = std::max(max_arg_length, arg_len(arg));
        }

        auto arg_placeholder = [](const CliArg &arg) -> string {
            if (arg.type == CliFlagType::Bool) return "";
            return arg.long_name.empty() ? "<value>" : "<" + arg.long_name + ">";;
        };

        size_t max_placeholder_length = 0;
        for (const auto &arg : args) {
            max_placeholder_length = std::max(max_placeholder_length, arg_placeholder(arg).length());
        }

        for (const auto &arg : args) {
            // Initial indentation
            std::cout << "    ";

            // Print short argument name
            if (arg.short_name != '\0') {
                std::cout << "-" << arg.short_name;
                if (!arg.long_name.empty()) std::cout << ", ";
            }

            // Print long argument name
            if (!arg.long_name.empty()) {
                std::cout << "--" << arg.long_name;
            }

            // Padding for alignment
            for (size_t i = 0; i < max_arg_length - arg_len(arg); ++i) {
                std::cout << " ";
            }

            // Print argument value if it's an option
            string placeholder = arg_placeholder(arg);
            std::cout << " " << placeholder;
            for (size_t i = 0; i < max_placeholder_length - placeholder.length(); ++i) {
                std::cout << " ";
            }

            std::cout << "  " << arg.description << std::endl;
        }
    }

    CliArg::CliArg(const string &long_name, CliFlagType type, string description):
            long_name(long_name), type(type), description(description) {};

    CliArg::CliArg(char short_name, CliFlagType type, string description):
            short_name(short_name), type(type), description(description) {};

    CliArg::CliArg(char short_name, const string &long_name, CliFlagType type, string description):
            short_name(short_name), long_name(long_name), type(type), description(description) {};

    bool CliArg::is_flag() const {
        return type == CliFlagType::Bool;
    }

    bool CliArg::is_option() const {
        return type == CliFlagType::Value;
    }

    [[noreturn]]
    void CliCommand::cli_panic(const string &msg) {
        std::cerr << "[ERROR] " << msg << "\n" << std::endl;
        usage();
        exit(EXIT_FAILURE);
    }

    string CliCommand::parse_args(int argc, string argv[]) {
        assert(argc >= 0 && "Argc must be non-negative");

        for (; argc > 0; --argc, ++argv) {

            string arg_name = *argv;

            if (arg_name.empty()) {
                cli_panic("Empty argument found in command line arguments.");
            }

            if (arg_name[0] != '-') {
                if (is_menu()) return arg_name; // This is the next command name
                else {
                    args.push_back(arg_name); // This is a value argument
                    continue;
                }
            }

            bool found = false;

            for (CliArg &arg : flags) {

                bool short_name_matches = arg.short_name != '\0'
                    && arg_name.length() == 2
                    && arg_name[0] == '-'
                    && arg_name[1] == arg.short_name;

                bool long_name_matches = !arg.long_name.empty()
                    && arg_name.length() > 2
                    && arg_name.substr(0, 2) == "--"
                    && arg_name.substr(2) == arg.long_name;

                // Short argument
                if (short_name_matches || long_name_matches) {
                    found = true;
                    switch (arg.type) {
                        case CliFlagType::Bool:
                            arg.set = true;
                            break;
                        case CliFlagType::Value:
                            if (argc <= 1) cli_panic("Expected value for argument: " + arg_name);
                            argc--;
                            argv++;
                            arg.value = *argv;
                            arg.set = true;
                            break;
                    }
                }
            }

            if (!found) cli_panic("Unknown argument: " + arg_name);
        }

        return ""; // No next command
    }

    int CliCommand::call_func() {
        if (!func) cli_panic("No function set for command: " + name);
        return func(*this);
    }

    CliCommand::CliCommand(const string &name, CliCommandFunc func, const string &description)
       : name(name), func(func), description(description) {}

    CliCommand::CliCommand(const string &name, const string &description)
        : CliCommand(name, nullptr, description) {
            func = [] (CliCommand &cmd) {
                std::cout << "No command provided.\n" << std::endl;
                cmd.usage();
                return EXIT_FAILURE;
            };
        }

    bool CliCommand::is_menu() const {
        return !commands.empty();
    }

    void CliCommand::usage() const {
        if (!description.empty()) {
            std::cout << description << std::endl;
        }

        size_t max_name_length = 0;
        for (const auto &cmd : commands) {
            max_name_length = std::max(max_name_length, cmd.name.length());
        }

        if (!commands.empty()) {
            std::cout << "\nAvailable commands:" << std::endl;
            for (const auto &cmd : commands) {
                std::cout << "    " << cmd.name << "     ";
                size_t padding = max_name_length - cmd.name.length();
                for (size_t i = 0; i < padding; ++i) std::cout << " ";
                std::cout << cmd.description << std::endl;
            }
        }

        if (!flags.empty()) {
            std::cout << "\nArguments:" << std::endl;
            print_cli_args(flags);
        }
    }

    CliArg* CliCommand::find_short(char name) {
        for (CliArg &arg : flags) {
            if (arg.short_name == name) return &arg;
        }
        return nullptr; // Not found
    }

    CliArg* CliCommand::find_long(string name) {
        for (CliArg &arg : flags) {
            if (arg.long_name == name) return &arg;
        }
        return nullptr; // Not found
    }

    void CliCommand::handle_help() {
        CliArg *help_arg = find_long("help");
        if (!help_arg) help_arg = find_short('h');
        if (!help_arg) return; // No help argument found

        if (!help_arg->set) return;

        usage();
        exit(EXIT_SUCCESS);
    }

    int CliCommand::run(int argc, string argv[]) {
        string subcommand_name = parse_args(argc, argv);

        if (subcommand_name.empty()) return call_func();

        if (!is_menu()) return call_func();

        vector<string> subcommand_path = path;
        subcommand_path.push_back(subcommand_name);

        // Find the subcommand
        for (auto &cmd : commands) {
            if (cmd.name == subcommand_name) {

                // Pass parent args to subcommand in reverse
                // order to keep --help as the last argument
                for (int i = flags.size() - 1; i >= 0; --i) {
                    CliArg &arg = flags[i];

                    // Skip if the argument is already set
                    if (cmd.find_short(arg.short_name)) continue;
                    if (cmd.find_long(arg.long_name))   continue;

                    cmd.add_arg(arg);
                }

                cmd.path = subcommand_path;

                // Skip this command name
                return cmd.run(argc-1, argv+1);
            }
        }

        cli_panic("Unknown command: " + subcommand_name);
    }

    void CliCommand::set_description(const string &desc) {
        description = desc;
    }

    void CliCommand::set_default_command(CliCommandFunc f) {
        func = f;
    }

    CliCommand CliCommand::alias(const string &name, const string &description) {
        CliCommand alias_cmd = *this;
        alias_cmd.name = name;
        if (description.empty()) {
            alias_cmd.description = "Alias for command: " + this->name;
        }
        else {
            alias_cmd.description = description;
        }
        return alias_cmd;
    }

    CliCommand& CliCommand::add_command(CliCommand command) {
        // TODO: This may reallocate, thus invalidating previously returned references
        commands.push_back(command);
        return commands[commands.size() - 1];
    }

    CliCommand& CliCommand::add_command(const string &name, string description, CliCommandFunc func) {
        return add_command(CliCommand(name, func, description));
    }

    CliCommand& CliCommand::add_command(const string &name, string description) {
        return add_command(CliCommand(name, description));
    }

    CliCommand& CliCommand::add_command(const string &name, CliCommandFunc func) {
        return add_command(CliCommand(name, func));
    }

    CliCommand& CliCommand::add_command(const string &name) {
        return add_command(CliCommand(name));
    }

    CliCommand& CliCommand::add_arg(const CliArg &arg) {
        CliArg * short_existing = find_short(arg.short_name);
        CliArg * long_existing  = find_long(arg.long_name);

        if (short_existing) PANIC("Short argument already exists: " + string({arg.short_name}));
        if (long_existing)  PANIC("Long argument already exists: " + arg.long_name);

        flags.push_back(arg);

        return *this;
    }

    CliCommand& CliCommand::add_arg(char short_name, CliFlagType type, string description) {
        return add_arg(CliArg(short_name, type, description));
    }

    CliCommand& CliCommand::add_arg(const string &long_name, CliFlagType type, string description) {
        return add_arg(CliArg(long_name, type, description));
    }

    CliCommand& CliCommand::add_arg(char short_name, const string &long_name, CliFlagType type, string description) {
        return add_arg(CliArg(short_name, long_name, type, description));
    }

    CliCommand& CliCommand::add_arg(const string &long_name, char short_name, CliFlagType type, string description) {
        return add_arg(CliArg(short_name, long_name, type, description));
    }

    void Cli::set_defaults(int argc, char* argv[]) {
        assert(argc > 0 && "No program provided via argv[0]");

        name = argv[0];
        path.push_back(name);

        raw_args.assign(argv + 1, argv + argc);

        // Add help argument. This will be inherited by all sub commands.
        add_arg("help", 'h', CliFlagType::Bool, "Prints this help message");
    }

    Cli::Cli(int argc, char* argv[]): CliCommand("") {
        set_defaults(argc, argv);
    }

    Cli::Cli(string title, int argc, char* argv[]) : CliCommand("", title) {
        set_defaults(argc, argv);
    }

    int Cli::serve() {
        return run(raw_args.size(), raw_args.data());
    }

    namespace term {
        TermSize size() {
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            return {w.ws_col, w.ws_row};
        }
    }
}

#endif // BOB_IMPLEMENTATION
