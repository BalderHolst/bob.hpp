#include "../../bob.hpp"
#include <cstdlib>
#include <unistd.h>

using namespace bob;
using namespace std;

int main(int argc, char* argv[]) {

    go_rebuild_yourself(argc, argv);

    auto runner = CmdRunner(3);

    srand(0);

    for (int i = 0; i <= 20; ++i) {

        string script = "import time;"
                        "print('Job " + to_string(i) + " started...'); "
                        "time.sleep(" + to_string((float) i / 20.0) + "); "
                        "print('Job + " + to_string(i) + " finished!')";

        auto cmd = Cmd({"python3", "-c", script});

        runner.push(cmd);
    }

    runner.run();

    return EXIT_SUCCESS;
}
