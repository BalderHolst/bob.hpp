#!/usr/bin/env python3

import os
import sys
import subprocess

TEST_DIR="./simple-examples"

def main():
    [_, *argv] = sys.argv

    match len(argv):
        case 0:
            run("replay")
        case 1:
            run(argv[0])
        case _:
            print("Usage: test.py <record|replay>")
            print("ERROR: too many arguments provided")
            exit(1)

def run(action):
    test_dirs = [f.path for f in os.scandir(TEST_DIR) if f.is_dir()]

    # Compile binaries
    for test_dir in test_dirs:
        cmd = ["g++", f"{test_dir}/bob.cpp", "-o", f"{test_dir}/bob"]
        print(f"COMPILING: {' '.join(cmd)}")
        proc = subprocess.run(cmd)
        if proc.returncode != 0:
            print(f"ERROR: Compilation failed in {test_dir}")
            exit(proc.returncode)

    # Run tests
    for test_dir in test_dirs:
        rere_path = os.path.relpath(os.path.abspath("./rere.py"), test_dir)
        cmd = [rere_path, action, "test.list"]
        print(f"RUNNING: [{test_dir}] {' '.join(cmd)}")

        # Don't fail on non-zero return code
        proc = subprocess.run(cmd, cwd=test_dir)

        if proc.returncode != 0:
            print(f"ERROR: Test failed in {test_dir}")
            exit(proc.returncode)

if __name__ == '__main__':
    main()
