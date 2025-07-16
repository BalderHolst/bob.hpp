#include "build.hpp"
#include <unistd.h>

int main(int argc, char* argv[]) {
    go_rebuild_yourself(argc, argv);

    auto cmd = Cmd("python");
    cmd.push("-c");
    cmd.push("import os; print(f'Working Directory: {os.getcwd()}')");
    cmd.run("/tmp");

    return 0;
}
