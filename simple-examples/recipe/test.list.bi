:i count 2
:b shell 4
tree
:i returncode 0
:b stdout 185
.
├── bob
├── bob.cpp
├── Makefile
├── src
│   ├── main.c
│   └── other.c
├── test.list
└── test.list.bi

2 directories, 7 files

:b stderr 0

:b shell 5
./bob
:i returncode 0
:b stdout 200
Creating directory: ./build
CMD: gcc -c ./src/main.c -o ./build/main.o -Wall -Wextra -O2
CMD: gcc -c ./src/other.c -o ./build/other.o -Wall -Wextra -O2
CMD: gcc -o main ./build/main.o ./build/other.o

:b stderr 0

