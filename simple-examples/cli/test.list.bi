:i count 26
:b shell 5
./bob
:i returncode 1
:b stdout 404
No command provided.

Bob CLI Example

Available commands:
    hello       Prints a hello message
    submenu     A submenu of commands
    path        Prints the path of this command
    args        Prints the arguments passed to the CLI
    flags       Prints prints its flag arguments and their values

Arguments:
    -h, --help      Prints this help message
    -v, --verbose   Enable verbose output

:b stderr 0

:b shell 12
./bob --help
:i returncode 1
:b stdout 404
No command provided.

Bob CLI Example

Available commands:
    hello       Prints a hello message
    submenu     A submenu of commands
    path        Prints the path of this command
    args        Prints the arguments passed to the CLI
    flags       Prints prints its flag arguments and their values

Arguments:
    -h, --help      Prints this help message
    -v, --verbose   Enable verbose output

:b stderr 0

:b shell 8
./bob -h
:i returncode 1
:b stdout 404
No command provided.

Bob CLI Example

Available commands:
    hello       Prints a hello message
    submenu     A submenu of commands
    path        Prints the path of this command
    args        Prints the arguments passed to the CLI
    flags       Prints prints its flag arguments and their values

Arguments:
    -h, --help      Prints this help message
    -v, --verbose   Enable verbose output

:b stderr 0

:b shell 11
./bob bogos
:i returncode 1
:b stdout 382
Bob CLI Example

Available commands:
    hello       Prints a hello message
    submenu     A submenu of commands
    path        Prints the path of this command
    args        Prints the arguments passed to the CLI
    flags       Prints prints its flag arguments and their values

Arguments:
    -h, --help      Prints this help message
    -v, --verbose   Enable verbose output

:b stderr 32
[ERROR] Unknown command: bogos


:b shell 11
./bob hello
:i returncode 1
:b stdout 23
Hello, my name is Bob!

:b stderr 0

:b shell 18
./bob --error-flag
:i returncode 1
:b stdout 382
Bob CLI Example

Available commands:
    hello       Prints a hello message
    submenu     A submenu of commands
    path        Prints the path of this command
    args        Prints the arguments passed to the CLI
    flags       Prints prints its flag arguments and their values

Arguments:
    -h, --help      Prints this help message
    -v, --verbose   Enable verbose output

:b stderr 40
[ERROR] Unknown argument: --error-flag


:b shell 8
./bob -e
:i returncode 1
:b stdout 382
Bob CLI Example

Available commands:
    hello       Prints a hello message
    submenu     A submenu of commands
    path        Prints the path of this command
    args        Prints the arguments passed to the CLI
    flags       Prints prints its flag arguments and their values

Arguments:
    -h, --help      Prints this help message
    -v, --verbose   Enable verbose output

:b stderr 30
[ERROR] Unknown argument: -e


:b shell 12
./bob -error
:i returncode 1
:b stdout 382
Bob CLI Example

Available commands:
    hello       Prints a hello message
    submenu     A submenu of commands
    path        Prints the path of this command
    args        Prints the arguments passed to the CLI
    flags       Prints prints its flag arguments and their values

Arguments:
    -h, --help      Prints this help message
    -v, --verbose   Enable verbose output

:b stderr 34
[ERROR] Unknown argument: -error


:b shell 13
./bob submenu
:i returncode 1
:b stdout 260
No command provided.

A submenu of commands

Available commands:
    subcommand1     A subcommand in the submenu
    subcommand2     A subcommand in the submenu

Arguments:
    -v, --verbose   Enable verbose output
    -h, --help      Prints this help message

:b stderr 0

:b shell 25
./bob submenu subcommand1
:i returncode 0
:b stdout 31
This is the FIRST subcommand!!

:b stderr 0

:b shell 25
./bob submenu subcommand2
:i returncode 0
:b stdout 32
This is the SECOND subcommand!!

:b stderr 0

:b shell 26
./bob submenu doesnotexist
:i returncode 1
:b stdout 238
A submenu of commands

Available commands:
    subcommand1     A subcommand in the submenu
    subcommand2     A subcommand in the submenu

Arguments:
    -v, --verbose   Enable verbose output
    -h, --help      Prints this help message

:b stderr 39
[ERROR] Unknown command: doesnotexist


:b shell 16
./bob submenu -h
:i returncode 1
:b stdout 260
No command provided.

A submenu of commands

Available commands:
    subcommand1     A subcommand in the submenu
    subcommand2     A subcommand in the submenu

Arguments:
    -v, --verbose   Enable verbose output
    -h, --help      Prints this help message

:b stderr 0

:b shell 10
./bob path
:i returncode 0
:b stdout 18
Path: ./bob path 

:b stderr 0

:b shell 13
./bob path -h
:i returncode 0
:b stdout 131
Prints the path of this command

Arguments:
    -v, --verbose   Enable verbose output
    -h, --help      Prints this help message

:b stderr 0

:b shell 24
./bob args one two three
:i returncode 0
:b stdout 64
Arguments:
    argv[0]: one
    argv[1]: two
    argv[2]: three

:b stderr 0

:b shell 27
./bob args one two three -h
:i returncode 0
:b stdout 138
Prints the arguments passed to the CLI

Arguments:
    -v, --verbose   Enable verbose output
    -h, --help      Prints this help message

:b stderr 0

:b shell 37
./bob args one two three --unexpected
:i returncode 1
:b stdout 138
Prints the arguments passed to the CLI

Arguments:
    -v, --verbose   Enable verbose output
    -h, --help      Prints this help message

:b stderr 40
[ERROR] Unknown argument: --unexpected


:b shell 25
./bob flags --an-argument
:i returncode 1
:b stdout 320
Prints prints its flag arguments and their values

Arguments:
    -a, --an-argument <an-argument>  An argument with a value
    -f, --flag                       A simple flag argument
    -v, --better-v                   A better -v flag than the global one
    -h, --help                       Prints this help message

:b stderr 52
[ERROR] Expected value for argument: --an-argument


:b shell 31
./bob flags --an-argument value
:i returncode 0
:b stdout 287
    Argument: an-argument (short: a), Type: Option, Value: value, Set: true
    Argument: flag (short: f), Type: Flag, Value: <none>, Set: false
    Argument: better-v (short: v), Type: Flag, Value: <none>, Set: false
    Argument: help (short: h), Type: Flag, Value: <none>, Set: false

:b stderr 0

:b shell 21
./bob flags --flag -f
:i returncode 0
:b stdout 288
    Argument: an-argument (short: a), Type: Option, Value: <none>, Set: false
    Argument: flag (short: f), Type: Flag, Value: <none>, Set: true
    Argument: better-v (short: v), Type: Flag, Value: <none>, Set: false
    Argument: help (short: h), Type: Flag, Value: <none>, Set: false

:b stderr 0

:b shell 17
./bob flags -f -v
:i returncode 0
:b stdout 287
    Argument: an-argument (short: a), Type: Option, Value: <none>, Set: false
    Argument: flag (short: f), Type: Flag, Value: <none>, Set: true
    Argument: better-v (short: v), Type: Flag, Value: <none>, Set: true
    Argument: help (short: h), Type: Flag, Value: <none>, Set: false

:b stderr 0

:b shell 20
./bob flags -f -v -a
:i returncode 1
:b stdout 320
Prints prints its flag arguments and their values

Arguments:
    -a, --an-argument <an-argument>  An argument with a value
    -f, --flag                       A simple flag argument
    -v, --better-v                   A better -v flag than the global one
    -h, --help                       Prints this help message

:b stderr 41
[ERROR] Expected value for argument: -a


:b shell 28
./bob flags -f -v -a arg-val
:i returncode 0
:b stdout 287
    Argument: an-argument (short: a), Type: Option, Value: arg-val, Set: true
    Argument: flag (short: f), Type: Flag, Value: <none>, Set: true
    Argument: better-v (short: v), Type: Flag, Value: <none>, Set: true
    Argument: help (short: h), Type: Flag, Value: <none>, Set: false

:b stderr 0

:b shell 20
./bob flags -f -v -h
:i returncode 0
:b stdout 320
Prints prints its flag arguments and their values

Arguments:
    -a, --an-argument <an-argument>  An argument with a value
    -f, --flag                       A simple flag argument
    -v, --better-v                   A better -v flag than the global one
    -h, --help                       Prints this help message

:b stderr 0

:b shell 17
./bob flags bogus
:i returncode 0
:b stdout 289
    Argument: an-argument (short: a), Type: Option, Value: <none>, Set: false
    Argument: flag (short: f), Type: Flag, Value: <none>, Set: false
    Argument: better-v (short: v), Type: Flag, Value: <none>, Set: false
    Argument: help (short: h), Type: Flag, Value: <none>, Set: false

:b stderr 0

