#define BOB_IMPLEMENTATION
#include "bob.hpp"

using namespace bob;

const path SRC             = "src";
const string CPP           = "g++";
const vector<string> FLAGS = {"-Wall", "-Wextra", "-O2"};

int main(int argc, char *argv[]) {
    GO_REBUILD_YOURSELF(argc, argv); // Recompile if the source files have changed

    return Cmd({CPP, SRC / "main.cpp", SRC / "add.cpp", "-o", "main"})
        .push_many(FLAGS)
        .run();
}
