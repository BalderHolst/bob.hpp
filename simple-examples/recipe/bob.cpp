#define BOB_IMPLEMENTATION
#include "../../bob.hpp"

using namespace std;
using namespace bob;

const string CC = "gcc";
const vector<string> CFLAGS = {"-Wall", "-Wextra", "-O2"};

const auto build_objs = Recipe({"./build/main.o", "./build/other.o"}, {"./src/main.c", "./src/other.c"},
    [](const vector<path> &inputs, const vector<path> &outputs) {
        assert(inputs.size() == outputs.size());
        mkdirs("./build");
        CmdRunner runner;
        for (int i = 0; i < inputs.size(); ++i) {
            runner.push(Cmd({CC, "-c", inputs[i].string(), "-o", outputs[i].string()}).push_many(CFLAGS));
        }
        runner.run();
});

const auto build_main = Recipe({"main"}, {"./build/main.o", "./build/other.o"},
    [](const vector<path> &inputs, const vector<path> &outputs) {
    assert(outputs.size() == 1);
    auto cmd = Cmd({CC, "-o", outputs[0]});
    for (const auto &input : inputs) {
        cmd.push(input.string());
    }
    cmd.run();
});

int main(int argc, char *argv[]) {
    GO_REBUILD_YOURSELF(argc, argv);

    build_objs.build();
    build_main.build();
}
