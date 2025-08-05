# Bob the Build... System
A C++ header only library used for defining build recipes without external dependencies. It provides a collection of classes and functions for executing shell commands, managing dependencies, running tasks in parallel, and building simple CLIs to run project tasks.

Check out the [theme song](https://www.youtube.com/watch?v=HdVg-2jn2OU)!

## Idea
This idea is directly stolen from [nob.h](https://github.com/tsoding/nob.h) by [mr. tsoding](https://github.com/tsoding) which is a very similar project, but for C.

A simple project using `bob.hpp` may look like this:

```
./examples/minimal
├── src
│   ├── add.cpp
│   └── main.cpp
├── bob.cpp
└── bob.hpp
```

`bob.hpp` is downloaded directly from this repo and added to the project. The `bob.cpp` file defines the steps required to build and do other tasks within the project.

To get started, simply include the 'bob.hpp' file in your main source file and use the 'GO_REBUILD_YOURSELF' macro. This macro checks if the executable is out of date and rebuilds and re-runs it if necessary, creating a self-rebuilding executable (coined "GO_REBUILD_YOURSELF technology" by [mr. tsoding](https://github.com/tsoding)).

A very simple build script could look like this:
```cpp
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
```

To run the build script do
```bash
g++ bob.cpp -o bob # Only once!
./bob
```

After the initial compilation, the `bob` executable will check if its source code has changed and rebuild itself before running if needed. **There is therefore no need to rebuild the executable manually after the initial bootstrapping**.

This build script simply runs the following command:
```bash
g++ src/main.cpp src/add.cpp -o main -Wall -Wextra -O2
```


## Key Features
- **Self-Rebuilding Executable**: The library's core feature allows your build tool to automatically rebuild and restart itself whenever its source code changes, ensuring you're always running the latest version.
- **Command Execution**: The 'bob::Cmd' class provides a simple way to execute system commands synchronously ('run()') or asynchronously ('run_async()') and to capture their output.
- **Parallel Execution**: The 'bob::CmdRunner' class enables you to run multiple commands concurrently, utilizing multiple processor cores to speed up build processes.
- **Dependency Management**: The 'bob::Recipe' class helps manage build dependencies by checking file timestamps to determine if a build step is needed. This is useful for compiling source files only when they are newer than their corresponding object files.
- **Command-Line Interface (CLI)**: The 'bob::Cli' and 'bob::CliCommand' classes make it easy to create structured command-line tools with subcommands, flags, and help messages, providing a user-friendly interface to your build tool.
- **File System Utilities**: Bob includes a few helper functions for common tasks, such as creating directories ('mkdirs'), finding binaries in the system's '$PATH' ('search_path'), and locating the root of a Git repository ('git_root').
- **Terminal Styling**: The 'bob::term' namespace offers a set of constants for adding colors and styling to your terminal output, making your build logs more readable.

## Rebuild Configuration
You can customize the rebuild command used by 'GO_REBUILD_YOURSELF' by defining 'BOB_REBUILD_CMD'. The command is a C++ initializer list of strings, with `_PROGRAM_` and `_SOURCE_` placeholders for the executable and source file names, respectively.

```cpp
#define BOB_REBUILD_CMD { "clang++", "-o", "_PROGRAM_", "_SOURCE_", "-Wall", "-O2" }
#include "bob.hpp"
```

Alternatively, you can provide a custom 'RebuildConfig' object directly to the 'go_rebuild_yourself' function instead of using the `GO_REBUILD_YOURSELF` macro:

```cpp
bob::RebuildConfig custom_config({ "clang++", "-o", "_PROGRAM_", "_SOURCE_", "-Wall", "-O2" });
bob::go_rebuild_yourself(argc, argv, __FILE__, custom_config);
```


