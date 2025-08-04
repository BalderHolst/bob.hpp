#define BOB_IMPLEMENTATION
#include "bob.hpp"

using namespace bob;
using namespace std;

int main(int argc, char* argv[]) {
    GO_REBUILD_YOURSELF(argc, argv);

    ensure_installed({"python3"});

    Cmd cmd({"python3", "./script.py"});
    cmd.capture_output = true;

    cmd.run();

    cout << term::BRIGHT_BLUE;
    cout << "\n======== SCRIPT OUTPUT ========" << endl;
    cout << cmd.output_str;
    cout << "===============================" << endl;
    cout << term::RESET;
}
