#define BOB_IMPLEMENTATION
#include "../../bob.hpp"

#include <cstdlib>
#include <unistd.h>

using namespace bob;
using namespace std;

int main(int argc, char* argv[]) {
    GO_REBUILD_YOURSELF(argc, argv);

    Cli cli("Bob CLI Example", argc, argv);

    cli.add_arg("verbose", 'v', CliFlagType::Bool, "Enable verbose output");

    cli.add_command("hello", "Prints a hello message", [](CliCommand &_){
        cout << "Hello, my name is Bob!" << endl;
        return EXIT_FAILURE;
    });

    CliCommand &submenu = cli.add_command("submenu", "A submenu of commands");
    submenu.add_command("subcommand1", "A subcommand in the submenu", [](CliCommand &cmd) {
        cout << "This is the FIRST subcommand!!" << endl;
        return EXIT_SUCCESS;
    });
    submenu.add_command("subcommand2", "A subcommand in the submenu", [](CliCommand &cmd) {
        cout << "This is the SECOND subcommand!!" << endl;
        return EXIT_SUCCESS;
    });

    cli.add_command("path", "Prints the path of this command", [](CliCommand &cmd) -> int {
        cmd.handle_help();
        cout << "Path: ";
        for (const auto &part : cmd.path) {
            cout << part << " ";
        }
        cout << endl;
        return EXIT_SUCCESS;
    });

    cli.add_command("args", "Prints the arguments passed to the CLI", [](CliCommand &cmd) -> int {
        cmd.handle_help();
        cout << "Arguments:" << endl;
        for (int i = 0; i < cmd.args.size(); ++i) {
            cout << "    argv[" << i << "]: " << cmd.args[i] << endl;
        }
        return EXIT_SUCCESS;
    });

    cli.add_command("flags", "Prints prints its flag arguments and their values", [](CliCommand &cmd) -> int {
        cmd.handle_help();
        for (const auto &flag : cmd.flags) {
            cout << "    Argument: " << (flag.long_name.empty() ? "<empty>" : flag.long_name)
                 << " (short: " << (flag.short_name ? string({flag.short_name}) : "<empty>") << ")"
                 << ", Type: " << (flag.type == CliFlagType::Bool ? "Flag" : "Option")
                 << ", Value: " << (flag.value.empty() ? "<none>" : flag.value)
                 << ", Set: " << (flag.set ? "true" : "false") << endl;
        }
        return EXIT_SUCCESS;
    })
        .add_arg("an-argument", 'a', CliFlagType::Value, "An argument with a value")
        .add_arg("flag",        'f', CliFlagType::Bool,   "A simple flag argument")
        .add_arg("better-v",    'v', CliFlagType::Bool,   "A better -v flag than the global one");

    return cli.serve();
}
