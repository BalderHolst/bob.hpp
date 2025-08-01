#include <cstdlib>
#define BOB_IMPLEMENTATION
#include "bob.hpp"

#include <filesystem>
#include <iostream>
#include <sys/inotify.h>

using namespace bob;
using namespace std;

const int DEFAULT_SERVER_PORT = 8000;

const path TEST_DIR = path(__FILE__).parent_path().parent_path() / "simple-examples";

enum class Action {
    Record,
    Replay,
};

vector<path> find_test_cases() {
    vector<path> cases;
    for (const auto &entry : fs::directory_iterator(TEST_DIR)) {
        path test_case = entry.path();
        if (fs::is_directory(test_case)) {
            cases.push_back(test_case);
        }
    }
    return cases;
}

void label(size_t width, const string &text, const string &color = "", char fill = '=', size_t left_padding = 8) {
    size_t right_padding = (width - text.length()) - 2 - left_padding;
    cout << color;
    cout << setw(left_padding) << setfill(fill) << "";
    cout << " " << text << " ";
    cout << setw(right_padding) << setfill(fill) << "";
    cout << term::RESET;
    cout << endl;
}

void line(size_t width, const string color = "", char fill = '=') {
    cout << color << setw(width) << setfill(fill) << "" << term::RESET << endl;
}

int test(CliCommand &cmd, Action action, path test_case = "") {
    cmd.handle_help();

    ensure_installed({"git", "g++", "python3"});

    vector<path> test_cases;
    if (!test_case.empty()) {
        test_cases.push_back(test_case);
    } else {
        test_cases = find_test_cases();
        if (test_cases.empty()) {
            cerr << "No test cases found in " << TEST_DIR << endl;
            return EXIT_FAILURE;
        }
    }

    CmdRunner runner;

    cout << "\nCleaning test directories..." << endl;
    runner.clear();
    for (const path &test_case : test_cases) {
        runner.push(Cmd({"git", "clean", "-xdf", test_case}));
    }
    runner.run();

    cout << "\nCompiling test cases..." << endl;
    runner.clear();
    for (const path &test_case : test_cases) {
        runner.push(Cmd({"g++", "bob.cpp", "-o", "bob"}, test_case));
    }
    runner.run();

    cout << "\nRunning tests..." << endl;
    runner.clear();
    for (const auto &test_case : test_cases) {
        path rere_path = fs::relative(git_root().unwrap() / "rere.py", test_case);
        runner.push(Cmd({
                    "python3",
                    rere_path,
                    (action == Action::Record) ? "record" : "replay",
                    "test.list"}, test_case));
    }
    runner.capture_output(true);
    runner.run();

    if (!runner.any_failed()) {
        return EXIT_SUCCESS;
    }

    // Something went wrong...

    // Collect failed commands
    vector<Cmd *> failed_cmds;
    for (int i = 0; i < runner.size(); i++) {
        auto &cmd       = runner.cmds[i];
        auto &exit_code = runner.exit_codes[i];
        if (exit_code != 0) {
            failed_cmds.push_back(&cmd);
        }
    }

    size_t w = term::size().w;

    cout << endl;
    for (auto cmd : failed_cmds) {
        label(w, cmd->render(), term::RED);
        cout << cmd->stdout_str << endl;
    }
    line(w, term::RED);

    // TODO: ?/? tests passed
    cout << term::RED << "\nSome commands failed:" << endl;

    for (auto cmd : failed_cmds) {
        cout << "    " << cmd->render() << endl;
    }

    cout << term::RESET;

    return EXIT_FAILURE;
}

void add_test_commands(Cli &cli) {
    CliCommand &record = cli.add_command("record", "Record tests", [](CliCommand &cmd) {
        return test(cmd, Action::Record);
    });

    // Add a sub command for every test case
    for (const auto &test_case : find_test_cases()) {
        string test_name = test_case.filename().string();
            record.add_command(test_name, "Record test case: " + test_name, [test_case](CliCommand &cmd) {
            return test(cmd, Action::Record, test_case);
        });
    }

    CliCommand &replay = cli.add_command("replay", "Replay tests", [](CliCommand &cmd) {
        return test(cmd, Action::Replay);
    });

    // Add a sub command for every test case
    for (const auto &test_case : find_test_cases()) {
        string test_name = test_case.filename().string();
            replay.add_command(test_name, "Replay test case: " + test_name, [test_case](CliCommand &cmd) {
            return test(cmd, Action::Replay, test_case);
        });
    }

    // Duplicate `replay` command as `test`
    cli.add_command(replay.alias("test"));

}

void document(CliCommand &cmd) {
    cmd.handle_help();
    ensure_installed({"doxygen"});
    path root = git_root().unwrap();
    Cmd({"doxygen", "docs/Doxyfile"}, root).check();
}

void watch_changes(const vector<path> &paths, CliCommand &cmd) {
    int fd = inotify_init();
    if (fd < 0) {
        PANIC("Failed to initialize inotify");
    }

    vector<int> wds;
    for (const auto &p : paths) {
        int wd = inotify_add_watch(fd, p.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
        if (wd < 0) {
            PANIC("Failed to add inotify watch for " + p.string());
        }
        wds.push_back(wd);
    }

    cout << "Watching for changes..." << endl;

    while (true) {
        char buffer[1024];
        int length = read(fd, buffer, sizeof(buffer));
        if (length < 0) {
            PANIC("Failed to read from inotify");
        }
        document(cmd); // Rebuild documentation on change
    }
}

int serve(CliCommand &cmd) {
    document(cmd);

    cout << endl;

    path site = git_root().unwrap() / "docs" / "html";
    int port = DEFAULT_SERVER_PORT;

    auto port_arg = cmd.find_long("port");
    if (port_arg && port_arg->set) {
        try {
            port = stoi(port_arg->value);
        } catch (const std::invalid_argument &) {
            PANIC("Invalid port number: " + port_arg->value);
        }
    }

    auto watch_arg = cmd.find_long("watch");

    auto server_fut = Cmd({"python3", "-m", "http.server", to_string(port), "-d", site}).run_async();

    if (watch_arg && watch_arg->set) {

    }

    return server_fut.await();
}

void add_doc_commands(Cli &cli) {
    auto &doc_cmd = cli.add_command("doc", "Generate documentation", [](CliCommand &cmd) {
        document(cmd);
        return EXIT_SUCCESS;
    });

    doc_cmd.add_command("serve", "Serve the documentation via a local web server", serve)
        .add_arg('p', "port",  CliFlagType::Value,
                "Port to serve the documentation on (default: " + to_string(DEFAULT_SERVER_PORT) + ")")
        .add_arg('w', "watch", CliFlagType::Bool,
                "Watch for changes and rebuild the documentation automatically");

}


int main(int argc, char* argv[]) {
    GO_REBUILD_YOURSELF(argc, argv);

    Cli cli("Task CLI for the bob.hpp project", argc, argv);

    add_test_commands(cli);
    add_doc_commands(cli);

    return cli.serve();
}
