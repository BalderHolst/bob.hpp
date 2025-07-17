#include "bob.hpp"
#include <cstdlib>
#include <unistd.h>

using namespace bob;
using namespace std;

int main(int argc, char* argv[]) {
    go_rebuild_yourself(argc, argv);

    Cli cli(argc, argv, "Bob CLI Example");

    auto hello = [](CliCommand &_) -> int {
        cout << "Hello, my name is Bob!" << endl;
        return EXIT_FAILURE;
    };

    cli.add_command("hello", hello, "Prints a hello message");

    cli.add_arg("verbose", 'v', CliArgType::Flag, "Enable verbose output");
    cli.add_arg('s', CliArgType::Option, "Short option with a value");

    cli.add_arg("long-option", CliArgType::Option, "Long option with a value");

    CliCommand &submenu = cli.add_command("submenu", "A submenu of commands");

    submenu.add_command("subcommand", [](CliCommand &cmd) -> int {
        cout << "This is a subcommand in the submenu!" << endl;
        return EXIT_SUCCESS;
    }, "A subcommand in the submenu");

    return cli.serve();
}
