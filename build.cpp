#include "build.hpp"
#include <unistd.h>

using namespace build;

int main(int argc, char* argv[]) {

    go_rebuild_yourself(argc, argv);

    auto res = git_root();

    if (res.is_ok()) {
        log(git_root().unwrap());
    } else {
        log(git_root().unwrap_err());
    }


    return 0;
}
