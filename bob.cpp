#include "bob.hpp"
#include <cstdlib>
#include <unistd.h>

using namespace bob;
using namespace std;

int main(int argc, char* argv[]) {
    go_rebuild_yourself(argc, argv);

    Cli cli("Bob CLI Example");

    auto hello = [](CliCommand * _) {
        cout << "Hello, my name is Bob!" << endl;
        return EXIT_FAILURE;
    };

    cli.add_command("hello", hello, "Prints a hello message");

    cli.add_arg("verbose", 'v', CliArgType::Flag, "Enable verbose output");
    cli.add_arg('s', CliArgType::Option, "Short option with a value");

    cli.add_arg("long-option", CliArgType::Option, "Long option with a value");

    return cli.serve(argc, argv);
}
