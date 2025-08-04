# bob.hpp - Build C++ with C++
A C++ header only library used for defining build recipes without external dependencies.

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

A very simple build script could look like this:
```cpp
#define BOB_IMPLEMENTATION
#include "bob.hpp"

using namespace bob;

const path SRC             = "src";
const string CPP           = "g++";
const vector<string> FLAGS = {"-Wall", "-Wextra", "-O2"};

int main(int argc, char *argv[]) {
    GO_REBUILD_YOURSELF(argc, argv);
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


## Features
- [x] Build and run shell commands
- [x] Run commands in parallel
- [x] Define build recipies
- [x] Simple CLI builder
- [ ] Cross platforn (linux & windows)

