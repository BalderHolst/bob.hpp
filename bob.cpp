#include "bob.hpp"
#include <cstdlib>
#include <unistd.h>

using namespace bob;
using namespace std;

int main(int argc, char* argv[]) {
    go_rebuild_yourself(argc, argv);

    Cli cli("Bob CLI Example", argc, argv);

    cli.add_arg("verbose", 'v', CliArgType::Flag, "Enable verbose output");

    cli.add_command("hello", "Prints a hello message", [](CliCommand &_){
        cout << "Hello, my name is Bob!" << endl;
        return EXIT_FAILURE;
    });

    cli.add_command("submenu", "A submenu of commands").subcommand()
        .add_command("subcommand1", "A subcommand in the submenu", [](CliCommand &cmd) {
            cout << "This is the FIRST subcommand!!" << endl;
            return EXIT_SUCCESS;
        })
        .add_command("subcommand2", "A subcommand in the submenu", [](CliCommand &cmd) {
            cout << "This is the SECOND subcommand!!" << endl;
            return EXIT_SUCCESS;
        });

    cli.add_command("Values", "Prints the raw arguments passed to the CLI", [](CliCommand &cmd) -> int {
        cmd.handle_help(); // Handle help argument if set
        cout << "Arguments" << endl;
        for (int i = 0; i < cmd.value_args.size(); ++i) {
            cout << "    argv[" << i << "]: " << cmd.value_args[i] << endl;
        }
        return EXIT_SUCCESS;
    });

    cli.add_command("args", "Prints prints its flag arguments and their values", [](CliCommand &cmd) -> int {
        cmd.handle_help(); // Handle help argument if set
        for (const auto &arg : cmd.args) {
            cout << "    Argument: " << (arg.long_name.empty() ? "<empty>" : arg.long_name)
                 << " (short: " << (arg.short_name ? string({arg.short_name}) : "<empty>") << ")"
                 << ", Type: " << (arg.type == CliArgType::Flag ? "Flag" : "Option")
                 << ", Value: " << (arg.value.empty() ? "<none>" : arg.value)
                 << ", Set: " << (arg.set ? "true" : "false") << endl;
        }
        return EXIT_SUCCESS;
    }).subcommand()
        .add_arg("an-argument", 'a', CliArgType::Option, "An argument with a value")
        .add_arg("flag",        'f', CliArgType::Flag,   "A simple flag argument")
        .add_arg("better-v",    'v', CliArgType::Flag,   "A better -v flag than the global one");

    return cli.serve();
}
