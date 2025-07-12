#include "build.hpp"

int main(int argc, char* argv[]) {
    go_rebuild_yourself(argc, argv);

    auto cmd = CMD ("python", "-c", "exit(11)");
    int status = cmd.run();

    return status;
}
