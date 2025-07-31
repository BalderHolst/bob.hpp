:i count 2
:b shell 4
tree
:i returncode 0
:b stdout 268
.
├── bob
├── bob.cpp
├── build
│   ├── main.o
│   └── other.o
├── main
├── Makefile
├── src
│   ├── main.c
│   └── other.c
├── test.list
└── test.list.bi

3 directories, 10 files

:b stderr 0

:b shell 5
./bob
:i returncode 0
:b stdout 0

:b stderr 0

