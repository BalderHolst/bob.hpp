#include <cstdlib>
#define BOB_IMPLEMENTATION
#define BOB_REBUILD_CMD { "g++", "-o", "<PROGRAM>", "<SOURCE>" }
#include "bob.hpp"

#include <filesystem>
#include <iostream>
#include <thread>
#include <fstream>

using namespace bob;
using namespace std;

const int DEFAULT_SERVER_PORT = 8000;

const path TEST_DIR = path(__FILE__).parent_path().parent_path() / "examples";

enum class Action {
    Record,
    Replay,
};

size_t term_width() {
    size_t w = term::size().w;
    if (w < 80) w = 80; // Ensure a minimum width for the output
    if (w > 1000) w = 100; // If the terminal is VERY wide, limit it to 100 characters (fix for CI)
    return w;
}

vector<path> find_test_cases() {
    vector<path> cases;
    for (const auto &entry : fs::directory_iterator(TEST_DIR)) {
        path test_case = entry.path();
        if (fs::is_directory(test_case)) {
            if (!fs::exists(test_case / "test.list")) continue;
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

path find_root() {
    path root;
    if (!git_root(&root)) {
        PANIC("Could not find git root directory.");
    }
    return root;
}

int test(CliCommand &cmd, Action action, path test_case = "") {
    cmd.handle_help();

    ensure_installed({"git", "g++", "python3"});

    path root = find_root();

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
    if (runner.any_failed()) {
        runner.print_failed();
        return EXIT_FAILURE;
    }

    cout << "\nCompiling test cases..." << endl;
    runner.clear();
    for (const path &test_case : test_cases) {
        runner.push(Cmd({"g++", "bob.cpp", "-o", "bob"}, test_case));
    }
    runner.run();
    if (runner.any_failed()) {
        runner.print_failed();
        return EXIT_FAILURE;
    }

    cout << "\nRunning tests..." << endl;
    runner.clear();
    for (const auto &test_case : test_cases) {
        path rere_path = fs::relative(root / "rere.py", test_case);
        runner.push(Cmd({
                    "python3",
                    rere_path,
                    (action == Action::Record) ? "record" : "replay",
                    "test.list"}, test_case));
    }
    runner.capture_output(true);
    runner.run();

    if (runner.all_succeded()) {
        cout << term::GREEN << term::BOLD;
        if (action == Action::Record) cout << "\nOutput recorded successfully!";
        else                          cout << "\nAll tests succeeded!";
        cout << term::RESET << endl;
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

    size_t w = term_width();

    cout << endl;
    for (auto cmd : failed_cmds) {
        label(w, cmd->render(), term::RED);
        cout << cmd->output_str << endl;
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

const string CODE_MARKER = "```";
const string DOCS_MARKER = "//!";

struct CodeBlock {
    size_t start_line;
    string content;
    path file_path;
};

int doc_test(CliCommand &cmd) {

    path doctest_dir = find_root() / "docs" / "doctest";

    cmd.handle_help();

    const path HEADER = path(__FILE__).parent_path() / "bob.hpp";

    // Read header file line by line
    if (!fs::exists(HEADER)) {
        PANIC("Header file '" + HEADER.string() + "' does not exist. ");
    }

    vector<CodeBlock> code_blocks;

    std::ifstream header_file(HEADER);
    size_t line_nr = 0;
    bool in_code_block = false;
    for (std::string line; std::getline(header_file, line);) {
        line_nr++;
        if (line.find(CODE_MARKER) != std::string::npos) {
            if (!in_code_block) {
                code_blocks.push_back(CodeBlock{line_nr, ""});
                in_code_block = true;
            } else {
                in_code_block = false;
            }
        } else if (in_code_block) {
            // Add line to the current code block
            if (!code_blocks.empty()) {
                int start = line.find(DOCS_MARKER) + DOCS_MARKER.length();
                if (line[start] == ' ') start++; // Skip leading space after marker
                if (start > line.length()) start = 0;
                code_blocks.back().content += line.substr(start) + "\n";
            }
        }

    }

    cout << term::BOLD << "\nFound " << code_blocks.size() << " code blocks in the header file.\n" << term::RESET << endl;

    mkdirs(doctest_dir);

    // Generate doctest files
    path rel_header_path = fs::relative(HEADER, doctest_dir);
    for (auto &block : code_blocks) {
        path test_file = doctest_dir / ("block-" + to_string(block.start_line) + ".cpp");

        std::ofstream out(test_file);
        if (!out.is_open()) {
            PANIC("Could not open file '" + test_file.string() + "' for writing.");
        }
        out << "// Test case generated from " << HEADER.string() << " at line " << block.start_line << "\n";
        out << "#define BOB_IMPLEMENTATION"                      << "\n";
        out << "#include \"" << rel_header_path.string() << "\"" << "\n";
        out << "using namespace::bob;"                           << "\n";
        out << "#include <filesystem>"                           << "\n";
        out << "using std::filesystem::path;"                    << "\n";
        out << "using namespace::std;"                           << "\n";
        out << ""                                                << "\n";
        out << "int main(int argc, char *argv[]) {"              << "\n";
        out << block.content                                     << "\n";
        out << "    return 0;"                                   << "\n";
        out << "}"                                               << "\n";
        out.close();

        block.file_path = test_file;
    }

    // Compile the generated test files
    CmdRunner runner;
    for (const auto &block : code_blocks) {
        if (block.file_path.empty()) continue; // Skip empty blocks
        runner.push(Cmd({"g++", "-o", block.file_path.stem().string(), block.file_path.string()}, doctest_dir));
    }
    runner.run();

    size_t errors = 0;
    size_t w = term_width();
    if (runner.any_failed()) {
        cout << term::RED << "\nSome documentation examples failed to compile:" << term::RESET << endl;
        for (int i = 0; i < runner.size(); i++) {
            auto &cmd = runner.cmds[i];
            auto &block = code_blocks[i];
            if (runner.exit_codes[i] != 0) {
                label(w, HEADER.string() + ":" + to_string(block.start_line), term::RED);
                cout << cmd.output_str << endl;
                errors++;
            }
        }

        cout << term::RED;
        cout << "\n" << errors << " out of " << code_blocks.size() << " examples failed to compile." << term::RESET << endl;
        cout << term::RESET;

        return EXIT_FAILURE;
    }

    cout << term::GREEN << term::BOLD;
    cout << "\nAll documentation examples compiled successfully!";
    cout << term::RESET << endl;

    return EXIT_SUCCESS;
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

const string WARNING_LABEL = "[WARNING] ";

int document(CliCommand &cli_cmd) {
    cli_cmd.handle_help();
    ensure_installed({"git", "doxygen"});
    path root = find_root();

    // Pull theme submodule
    Cmd({"git", "submodule", "update", "--init", "--recursive"}, root).check();

    Cmd cmd({"doxygen", "docs/Doxyfile"}, root);
    cmd.capture_output = true;

    int exit_code = cmd.run();

    auto error_arg = cli_cmd.find_long("error");
    bool error = error_arg && error_arg->set;

    string color = error ? term::RED : term::YELLOW;

    cout << color;
    cout << endl;

    size_t warnings = 0;

    std::istringstream iss(cmd.output_str);
    for (std::string line; std::getline(iss, line);) {
        if (line.find(WARNING_LABEL) != 0) continue; // Skip non-warning lines
        warnings++;
        cout << line.substr(WARNING_LABEL.length()) << endl;
    }

    cout << term::RESET << endl;

    size_t w = term_width();

    cout << term::BOLD;
    if (warnings > 0) {
        if (error) exit_code = EXIT_FAILURE;
        cout << color << "Doxygen generated " << warnings << (error ? " error" : " warning") << (warnings > 1 ? "s" : "") << endl;
    }

    cout << term::RESET;

    cout << "\nDocumentation generated in: " << root / "docs" / "html" << endl;

    return exit_code;
}

int serve(CliCommand &cmd) {
    document(cmd);

    cout << endl;

    path root = find_root();

    path site = root / "docs" / "html";
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
        vector<path> watch_paths = {root / "bob.hpp", root / "docs" / "Doxyfile"};
        vector<fs::file_time_type> stats = {};

        // Initialize stats for each path
        cout << "Watching for changes in: " << endl;
        path cwd = fs::current_path();
        for (const auto &p : watch_paths) {
            cout << "    ./" << fs::relative(p, cwd).string() << endl;
            auto mtime = fs::last_write_time(p);
            stats.push_back(mtime);
        }

        assert(watch_paths.size() == stats.size());

        // Event loop to watch for changes
        for (bool done = false; !done;) {
            bool change_detected = false;

            for (int i = 0; i < watch_paths.size(); i++) {
                path &p = watch_paths[i];
                auto &mtime = stats[i];
                auto new_mtime = fs::last_write_time(p);
                if (mtime == new_mtime) continue; // No change

                cout << "Change detected in " << p.string() << ", rebuilding documentation..." << endl;
                change_detected = true;
                mtime = new_mtime; // Update the last modified time
            }

            if (change_detected) {
                document(cmd);
            }

            server_fut.poll();

            std::this_thread::sleep_for(chrono::milliseconds(20));
        }
    }

    return server_fut.await();
}

void add_doc_commands(Cli &cli) {
    auto &doc_cmd = cli.add_command("doc", "Generate documentation", document)
        .add_flag('e', "error", CliFlagType::Bool, "Treat warnings as errors");

    doc_cmd.add_command("test", "Run doctests", doc_test);

    doc_cmd.add_command("serve", "Serve the documentation via a local web server", serve)
        .add_flag('p', "port",  CliFlagType::Value,
                "Port to serve the documentation on (default: " + to_string(DEFAULT_SERVER_PORT) + ")")
        .add_flag('w', "watch", CliFlagType::Bool,
                "Watch for changes and rebuild the documentation automatically");
}

void add_readme_command(Cli &cli) {
    cli.add_command("gen-readme", "Generate README.md from README.mdx", [](CliCommand &cmd) {
        cmd.handle_help();

        bool print = cmd.find_long("print")->set;

        path root = find_root();
        path readme_mdx = root / "README.mdx";
        path readme_md = root / "README.md";

        if (!fs::exists(readme_mdx)) {
            PANIC("README.mdx file does not exist: " + readme_mdx.string());
        }

        Cmd txtx({"./txtx.py", readme_mdx.string()}, root);
        txtx.silent = !print;
        int exit_code = txtx.run();

        if (exit_code != 0) {
            cerr << term::RED << "Failed to generate README.md from README.mdx:" << term::RESET << endl;
            cerr << txtx.output_str << endl;
            return EXIT_FAILURE;
        }

        // Write README.md
        ofstream output(readme_md);
        output << txtx.output_str << endl;

        return EXIT_SUCCESS;
    })
        .add_flag('p', "print", CliFlagType::Bool,
                "Print the generated README.md content instead of writing it to a file");
}


int main(int argc, char* argv[]) {
    GO_REBUILD_YOURSELF(argc, argv);

    Cli cli("Task CLI for the bob.hpp project", argc, argv);

    add_test_commands(cli);
    add_doc_commands(cli);
    add_readme_command(cli);

    return cli.serve();
}
