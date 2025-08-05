/* ============================================================================== *
 * MIT License                                                                    *
 *                                                                                *
 * Copyright (c) 2025 Balder W. Holst                                             *
 *                                                                                *
 * Permission is hereby granted, free of charge, to any person obtaining a copy   *
 * of this software and associated documentation files (the "Software"), to deal  *
 * in the Software without restriction, including without limitation the rights   *
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell      *
 * copies of the Software, and to permit persons to whom the Software is          *
 * furnished to do so, subject to the following conditions:                       *
 *                                                                                *
 * The above copyright notice and this permission notice shall be included in all *
 * copies or substantial portions of the Software.                                *
 *                                                                                *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,       *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE    *
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER         *
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  *
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE  *
 * SOFTWARE.                                                                      *
 * ============================================================================== */

#ifndef BOB_H_
#define BOB_H_

#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <thread>
#include <functional>
#include <cassert>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <pty.h>

namespace fs = std::filesystem;

//! Contains the functionality of the Bob build system.
namespace bob {
    using std::string;
    using std::vector;
    using fs::path;

    #ifndef BOB_REBUILD_CMD
        #define BOB_REBUILD_CMD { "g++", "-o", "_PROGRAM_", "_SOURCE_" }
    #endif

    //! Macro to rebuild and rerun the current executable from its source code if needed.
    //! The new executable  will be run with the same arguments as the current one.
    //! This function is best used at the beginning of the `main` function.
    #define GO_REBUILD_YOURSELF(argc, argv) bob::go_rebuild_yourself(argc, argv, __FILE__, bob::RebuildConfig(BOB_REBUILD_CMD))

    // Forward declarations
    class Cmd;
    class CliCommand;

    //! \brief Configuration for rebuilding the current executable.
    //!
    //! This class allows specifying how to rebuild the current executable
    //! `parts` is a vector of strings that represent the command line parts.
    //! The special strings `_PROGRAM_` and `_SOURCE_` can be used to
    //! represent the program name and source file name respectively.
    //! By default, these are set to "bob" and "bob.cpp".
    //!
    //! @note This class is used indirectly by the `GO_REBUILD_YOURSELF` macro.
    //! To customize the rebuild command in used by the macro, you can define
    //! `BOB_REBUILD_CMD` before including the `bob.hpp` header file.
    //!
    //! @par Example
    //! ```cpp
    //! // Specify the desired rebuild command.
    //! RebuildConfig config({ "g++", "-o", "_PROGRAM_", "_SOURCE_", "-Wall" });
    //!
    //! // Create a run-able command. Program and source files can be specified here if needed.
    //! Cmd cmd = config.cmd();
    //!
    //! // Runs: g++ -o my_program my_source.cpp -Wall
    //! cmd.run();
    //! ```
    struct RebuildConfig {
        //! Special string to represent the program name in the command parts.
        const std::string_view PROGRAM = "_PROGRAM_";

        //! Special string to represent the source file name in the command parts.
        const std::string_view SOURCE  = "_SOURCE_";

        //! A vector of strings representing the command parts before substitution.
        vector<string> parts;

        //! Create an empty rebuild configuration.
        RebuildConfig() = default;

        //! Create a rebuild configuration with the given parts.
        RebuildConfig(vector<string> &&parts) : parts(std::move(parts)) {}

        //! Create a `Cmd` object from this configuration. The `_PROGRAM_` and `_SOURCE_` placeholders
        //! will be replaced with the provided `program` and `source` arguments.
        Cmd cmd(string source = "bob.cpp", string program = "bob") const;
    };

    //! Rebuilds the current executable from its source file. After building the new executable,
    //! it will run the new executable with the same arguments as the current one. This function
    //! is called with `GO_REBUILD_YOURSELF(argc, argv)` macro which provides the `source_file_name`
    //! argument automatically.
    void go_rebuild_yourself(int argc, char* argv[], path source_file_name);

    //! Runs the current executable and returns the exit status.
    //! This is used in the `go_rebuild_yourself(int argc, char * argv[], path source_file_name)` function.
    int run_yourself(fs::path bin, int argc, char* argv[]);

    //! Checks if an output file is older than its input file and needs to be rebuilt.
    //!
    //! @param input  The source file path.
    //! @param output The target file path.
    //! @return `true` if the output is missing or older than the input; false otherwise.
    //!
    //! @par Example
    //! ```
    //! if (bob::file_needs_rebuild("main.cpp", "main.o")) {
    //!     // Compile `main.cpp` to `main.o`
    //! }
    //! ```
    bool file_needs_rebuild(path input, path output);

    #define PANIC(msg) bob::_panic(__FILE__, __LINE__, msg)
    #define WARNING(msg) bob::_warning(__FILE__, __LINE__, msg)

    //! \cond DO_NOT_DOCUMENT
    [[noreturn]] void _panic(path file, int line, string msg);
    void _warning(path file, int line, string msg);
    //! \endcond

    //! Creates a directory and all necessary parent directories.
    //!
    //! @param dir The directory path to create.
    //! @return The created directory path.
    //!
    //! @par Examples
    //! ```cpp
    //!     path build_dir = mkdirs("build/output");
    //! ```
    //! or
    //! ```
    //!     path build_dir = "build/output";
    //!     mkdirs(build_dir);
    //! ```
    path mkdirs(path dir);

    //! Converts a path to a string with the `-I` prefix for use in compiler commands.
    //!
    //! @param p The path to convert.
    //! @return The path as a UTF-8 encoded string.
    //!
    //! @par Example
    //! ```
    //! Cmd compile({"g++", I("src"), "-o", "output"});
    //! compile.run(); // Runs `g++ -Isrc -o output`
    //! ```
    string I(path p);

    //! Searches for a binary in the system $PATH variable.
    //!
    //! @param bin_name The name of the binary to search for.
    //! @return The path to the binary if found, otherwise an empty path.
    //!
    //! @par Example
    //! ```
    //! path git_bin = search_path("git");
    //! if (git_bin.empty()) {
    //!     std::cerr << "git not found!\n";
    //! }
    //! ```
    path search_path(const string &bin_name);

    //! Displays a checklist of items with their statuses.
    //!
    //! @param items    A list of checklist item names.
    //! @param statuses A list of boolean statuses; true means complete, false means incomplete.
    //!
    //! @par Example
    //! ```
    //! checklist({"Download", "Build", "Test"}, {true, false, false});
    //! ```
    void checklist(const vector<string> &items, const vector<bool> &statuses);

    //! Ensures that the specified packages are installed. If they are not, a checklist is printed
    //! of the missing and found packages, and the program exits with an error code.
    //!
    //! @param packages A list of package names to verify and install if missing.
    //!
    //! @par Example
    //! ```
    //! ensure_installed({"cmake", "ninja", "git"});
    //! ```
    void ensure_installed(vector<string> packages);

    //! Finds the root directory containing a specific marker file.
    //!
    //! @param root         Pointer to store the found root path.
    //! @param marker_file  The file name to look for in the directory hierarchy.
    //! @return true if the root was found; false otherwise.
    //!
    //! @par Example
    //! ```
    //! path project_root;
    //! if (find_root(&project_root, ".git")) {
    //!     std::cout << "Root: " << project_root << "\n";
    //! }
    //! ```
    bool find_root(path * root, string marker_file);

    //! Finds the root of a Git repository.
    //!
    //! @param root Pointer to store the Git repository root path.
    //! @return true if inside a Git repository; false otherwise.
    //!
    //! @par Example
    //! ```
    //! path git_root_path;
    //! if (git_root(&git_root_path)) {
    //!     std::cout << "Git root: " << git_root_path << "\n";
    //! }
    //! ```
    bool git_root(path * root);

    //! Represents a command that being executed in the background.
    struct CmdFuture {
        //! Process ID of the child process.
        pid_t cpid;
        //! Indicates whether the command has completed.
        bool done;
        //! Exit code of the command. Only valid after the command has completed.
        int exit_code;
        //! File descriptor for reading the command's output (stdout and stderr).
        int output_fd;
        //! If true, the command's output will not be printed to stdout while it is running.
        bool silent = false;

        CmdFuture();

        //! Blocks until the command completes and returns the exit code.
        int await(string * output = nullptr);

        //! Polls the command's output and checks if it has completed.
        //! Also captures and prints any output.
        bool poll(string * output = nullptr);

        //! Kills the command if it is still running.
        bool kill();
    };

    //! \brief Represents a command to be executed in the operating system shell.
    //!
    //! This class allows building command lines from parts (strings or paths),
    //! running them synchronously or asynchronously, and optionally capturing their output.
    //! It supports managing execution context such as the root directory and controlling
    //! output visibility (using the `silent` field).
    //!
    //! @par Example - Synchronous Command Execution
    //! ```cpp
    //! Cmd compile({"g++", I("src"), "-o", "output"});
    //! compile.capture_output = true;
    //! int status = compile.run();
    //! if (status != 0) {
    //!     std::cerr << "Compilation failed:\n" << compile.output_str;
    //! }
    //! ```
    //! @par Example - Asynchronous Command Execution
    //! ```cpp
    //! Cmd cmd({"sleep", "3"}); // Heavy work
    //! CmdFuture fut = cmd.run_async();
    //! while (!fut.done) {
    //!     cmd.poll_future(fut);
    //!     std::cout << "waiting for command to finish...\n";
    //!     usleep(100000); // sleep for 100ms
    //! }
    //! std::cout << "Command finished with exit code: " << fut.exit_code << "\n";
    //! ```
    class Cmd {
        vector<string> parts;
    public:
        //! If true, the command's output will be captured.
        bool capture_output = false;

        //! If true, the command will not print its output to stdout.
        bool silent = false;

        //! The command's output captured during execution (stdout and stderr).
        string output_str = "";

        //! The root directory from which the command is executed.
        path root = ".";

        //! Creates an empty command.
        Cmd() = default;

        //! Creates a command from a list of parts and optional root directory.
        //!
        //! @param parts The command and its arguments as string parts.
        //! @param root The root directory where the command runs (default is current directory).
        //!
        //! @par Example
        //! ```cpp
        //! Cmd compile({"g++", "-o", "app"}, "/home/user/project");
        //! ```
        Cmd(vector<string> &&parts, path root = ".");

        //! Adds a single part (argument or command) to the command.
        //!
        //! @param part The part to add.
        //! @return Reference to this command (for chaining).
        //!
        //! @par Example
        //! ```cpp
        //! Cmd cmd;
        //! cmd.push("g++").push("app.c").push("-o").push("app");
        //! ```
        Cmd& push(const string &part);

        //! Adds multiple string parts to the command.
        //!
        //! @param parts The parts to add.
        //! @return Reference to this command.
        //!
        //! @par Example
        //! ```cpp
        //! const vector<string> FLAGS = {"-Wall", "-O2"};
        //! const string COMPILER = "g++";
        //! Cmd cmd;
        //! cmd
        //!     .push_many({COMPILER, "app.c", "-o", "app"})
        //!     .push_many(FLAGS);
        //! ```
        Cmd& push_many(const vector<string> &parts);

        //! Creates a printable string representation of the command.
        //!
        //! @return The command rendered as a string.
        //!
        //! @par Example
        //! ```cpp
        //! Cmd cmd({"ls", "-la"});
        //! std::cout << cmd.render() << std::endl; // prints: "ls -la"
        //! ```
        string render() const;

        //! Runs the command asynchronously and returns a future object.
        //!
        //! @return A CmdFuture object representing the running command.
        //!
        //! @par Example
        //! ```cpp
        //! Cmd cmd({"sleep", "3"}); // Heavy work
        //! CmdFuture fut = cmd.run_async();
        //! // Do other work, then await or poll fut...
        //! ```
        CmdFuture run_async() const;

        //! Runs the command synchronously and returns its exit status.
        //!
        //! @return The command's exit code.
        //!
        //! @par Example
        //! ```cpp
        //! Cmd cmd({"seq", "1000"});
        //! int status = cmd.run();
        //! if (status != 0) { /* handle error */ }
        //! ```
        int run();

        //! Runs the command and throws if the exit status is not zero.
        //!
        //! @throws std::runtime_error if the command exits with a non-zero status.
        //!
        //! @par Example
        //! ```cpp
        //! Cmd cmd({"echo", "Hello, World!"});
        //! cmd.check(); // throws if command failed
        //! ```
        void check();

        //! Clears all parts of the command so it can be reused.
        //!
        //! @par Example
        //! ```cpp
        //!
        //! Cmd cmd({"echo", "Hello"});
        //! cmd.run();
        //!
        //! cmd.clear();
        //! cmd.push("echo").push(" world!");
        //! cmd.run();
        //! ```
        void clear();

        //! Polls an ongoing asynchronous command to check if it has finished.
        //! Captures and prints output based on the `capture_output` and `silent` fields.
        //!
        //! @param fut The CmdFuture object representing the running command.
        //! @return True if the command has completed; false otherwise.
        //!
        //! @par Example
        //! ```cpp
        //! Cmd cmd({"sleep", "3"}); // Heavy work
        //! CmdFuture fut = cmd.run_async();
        //! while (!cmd.poll_future(fut)) {
        //!     std::cout << "Waiting..." << std::endl;
        //! }
        //! std::cout << "Done!" << std::endl;;
        //! ```
        bool poll_future(CmdFuture &fut);

        //! Waits for an asynchronous command to finish and captures its output.
        //!
        //! @param fut The CmdFuture object representing the running command.
        //! @return The command's exit status.
        //!
        //! @par Example
        //! ```cpp
        //! Cmd cmd({"sleep", "3"});
        //! CmdFuture fut = cmd.run_async();
        //! int exit_code = cmd.await_future(fut);
        //! ```
        int await_future(CmdFuture &fut);
    };
    //! \example minimal/bob.cpp

    //! A class for running many commands in parallel.
    class CmdRunner {

        //! Internal structure to hold a command and its future.
        struct CmdRunnerSlot {
            CmdFuture fut;
            int index;
            CmdRunnerSlot() : index{-1} {}
        };

        //!  The index of the next command to run.
        size_t cursor = 0;
        //! The number of processes to run concurrently.
        size_t process_count;
        //! A vector of slots, each holding future of a running command.
        vector<CmdRunnerSlot> slots;
        //! Initializes the slots with empty complete futures.
        void init_slots();
        //! Populate the slots with running commands.
        bool populate_slots();
        //! Wait for all slots to complete.
        void await_slots();
        //! Set the exit code for a slot based on its future.
        void set_exit_code(CmdRunnerSlot &slot);
        //! Check if there are any commands waiting to be run.
        bool any_waiting();

    public:
        //! The commands to be run by this runner.
        vector<Cmd> cmds;
        //! Exit codes for each command in `cmds`.
        vector<int> exit_codes;
        //! Create a `CmdRunner` with a specified number of processes.
        CmdRunner(size_t process_count);
        //! Create a `CmdRunner` with a list of commands.
        //!
        //! The process count is the thread count of the current processor
        CmdRunner(vector<Cmd> cmds);
        //! Create a `CmdRunner` with a list of commands and a maximum number
        //! of simultaneous processes.
        CmdRunner(vector<Cmd> cmds, size_t process_count);
        //! Create an empty `CmdRunner`.
        //!
        //! The process count is the thread count of the current processor
        CmdRunner();
        //! Returns the number of commands in the runner.
        size_t size();
        //! Push a single command to the runner.
        void push(Cmd cmd);
        //! Push multiple commands to the runner.
        void push_many(vector<Cmd> cmds);
        //! Clears all commands in the runner for reuse.
        void clear();
        //! Runs all commands in the runner concurrently.
        bool run();
        //! Returns `true` if all commands in the runner succeeded (exit code 0).
        bool all_succeded();
        //! Returns `true` if any command in the runner failed (non-zero exit code).
        bool any_failed();
        //! Prints the output of all commands that failed.
        void print_failed();
        //! Sets the `capture_output` flag for all commands in the runner.
        void capture_output(bool capture = true);
    };
    //! \example parallel-cmds/bob.cpp

    //! A list of file paths.
    typedef std::vector<fs::path> Paths;

    //! A function that can be used in a `Recipe` to build outputs from inputs.
    typedef std::function<void(const vector<path>&, const vector<path>&)> RecipeFunc;

    //! A build recipe that defiles how to produce outputs from inputs.
    class Recipe {
    public:
        //! The input files that are used to build the outputs.
        Paths inputs;
        //! The output files that are expected to be produced by the recipe.
        Paths outputs;
        //! The function that will be called to build the outputs from the inputs.
        RecipeFunc func;

        //! Constructs a recipe with the given outputs, inputs, and build function.
        Recipe(const Paths &outputs, const Paths &inputs, RecipeFunc func);
        //! Use the modified time of the inputs and outputs to determine if the recipe needs to be rebuilt.
        bool needs_rebuild() const;
        //! Builds the outputs from the inputs using the recipe function.
        void build() const;
    };
    //! \example recipe/bob.cpp

    //! Types of command line flags.
    enum class CliFlagType {
        //! Boolean flag, e.g. `-v` or `--verbose`.
        Bool,
        //! Option with a value, e.g. `-o file.txt` or `--output file.txt`.
        Value
    };

    //! Represents a command flag argument
    struct CliFlag {

        //! The short name of the flag, e.g. `-v`.
        char    short_name  = '\0';
        //! The long name of the flag, e.g. `--verbose`.
        string  long_name   = "";
        //! Description of the flag, e.g. "Enable verbose output".
        string  description = "";
        //! Whether the flag is set or not.
        bool    set         = false;
        //! The value of the flag if it is an option with a value, e.g. `file.txt` for `--output file.txt`.
        //!
        //! Only used for options with values.
        string  value       = "";
        //! The type of the flag..
        CliFlagType type;

        //! Create a CLI flag
        CliFlag(const string &long_name, CliFlagType type, string description = "");
        //! Create a CLI flag
        CliFlag(char short_name, CliFlagType type, string description = "");
        //! Create a CLI flag
        CliFlag(char short_name, const string &long_name, CliFlagType type, string description = "");

        //! Checks if the flag is a boolean flag (no value).
        bool is_flag() const;

        //! Checks if the flag is an option (has a value).
        bool is_option() const;
    };

    //! A list of command line flags.
    typedef std::vector<CliFlag> CliFlags;

    //! Prints the command line flags and their descriptions. Used for help output.
    void print_cli_args(const CliFlags &args);

    //! A function that can be used to handle a command in a CLI.
    typedef std::function<int(CliCommand&)> CliCommandFunc;

    //! \brief Represents a CLI command
    //!
    //! @see Cli
    class CliCommand {
    public:
        //! Path to the command, e.g. {"./bob", "test", "run"}
        vector<string> path;
        //! Arguments that are not flags
        vector<string> args;
        // TODO: Make this function
        //! The name of the command
        string name;
        //! The function that will be called when this command is executed.
        CliCommandFunc func;
        //! Description of the command, e.g. "Run tests".
        string description;
        //! The flags provided to this command.
        vector<CliFlag> flags;
        //! Subcommands of this command
        vector<CliCommand> commands; // TODO: Make this a fixed size array, to avoid invalidating references in push

    private:
        [[noreturn]]
        void cli_panic(const string &msg);
        string parse_args(int argc, string argv[]);
        int call_func();

    public:

        //! Create a CLI command with a command function to be executed when this command is invoked.
        CliCommand(const string &name, CliCommandFunc func, const string &description = "");

        //! Create a CLI command which prints its subcommands and flags when invoked.
        CliCommand(const string &name, const string &description = "");

        //! Checks if this command is a menu command (has subcommands).
        bool is_menu() const;
        //! Prints the command's help message.
        void usage() const;
        //! Finds a flag by its short name.
        CliFlag* find_short(char name);
        //! Finds a flag by its long name.
        CliFlag* find_long(string name);
        //! Prints the help message if the `--help` flag is set.
        void handle_help();
        //! Runs the command with the given arguments.
        int run(int argc, string argv[]);
        //! Set the description of the command.
        void set_description(const string &desc);
        //! Sets the function to be called this command is executed without a subcommand.
        void set_default_command(CliCommandFunc f);
        //! Adds a subcommand to this command.
        //! Returns a reference to the new command.
        CliCommand& add_command(CliCommand command);
        //! Adds a subcommand with a command function to this command. Returns a reference to the new command.
        CliCommand& add_command(const string &name, string description, CliCommandFunc func);
        //! Adds a subcommand to this command. Returns a reference to the new command.
        CliCommand& add_command(const string &name, string description);
        //! Adds a subcommand with a command function and without a description to this command.
        //! Returns a reference to the new command.
        CliCommand& add_command(const string &name, CliCommandFunc func);
        //! Adds a subcommand without a description to this command. Returns a reference to the new command.
        CliCommand& add_command(const string &name);
        //! Adds a flag argument to this command.
        CliCommand& add_flag(const CliFlag &arg);
        //! Adds a short flag argument to this command.
        CliCommand& add_flag(char short_name, CliFlagType type, string description = "");
        //! Adds a long flag argument to this command.
        CliCommand& add_flag(const string &long_name, CliFlagType type, string description = "");
        //! Adds a short and long flag argument to this command.
        CliCommand& add_flag(char short_name, const string &long_name, CliFlagType type, string description = "");
        //! Adds a short and long flag argument to this command.
        CliCommand& add_flag(const string &long_name, char short_name, CliFlagType type, string description = "");
        //! Creates a command which is an alias for this command.
        CliCommand alias(const string &name, const string &description = "");
    };

    //! A command line interface (CLI) that can be used to run commands and subcommands.
    class Cli : public CliCommand {
        vector<string> raw_args;
        void set_defaults(int argc, char* argv[]);
    public:
        //! Create a new CLI interface.
        Cli(int argc, char* argv[]);
        //! Create a new CLI interface with a title.
        Cli(string title, int argc, char* argv[]);
        //! Run the CLI and handle the commands.
        int serve();
    };
    //! \example cli/bob.cpp

    //! Terminal related constants and functions.
    namespace term {

        //------------------------------------------------------------------------
        //! @name Reset Style
        //! @{
        const std::string RESET = "\033[0m"; //!< Reset style
        //! @}

        //------------------------------------------------------------------------
        //! @name Regular Colors
        //! @{
        const std::string BLACK   = "\033[30m"; //!< Black text
        const std::string RED     = "\033[31m"; //!< Red text
        const std::string GREEN   = "\033[32m"; //!< Green text
        const std::string YELLOW  = "\033[33m"; //!< Yellow text
        const std::string BLUE    = "\033[34m"; //!< Blue text
        const std::string MAGENTA = "\033[35m"; //!< Magenta text
        const std::string CYAN    = "\033[36m"; //!< Cyan text
        const std::string WHITE   = "\033[37m"; //!< White text
        //! @}

        //------------------------------------------------------------------------
        //! @name Bright Colors
        //! @{
        const std::string BRIGHT_BLACK   = "\033[90m"; //!< Bright black text
        const std::string BRIGHT_RED     = "\033[91m"; //!< Bright red text
        const std::string BRIGHT_GREEN   = "\033[92m"; //!< Bright green text
        const std::string BRIGHT_YELLOW  = "\033[93m"; //!< Bright yellow text
        const std::string BRIGHT_BLUE    = "\033[94m"; //!< Bright blue text
        const std::string BRIGHT_MAGENTA = "\033[95m"; //!< Bright magenta text
        const std::string BRIGHT_CYAN    = "\033[96m"; //!< Bright cyan text
        const std::string BRIGHT_WHITE   = "\033[97m"; //!< Bright white text
        //! @}

        //------------------------------------------------------------------------
        //! @name Background Colors
        //! @{
        const std::string BG_BLACK   = "\033[40m"; //!< Black background
        const std::string BG_RED     = "\033[41m"; //!< Red background
        const std::string BG_GREEN   = "\033[42m"; //!< Green background
        const std::string BG_YELLOW  = "\033[43m"; //!< Yellow background
        const std::string BG_BLUE    = "\033[44m"; //!< Blue background
        const std::string BG_MAGENTA = "\033[45m"; //!< Magenta background
        const std::string BG_CYAN    = "\033[46m"; //!< Cyan background
        const std::string BG_WHITE   = "\033[47m"; //!< White background
        //! @}

        //------------------------------------------------------------------------
        //! @name Bright Backgrounds
        //! @{
        const std::string BG_BRIGHT_BLACK   = "\033[100m"; //!< Bright black background
        const std::string BG_BRIGHT_RED     = "\033[101m"; //!< Bright red background
        const std::string BG_BRIGHT_GREEN   = "\033[102m"; //!< Bright green background
        const std::string BG_BRIGHT_YELLOW  = "\033[103m"; //!< Bright yellow background
        const std::string BG_BRIGHT_BLUE    = "\033[104m"; //!< Bright blue background
        const std::string BG_BRIGHT_MAGENTA = "\033[105m"; //!< Bright magenta background
        const std::string BG_BRIGHT_CYAN    = "\033[106m"; //!< Bright cyan background
        const std::string BG_BRIGHT_WHITE   = "\033[107m"; //!< Bright white background
        //! @}

        //------------------------------------------------------------------------
        //! @name Text Styles
        //! @{
        const std::string BOLD      = "\033[1m"; //!< Bold style
        const std::string DIM       = "\033[2m"; //!< Dim style
        const std::string UNDERLINE = "\033[4m"; //!< Underline style
        const std::string BLINK     = "\033[5m"; //!< Blink style
        const std::string INVERT    = "\033[7m"; //!< Invert style
        const std::string HIDDEN    = "\033[8m"; //!< Hidden style
        //! @}

        //! Terminal size in characters.
        struct TermSize {
            size_t w; //!< Terminal Width
            size_t h; //!< Terminal Height
        };

        //! Returns the size of the terminal in characters.
        TermSize size();
    }
}

#endif // BOB_H_

#ifdef BOB_IMPLEMENTATION
//! \cond DO_NOT_DOCUMENT

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

        auto output_mtime = fs::last_write_time(output);
        auto input_mtime  = fs::last_write_time(input);

        return output_mtime < input_mtime;
    }


    Cmd RebuildConfig::cmd(string source, string program) const {
        Cmd cmd;
        for (const string &part : parts) {
            if      (part == PROGRAM) cmd.push(program);
            else if (part == SOURCE)  cmd.push(source);
            else                      cmd.push(part);
        }
        return cmd;
    }

    void go_rebuild_yourself(int argc, char* argv[], path source_file_name, RebuildConfig config) {
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
                [binary_path, source_path, config](Paths, Paths) {
                    config.cmd(source_path.string(), binary_path.string()).check();
                }
        );

        if (rebuild_yourself.needs_rebuild()) {
            rebuild_yourself.build();
            exit(run_yourself(binary_path, argc, argv));
        }

    }

    bool find_root(path * root, string marker_file) {
        path &root_path = *root;
        root_path = fs::current_path();
        while (root_path != root_path.root_path()) {
            if (fs::exists(root_path / marker_file)) {
                return true;
            }
            root_path = root_path.parent_path();
        }
        return false;
    }

    bool git_root(path * root) {
        return find_root(root, ".git");
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
        bool got_data = read_fd(output_fd, &new_output);
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

    string Cmd::render() const {
        string result;
        for (const auto &part : parts) {
            if (!result.empty()) result += " ";
            result += part;
        }
        if (root != ".") {
            path rel_root = fs::relative(root, fs::current_path());
            result = "[from '" + rel_root.string() + "'] " + result;
        }
        return result;
    }

    CmdFuture Cmd::run_async() const {
        if (parts.empty() || parts[0].empty()) {
            PANIC("No command to run.");
        }

        std::cout << "CMD: " << render() << std::endl;

        // Pseudo-terminal for line-buffered output
        int output_fd;
        pid_t cpid = forkpty(&output_fd, nullptr, nullptr, nullptr);
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
        fcntl(output_fd, F_SETFL, O_NONBLOCK);

        CmdFuture future;
        future.cpid = cpid;
        future.output_fd = output_fd;
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
        bool done = fut.poll(&output_str);
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
            size_t index = cursor++;
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
        return cursor < cmds.size();
    }

    void CmdRunner::init_slots() {
        for (auto &slot : slots) {
            slot.fut.done = true;
            slot.fut.output_fd = -1;
            slot.index = -1; // Mark as empty
        }
    }

    CmdRunner::CmdRunner(size_t process_count):
        process_count(process_count),
        slots(vector<CmdRunnerSlot>(process_count))
    {
        init_slots();
        assert(process_count > 0 && "Process count must be greater than 0");
    };

    CmdRunner::CmdRunner(vector<Cmd> cmds) :
        process_count(sysconf(_SC_NPROCESSORS_ONLN)),
        slots(vector<CmdRunnerSlot>(process_count)),
        cmds(std::move(cmds))
    {
        init_slots();
        if (process_count == 0) process_count = 1; // Fallback
    }

    CmdRunner::CmdRunner(vector<Cmd> cmds, size_t process_count) :
        process_count(process_count),
        slots(vector<CmdRunnerSlot>(process_count)),
        cmds(std::move(cmds))
    {
        init_slots();
        if (process_count == 0) process_count = 1; // Fallback
    }

    CmdRunner::CmdRunner():
        process_count(sysconf(_SC_NPROCESSORS_ONLN)),
        slots(vector<CmdRunnerSlot>(process_count))
    {
        init_slots();
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
        cursor = 0;
        populate_slots();
        while (any_waiting()) {
            if (populate_slots()) continue;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        await_slots();
        return !any_failed();
    }

    bool CmdRunner::all_succeded() {
        return !any_failed();
    }

    bool CmdRunner::any_failed() {
        for (auto &exit_code : exit_codes) {
            if (exit_code != 0) return true;
        }
        return false;
    }
    
    void CmdRunner::print_failed() {
        for (size_t i = 0; i < cmds.size(); ++i) {
            if (exit_codes[i] != 0) {
                std::cerr << term::RED << "[FAILED] " << cmds[i].render() << " (exit code: " << exit_codes[i] << ")" << term::RESET << std::endl;
                if (!cmds[i].output_str.empty()) {
                    std::cerr << cmds[i].output_str;
                }
            }
        }
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

    void print_cli_args(const CliFlags &args) {
        auto arg_len = [](const CliFlag &arg) {
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

        auto arg_placeholder = [](const CliFlag &arg) -> string {
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

    CliFlag::CliFlag(const string &long_name, CliFlagType type, string description):
            long_name(long_name), type(type), description(description) {};

    CliFlag::CliFlag(char short_name, CliFlagType type, string description):
            short_name(short_name), type(type), description(description) {};

    CliFlag::CliFlag(char short_name, const string &long_name, CliFlagType type, string description):
            short_name(short_name), long_name(long_name), type(type), description(description) {};

    bool CliFlag::is_flag() const {
        return type == CliFlagType::Bool;
    }

    bool CliFlag::is_option() const {
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

            for (CliFlag &arg : flags) {

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

    CliFlag* CliCommand::find_short(char name) {
        for (CliFlag &arg : flags) {
            if (arg.short_name == name) return &arg;
        }
        return nullptr; // Not found
    }

    CliFlag* CliCommand::find_long(string name) {
        for (CliFlag &arg : flags) {
            if (arg.long_name == name) return &arg;
        }
        return nullptr; // Not found
    }

    void CliCommand::handle_help() {
        CliFlag *help_arg = find_long("help");
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
                    CliFlag &arg = flags[i];

                    // Skip if the argument is already set
                    if (cmd.find_short(arg.short_name)) continue;
                    if (cmd.find_long(arg.long_name))   continue;

                    cmd.add_flag(arg);
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

    CliCommand& CliCommand::add_flag(const CliFlag &arg) {
        CliFlag * short_existing = find_short(arg.short_name);
        CliFlag * long_existing  = find_long(arg.long_name);

        if (short_existing) PANIC("Short argument already exists: " + string({arg.short_name}));
        if (long_existing)  PANIC("Long argument already exists: " + arg.long_name);

        flags.push_back(arg);

        return *this;
    }

    CliCommand& CliCommand::add_flag(char short_name, CliFlagType type, string description) {
        return add_flag(CliFlag(short_name, type, description));
    }

    CliCommand& CliCommand::add_flag(const string &long_name, CliFlagType type, string description) {
        return add_flag(CliFlag(long_name, type, description));
    }

    CliCommand& CliCommand::add_flag(char short_name, const string &long_name, CliFlagType type, string description) {
        return add_flag(CliFlag(short_name, long_name, type, description));
    }

    CliCommand& CliCommand::add_flag(const string &long_name, char short_name, CliFlagType type, string description) {
        return add_flag(CliFlag(short_name, long_name, type, description));
    }

    void Cli::set_defaults(int argc, char* argv[]) {
        assert(argc > 0 && "No program provided via argv[0]");

        name = argv[0];
        path.push_back(name);

        raw_args.assign(argv + 1, argv + argc);

        // Add help argument. This will be inherited by all sub commands.
        add_flag("help", 'h', CliFlagType::Bool, "Prints this help message");
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

//! \endcond
#endif // BOB_IMPLEMENTATION
