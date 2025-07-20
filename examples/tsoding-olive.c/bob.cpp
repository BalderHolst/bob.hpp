#define BOB_IMPLEMENTATION
#include "../../bob.hpp"

#include <filesystem>

using namespace bob;
using namespace std;

const path BUILD_DIR  = "build";
const path TOOLS_DIR  = BUILD_DIR / "tools";
const path ASSETS_DIR = BUILD_DIR / "assets";
const path DEMOS_DIR  = BUILD_DIR / "demos";
const path WASM_DIR   = BUILD_DIR / "wasm";

const path TEST_BIN = BUILD_DIR / "test";

const path REPO_DIR       = "./olive.c";
const path TOOLS_SRC_DIR  = REPO_DIR / "tools";
const path ASSETS_SRC_DIR = REPO_DIR / "assets";
const path DEMOS_SRC_DIR  = REPO_DIR / "demos";

const path DEV_DEPS_DIR = REPO_DIR / "dev-deps";

const string CC = "clang";

const bool CACHE = true;

#define COMMON_CFLAGS "-Wall", "-Wextra", "-pedantic", I(REPO_DIR), I(BUILD_DIR), I(DEV_DEPS_DIR), "-ggdb"

void build_tools() {
    dir(TOOLS_DIR);
    path png2c = TOOLS_DIR / "png2c";
    path obj2c = TOOLS_DIR / "obj2c";

    if (CACHE && fs::exists(png2c) && fs::exists(obj2c)) {
        log("Tools already built: " + png2c.string() + ", " + obj2c.string());
        return;
    }

    Cmd png2c_cmd({CC, COMMON_CFLAGS, "-o", png2c.string(), TOOLS_SRC_DIR / "png2c.c", "-lm"});
    Cmd obj2c_cmd({CC, COMMON_CFLAGS, "-o", obj2c.string(), TOOLS_SRC_DIR / "obj2c.c", "-lm"});

    // Compile in parallel
    CmdRunner({png2c_cmd, obj2c_cmd}).run();

    if (!fs::exists(png2c)) panic("Failed to build `png2c` tool.");
    if (!fs::exists(obj2c)) panic("Failed to build `obj2c` tool.");
}

void build_assets() {
    dir(ASSETS_DIR);

    CmdRunner runner(1); // Run in sequence

    auto png2c = [] (string name) {
        path output = ASSETS_DIR / path(name).replace_extension(".c");
        path png = ASSETS_SRC_DIR / path(name).replace_extension(".png");
        return Cmd({TOOLS_DIR / "png2c", "-n", name, "-o", output, png});
    };

    auto obj2c = [] (string scale, path obj, string output) {
        return Cmd({TOOLS_DIR / "obj2c", "-s", scale, "-o", ASSETS_DIR / output, ASSETS_SRC_DIR / obj});
    };

    runner.push(png2c("tsodinPog"));
    runner.push(png2c("tsodinCup"));
    runner.push(png2c("oldstone"));
    runner.push(png2c("lavastone"));

    runner.push(obj2c("1",    "tsodinCupLowPoly.obj",  "tsodinCupLowPoly.c"));
    runner.push(obj2c("0.4",  "utahTeapot.obj",        "utahTeapot.c"      ));
    runner.push(obj2c("1.5",  "penger_obj/penger.obj", "penger.c"          ));

    runner.run();
}

Cmd build_tests() {
    return Cmd({CC, COMMON_CFLAGS, "-o", TEST_BIN, REPO_DIR / "test.c", "-lm", "-fsanitize=memory"});
}

Cmd build_wasm_demo(string name) {
    return Cmd({
        CC, COMMON_CFLAGS,
        "-O2", "-fno-builtin", "--target=wasm32",
        "--no-standard-libraries",
        "-Wl,--no-entry", "-Wl,--export=vc_render",
        "-Wl,--export=__heap_base", "-Wl,--allow-undefined",
        "-o", DEMOS_DIR / (name + ".wasm"),
        "-DVC_PLATFORM=VC_WASM_PLATFORM",
        DEMOS_SRC_DIR / (name + ".c")
    });
}

Cmd build_term_demo(string name) {
    return Cmd({
        CC, COMMON_CFLAGS, "-O2",
        "-o", DEMOS_DIR / (name + ".term"),
        "-DVC_PLATFORM=VC_TERM_PLATFORM", "-D_XOPEN_SOURCE=600",
        DEMOS_SRC_DIR / (name + ".c"),
        "-lm",
    });
}

Cmd build_sdl_demo(string name) {
    return Cmd({
        CC, COMMON_CFLAGS, "-O2",
        "-o", DEMOS_DIR / (name + ".sdl"),
        "-DVC_PLATFORM=VC_SDL_PLATFORM",
        DEMOS_SRC_DIR / (name + ".c"),
        "-lm", "-lSDL2"
    });
}

void build_vc_demo(CmdRunner& runner, string name) {
    runner.push(build_wasm_demo(name));
    runner.push(build_term_demo(name));
    runner.push(build_sdl_demo(name));
}

void build_all_vc_demos() {
    dir(DEMOS_DIR);

    CmdRunner runner;

    const vector<string> names = {
        "triangle",
        "dots3d",
        "squish",
        "triangle3d",
        "triangleTex",
        "triangle3dTex",
        "cup3d",
        "teapot3d",
        "penger3d",
    };

    size_t thread_count = 6;

    for (string name : names) {
        build_vc_demo(runner, name);
    }

    runner.run();

    for (string name : names) {
        path src_path = DEMOS_DIR / (name + ".wasm");
        path dst_path = WASM_DIR  / (name + ".wasm");
        fs::copy(src_path, dst_path);
    }
}

int main(int argc, char* argv[]) {
    GO_REBUILD_YOURSELF(argc, argv);

    build_tools();
    build_assets();
    // build_tests();
    // build_all_vc_demos();

    return 0;
}
