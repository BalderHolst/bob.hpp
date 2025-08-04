#!/usr/bin/env python3

# ============================================================================== #
# MIT License                                                                    #
#                                                                                #
# Copyright (c) 2023 Balder W. Holst                                             #
#                                                                                #
# Permission is hereby granted, free of charge, to any person obtaining a copy   #
# of this software and associated documentation files (the "Software"), to deal  #
# in the Software without restriction, including without limitation the rights   #
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell      #
# copies of the Software, and to permit persons to whom the Software is          #
# furnished to do so, subject to the following conditions:                       #
#                                                                                #
# The above copyright notice and this permission notice shall be included in all #
# copies or substantial portions of the Software.                                #
#                                                                                #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,       #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE    #
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER         #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  #
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE  #
# SOFTWARE.                                                                      #
# ============================================================================== #

from dataclasses import dataclass
import os
import sys
import subprocess
from enum import Enum

# Default values for syntax
PREFIX = "!"
L_EXE = "("
R_EXE = ")"
L_SCRIPT = "{"
R_SCRIPT = "}"

"""Temporary directory for storing scripts."""
TMP_DIR = "/tmp"

def usage():
    """Print usage information."""
    print(f"usage: python3 {sys.argv[0]} [OPTIONS] <template-file>");
    print("")
    print("Options:")
    print("  --help, -h             Display this help message.")
    print("  --prefix <char>        Set the prefix character. Default is '!'")
    print("  --exe-parens <str>     Set the executable parentheses. Default is '()'")
    print("  --script-parens <str>  Set the script parentheses. Default is '{}'")

def error(error):
    """Print an error message and exit."""
    print(error + "\n", file=sys.stderr)
    usage()
    exit(1)

class Color(Enum):
    """Colors for printing to stderr."""
    RED = 31
    GREEN = 32
    YELLOW = 33
    BLUE = 34
    MAGENTA = 35
    CYAN = 36

def eprint(s, color: None | Color = None, **kwargs):
    """Print to stderr. Maybe with a color."""
    if color is not None: print(f"\033[{color.value}m", end="", file=sys.stderr)
    print(s, file=sys.stderr, **kwargs)
    if color is not None: print("\033[0m", end="", file=sys.stderr)

def put(s):
    """Print to stdout without newline."""
    print(s, end="");

@dataclass
class Run:
    """A run of a shell command or script."""
    shell: str
    exit_code: int
    stdout: str
    stderr: str
    line: int
    col: int
    tmp_file: str = None

@dataclass
class Mark:
    """A location in a source file to be used later."""
    index: int
    line: int
    col: int

def strip_common_whitespace(s: str):
    """Strip common white space from the beginning of each line."""

    stripPrefix = True
    prefix = None
    for line in s.split("\n"):
        if line.strip() == "": continue
        if prefix is None:
            prefix = line[:len(line) - len(line.lstrip())]
            continue
        if not line.startswith(prefix):
            stripPrefix = False
            break
    if stripPrefix:
        s = "\n".join([line[len(prefix):] for line in s.split("\n")])
    return s

class EvaluatorState(Enum):
    """The state of the runner."""
    DEFAULT       = 0,
    FOUND_START   = 1,
    IN_SHELL      = 2,
    IN_EXE        = 3,
    IN_SCRIPT     = 4,

class Evaluator:
    """Class for evaluating a txtx file."""

    def __init__(self, path):
        self.cursor = 0
        self.line = 1
        self.col = 0
        self.path = path
        self.start = None
        self.cmd_start = None
        self.exe_start = None
        self.exe = None
        self.state = EvaluatorState.DEFAULT
        self.curly_count = 0
        self.runs = []
        if not os.path.isfile(path):
            error(f"'{path}' is not a file.")
        with open(path) as f:
            self.contents = f.read()

    def evaluate_cmd(self):
        """Evaluate a parsed shell command."""

        mark = self.cmd_start;
        cmd = self.contents[mark.index+len(L_SCRIPT):self.cursor]
        sys.stdout.flush()
        proc = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        self.runs.append(Run(cmd, proc.returncode, proc.stdout, proc.stderr, mark.line, mark.col, None))
        put(proc.stdout.rstrip())

    def evaluate_script(self):
        """Evaluate a parsed script."""

        exe = self.exe
        script = self.contents[self.cmd_start.index+len(L_SCRIPT):self.cursor].rstrip()
        mark = self.start

        script = strip_common_whitespace(script)

        # Write script to a temporary file
        dir = f"{TMP_DIR}/txtx"
        os.makedirs(dir, exist_ok=True)
        script_path = f"{dir}/{exe}-{mark.line}"
        with open(script_path, "w") as f:
            f.write(script)
        proc = subprocess.run([exe, script_path], capture_output=True, text=True)
        self.runs.append(Run(f"{exe} {script_path}",
            proc.returncode, proc.stdout, proc.stderr, mark.line, mark.col, script_path))
        put(proc.stdout.rstrip())

    def mark(self) -> Mark:
        """Create a mark at the current cursor position."""
        return Mark(self.cursor, self.line, self.col)

    def get(self):
        """Get the current character under cursor."""
        return self.contents[self.cursor]

    def evaluate(self):
        """Run the evaluation loop."""

        while self.cursor < len(self.contents):
            c = self.get()

            self.col += 1
            if c == "\n":
                self.line += 1
                self.col = 0

            if self.state == EvaluatorState.DEFAULT:
                if c == PREFIX:
                    self.state = EvaluatorState.FOUND_START
                    self.start = self.mark()
                else:
                    put(c)

            elif self.state == EvaluatorState.FOUND_START:
                if c == L_SCRIPT:
                    self.state = EvaluatorState.IN_SHELL
                    self.cmd_start = self.mark()
                    self.curly_count = 1
                elif c == L_EXE:
                    self.state = EvaluatorState.IN_EXE
                    self.exe_start = self.mark()
                # If we find double prefix, we the command is escaped
                elif c == PREFIX:
                    self.state = EvaluatorState.DEFAULT
                    put(c)
                    self.start = None
                else:
                    self.state = EvaluatorState.DEFAULT
                    put(self.contents[self.start.index:self.cursor+1])
                    self.start = None

            elif self.state == EvaluatorState.IN_SHELL:
                if c == L_SCRIPT: self.curly_count += 1
                if c == R_SCRIPT: self.curly_count -= 1
                if self.curly_count == 0:
                    self.evaluate_cmd()
                    self.start = None
                    self.state = EvaluatorState.DEFAULT

            elif self.state == EvaluatorState.IN_EXE:
                # Found the end of the executable name
                if c == R_EXE:
                    self.exe = self.contents[self.exe_start.index+len(L_EXE):self.cursor]

                    self.cursor += 1

                    if self.cursor >= len(self.contents):
                        error(f"{self.path}:{self.line} Unexpected end of file.")

                    if self.get() != L_SCRIPT:
                        error(f"{self.path}:{self.line} Expected '{L_SCRIPT}' after executable name.")

                    self.curly_count = 1
                    self.state = EvaluatorState.IN_SCRIPT

                    self.cmd_start = self.mark()

                elif c.isspace():
                    error(f"{self.path}:{self.line} Unexpected space in executable name.")

            elif self.state == EvaluatorState.IN_SCRIPT:
                if c == L_SCRIPT:  self.curly_count += 1
                if c == R_SCRIPT: self.curly_count -= 1

                if self.curly_count == 0:
                    self.state = EvaluatorState.DEFAULT
                    self.evaluate_script()

            # Increment cursor
            self.cursor += 1


    def check_errors(self):
        """Check for errors in the completed runs, reporting them if found"""

        errored = False
        printed = False

        for run in self.runs:
            if run.exit_code == 0 and run.stderr.strip() != "":
                if not printed:
                    print("");
                    printed = True
                eprint(f"{self.path}:{run.line}:{run.col} [{run.shell}] produced output on stderr:")
                eprint(run.stderr.rstrip())

        for run in self.runs:
            if run.exit_code != 0:
                if not printed:
                    print("");
                    printed = True
                errored = True
                eprint(f"{self.path}:{run.line}:{run.col} [{run.shell}] failed with exit code {run.exit_code}:",
                       color=Color.RED)
                eprint(run.stderr.rstrip(), color=Color.RED)

        if errored: exit(1)


def main():
    global L_EXE, R_EXE, L_SCRIPT, R_SCRIPT, PREFIX

    [_, *args] = sys.argv

    path = None

    while len(args) > 0:
        arg = args.pop(0)
        match arg:
            case "-h" | "--help":
                usage()
                exit(0)
            case "--prefix":
                if len(args) == 0: error("Expected a character for --prefix.")
                PREFIX = args.pop(0)
                if len(PREFIX) != 1: error(f"Expected one character for --prefix. Got '{PREFIX}'.")
            case "--exe-parens":
                if len(args) == 0: error("Expected a two character string for --exe-parens.")
                parens = args.pop(0)
                if len(parens) != 2: error(f"Expected two characters for --exe-parens. Got '{parens}'.")
                L_EXE = parens[0]
                R_EXE = parens[1]
            case "--script-parens":
                if len(args) == 0: error("Expected a two character string for --script-parens.")
                parens = args.pop(0)
                if len(parens) != 2: error(f"Expected two characters for --script-parens. Got '{parens}'.")
                L_SCRIPT = parens[0]
                R_SCRIPT = parens[1]
            case _:
                if path is not None: error(f"Only one file can be provided. Got '{path}' and '{arg}'.")
                path = arg

    if path is None: error("Please provide a file.")

    runner = Evaluator(path)
    runner.evaluate()
    runner.check_errors()

if __name__ == "__main__":
    main()
