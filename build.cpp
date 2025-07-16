#include "build.hpp"

int main(int argc, char* argv[]) {
    go_rebuild_yourself(argc, argv);

    auto cmd = Cmd("echo");
    cmd.push_str("hello!");

    cmd.run();

    return 0;
}
