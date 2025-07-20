#define BOB_IMPLEMENTATION
#include "../../bob.hpp"

#include <filesystem>
#include <algorithm>

using namespace bob;
using namespace std;

const bool CACHE_RAYLIB   = true;
const bool CACHE_EXAMPLES = false;

#define PLATFORM LINUX_WAYLAND
// #define PLATFORM LINUX_X11

const path BUILD_DIR = "./build";
const path RAYLIB_SRC_DIR = "./raylib/src";
const path RAYLIB_EXAMPLES_DIR = "./raylib/examples";

const path RAYLIB_BUILD_DIR = BUILD_DIR / "raylib";
const path OUTPUT_DIR       = BUILD_DIR / "examples";

const vector<string> skip_modules = {"others"};

const vector<string> EXAMPLE_CFLAGS = {
    "-Wall", "-std=c99",
    "-D_DEFAULT_SOURCE",
    "-Wno-missing-braces", "-Wunused-result",
    "-lm",
};

path build_raylib() {
    dir(RAYLIB_BUILD_DIR);

    cout << "Building Raylib..." << endl;
    Cmd cmd({"make"});

    if (!CACHE_RAYLIB) cmd.push("-B");

    cmd.push("RAYLIB_RELEASE_PATH=" + fs::absolute(RAYLIB_BUILD_DIR).string());

    cmd.push("PLATFORM=PLATFORM_DESKTOP");
    // cmd.push("RAYLIB_LIBTYPE=SHARED");

    #if PLATFORM == LINUX_WAYLAND
    cmd.push("GLFW_LINUX_ENABLE_WAYLAND=TRUE");
    cmd.push("GLFW_LINUX_ENABLE_X11=FALSE");
    #elif PLATFORM == LINUX_X11
    build_cmd.push("GLFW_LINUX_ENABLE_X11=TRUE");
    build_cmd.push("GLFW_LINUX_ENABLE_WAYLAND=FALSE");
    #else
    #error "Unsupported platform"
    #endif

    if (cmd.run(RAYLIB_SRC_DIR) == 0) {
        return RAYLIB_BUILD_DIR;
    } else {
        panic("Could not compile raylib.");
    }
}

Cmd build_example_cmd(path src, path bin) {
    dir((bin).parent_path());
    Cmd cmd({"gcc", "-o", bin});

    cmd.push_many(EXAMPLE_CFLAGS);

    // Source files
    cmd.push_many({src, RAYLIB_BUILD_DIR / "libraylib.a"});

    // Headers
    cmd.push({"-I" + RAYLIB_SRC_DIR.string()});

    return cmd;
}

void build_examples(path raylib) {
    cout << "Building Raylib examples..." << endl;

    vector<string> error_files;
    vector<string> error_msgs;

    CmdRunner runner;

    for (path module_dir : fs::directory_iterator(RAYLIB_EXAMPLES_DIR)) {
        if (!fs::is_directory(module_dir)) continue;

        auto v = skip_modules;
        auto x = module_dir.filename().string();
        if (std::find(v.begin(), v.end(), x) != v.end()) {
            continue;
        }

        for (path example_src : fs::directory_iterator(module_dir)) {

            if (example_src.filename() == "resources") {
                path module_resources = example_src;
                path output_resources_dir = OUTPUT_DIR / fs::relative(module_resources, RAYLIB_EXAMPLES_DIR);
                if (fs::exists(output_resources_dir)) continue;
                Cmd copy_resources({"cp", "-r", module_resources, output_resources_dir});
                copy_resources.run();
            }

            if (example_src.extension() != ".c") continue;
            path example_bin = OUTPUT_DIR / fs::relative(example_src, RAYLIB_EXAMPLES_DIR).replace_extension("");

            if (CACHE_EXAMPLES && fs::exists(example_bin)) {
                cout << "Found existing binary: " << example_bin << endl;
                continue;
            }

            auto cmd = build_example_cmd(example_src, example_bin);

            runner.push(cmd);
        }

        runner.run();

    }
}

int main(int argc, char* argv[]) {
    GO_REBUILD_YOURSELF(argc, argv);

    path raylib = build_raylib();
    build_examples(raylib);

    return 0;
}
