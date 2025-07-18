#include "bob.hpp"

#include <filesystem>
#include <iostream>

using namespace bob;
using namespace std;

const path TEST_DIR = path(__FILE__).parent_path().parent_path() / "simple-examples";

enum class Action {
    Record,
    Replay,
};

int run(Action action) {

    vector<path> test_cases;
    for (const auto &entry : fs::directory_iterator(TEST_DIR)) {
        path test_case = entry.path();
        if (!fs::is_directory(test_case)) continue;
        test_cases.push_back(test_case);
    }

    // Compile all test binaries
    CmdRunner compile_runner;
    for (const path &test_case : test_cases) {
        compile_runner.push(Cmd({"g++", test_case / "bob.cpp", "-o", test_case / "bob"}));
    }
    compile_runner.run();

    // Run the tests
    CmdRunner test_runner;
    for (const auto &test_case : test_cases) {
        path rere_path = fs::relative(git_root().unwrap() / "rere.py", test_case);
        test_runner.push(Cmd({
                    rere_path,
                    (action == Action::Record) ? "record" : "replay",
                    "test.list"}, test_case));
    }
    test_runner.run();

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    go_rebuild_yourself(argc, argv);

    Cli cli("Test CLI for the bob.hpp project", argc, argv);

    cli.set_default_command([](CliCommand &cmd) {
        cmd.handle_help();
        return run(Action::Replay);
    });

    cli.add_command("record", "Record tests", [](CliCommand &cmd) { return run(Action::Record); });
    cli.add_command("replay", "Replay tests", [](CliCommand &cmd) { return run(Action::Replay); });

    return cli.serve();
}
