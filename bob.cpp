#include "bob.hpp"
#include <cstdlib>
#include <unistd.h>

using namespace bob;
using namespace std;

int main(int argc, char* argv[]) {
    go_rebuild_yourself(argc, argv);

    Cli cli("Bob CLI Example");

    auto test = [](CliCommand * _) {
        cout << "Hello, my name is Bob!" << endl;
        return EXIT_FAILURE;
    };


    cli.add_command("hello", test, "Prints a hello message");

    return cli.serve(argc, argv);
}
