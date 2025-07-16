#include "bob.hpp"
#include <cstdlib>
#include <unistd.h>

using namespace bob;
using namespace std;

int main(int argc, char* argv[]) {

    go_rebuild_yourself(argc, argv);

    auto runner = CmdRunner(10);

    for (int i = 0; i <= 100; ++i) {

        auto random = [] () {
            return static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
        };

        string script = "import random;"
                        "import time;"
                        "print('Job " + to_string(i) + " started...'); "
                        "time.sleep(" + to_string(random()) + "); "
                        "print('Job + " + to_string(i) + " finished!')";

        auto cmd = Cmd({"python", "-c", script});

        runner.push(cmd);
    }

    runner.run();

    return EXIT_SUCCESS;
}
