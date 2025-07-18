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
#include <functional>

namespace fs = std::filesystem;

namespace bob {

    using std::string;
    using std::vector;
    using fs::path;

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
        path root;

        Cmd() = default;

        Cmd (vector<string> &&parts, path root = ".") : parts(std::move(parts)), root(root) {}

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

        CmdFuture run_async() const {
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

        int run() const {
            return run_async().await();
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

        auto compile_cmd = Cmd({"g++", src_str, "-o", bin_str});

        int status = compile_cmd.run();

        if (status != 0) {
            std::cerr << "Rebuild failed with status: " << status << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    inline int run_yourself(fs::path bin, int argc, char* argv[]) {
        fs::path bin_path = fs::relative(bin);
        auto run_cmd = Cmd({"./" + bin_path.string()});
        for (int i = 1; i < argc; ++i) run_cmd.push(argv[i]);
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
            exit(run_yourself(executable_path, argc, argv));
        }

    }

    enum class CliArgType {
        Flag,
        Option
    };

    struct CliArg {
        char    short_name  = '\0';
        string  long_name   = "";
        string  description = "";
        bool    set         = false; // Only used for boolean flags
        string  value       = "";    // Only used for options with values
        CliArgType type;

        CliArg(const string &long_name, CliArgType type, string description = ""):
            long_name(long_name), type(type), description(description) {};

        CliArg(char short_name, CliArgType type, string description = ""):
            short_name(short_name), type(type), description(description) {};

        CliArg(char short_name, const string &long_name, CliArgType type, string description = ""):
            short_name(short_name), long_name(long_name), type(type), description(description) {};

        bool is_flag() const {
            return type == CliArgType::Flag;
        }

        bool is_option() const {
            return type == CliArgType::Option;
        }
    };

    typedef std::vector<CliArg> CliArgs;

    inline void print_cli_args(const CliArgs &args) {
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
            if (arg.type == CliArgType::Flag) return "";
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

    class CliCommand;
    typedef std::function<int(CliCommand&)> CliCommandFunc;

    class CliCommand {

        [[noreturn]]
        void cli_panic(const string &msg) {
            std::cerr << "[ERROR] " << msg << "\n" << std::endl;
            usage();
            exit(EXIT_FAILURE);
        }

        string parse_args(int argc, string argv[]) {
            assert(argc >= 0 && "Argc must be non-negative");

            for (; argc > 0; --argc, ++argv) {

                string arg_name = *argv;

                if (arg_name.empty()) {
                    cli_panic("Empty argument found in command line arguments.");
                }

                if (arg_name[0] != '-') {
                    return arg_name; // This is the next command name
                }

                bool found = false;

                for (CliArg &arg : args) {

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
                            case CliArgType::Flag:
                                arg.set = true;
                                break;
                            case CliArgType::Option:
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

        int call_func() {
            if (!func) cli_panic("No function set for command: " + name);
            return func(*this);
        }

    public:
        string name;
        CliCommandFunc func;
        string description;
        vector<CliArg> args;
        vector<CliCommand> commands;

        CliCommand(const string &name, CliCommandFunc func, const string &description = "")
           : name(name), func(func), description(description) {}

        CliCommand(const string &name, const string &description = "")
            : CliCommand(name, nullptr, description) {
                func = [] (CliCommand &cmd) {
                    std::cout << "No command provided.\n" << std::endl;
                    cmd.usage();
                    return EXIT_FAILURE;
                };
            }

        void usage() const {
            if (!description.empty()) {
                std::cout << description << "\n" << std::endl;
            }

            size_t max_name_length = 0;
            for (const auto &cmd : commands) {
                max_name_length = std::max(max_name_length, cmd.name.length());
            }

            std::cout << "Available commands:" << std::endl;
            for (const auto &cmd : commands) {
                std::cout << "    " << cmd.name << "     ";
                size_t padding = max_name_length - cmd.name.length();
                for (size_t i = 0; i < padding; ++i) std::cout << " ";
                std::cout << cmd.description << std::endl;
            }

            if (!args.empty()) {
                std::cout << "\nArguments:" << std::endl;
                print_cli_args(args);
            }
        }

        CliArg* find_short(char name) {
            for (CliArg &arg : args) {
                if (arg.short_name == name) return &arg;
            }
            return nullptr; // Not found
        }

        CliArg* find_long(string name) {
            for (CliArg &arg : args) {
                if (arg.long_name == name) return &arg;
            }
            return nullptr; // Not found
        }

        void handle_help() {
            CliArg *help_arg = find_long("help");
            if (!help_arg) help_arg = find_short('h');
            if (!help_arg) return; // No help argument found

            if (!help_arg->set) return;

            usage();
            exit(EXIT_SUCCESS);
        }

        int run(int argc, string argv[]) {

            string subcommand_name = parse_args(argc, argv);

            if (subcommand_name.empty()) return call_func();

            // Find the subcommand
            for (auto &cmd : commands) {
                if (cmd.name == subcommand_name) {

                    // Pass parent args to subcommand in reverse
                    // order to keep --help as the last argument
                    for (int i = args.size() - 1; i >= 0; --i) {
                        CliArg &arg = args[i];

                        // Skip if the argument is already set
                        if (cmd.find_short(arg.short_name)) continue;
                        if (cmd.find_long(arg.long_name))   continue;

                        cmd.add_arg(arg);
                    }

                    // Skip this command name
                    return cmd.run(argc-1, argv+1);
                }
            }

            cli_panic("Unknown command: " + subcommand_name);
        }

        void set_description(const string &desc) {
            description = desc;
        }

        void set_default_command(CliCommandFunc f) {
            func = f;
        }

        CliCommand& add_command(CliCommand command) {
            commands.push_back(command);
            return *this;
        }

        CliCommand& add_command(const string &name, string description, CliCommandFunc func) {
            return add_command(CliCommand(name, func, description));
        }

        CliCommand& add_command(const string &name, string description) {
            return add_command(CliCommand(name, description));
        }

        CliCommand& add_command(const string &name, CliCommandFunc func) {
            return add_command(CliCommand(name, func));
        }

        CliCommand& add_command(const string &name) {
            return add_command(CliCommand(name));
        }

        CliCommand& subcommand() {
            return commands[commands.size() - 1];
        }

        CliCommand& add_arg(const CliArg &arg) {
            CliArg * short_existing = find_short(arg.short_name);
            CliArg * long_existing  = find_long(arg.long_name);

            if (short_existing) panic("Short argument already exists: " + string({arg.short_name}));
            if (long_existing)  panic("Long argument already exists: " + arg.long_name);

            args.push_back(arg);

            return *this;
        }

        CliCommand& add_arg(char short_name, CliArgType type, string description = "") {
            return add_arg(CliArg(short_name, type, description));
        }

        CliCommand& add_arg(const string &long_name, CliArgType type, string description = "") {
            return add_arg(CliArg(long_name, type, description));
        }

        CliCommand& add_arg(char short_name, const string &long_name, CliArgType type, string description = "") {
            return add_arg(CliArg(short_name, long_name, type, description));
        }

        CliCommand& add_arg(const string &long_name, char short_name, CliArgType type, string description = "") {
            return add_arg(CliArg(short_name, long_name, type, description));
        }

    };


    class Cli : public CliCommand {
        vector<string> raw_args;

        void set_defaults(int argc, char* argv[]) {
            assert(argc > 0 && "No program provided via argv[0]");

            name = argv[0];

            raw_args.assign(argv + 1, argv + argc);

            // Add help argument. This will be inherited by all sub commands.
            add_arg("help", 'h', CliArgType::Flag, "Prints this help message");
        }

    public:

        Cli(int argc, char* argv[]): CliCommand("") {
            set_defaults(argc, argv);
        }

        Cli(string title, int argc, char* argv[]) : CliCommand("", title) {
            set_defaults(argc, argv);
        }

        int serve() {
            return run(raw_args.size(), raw_args.data());
        }
    };


}
