:i count 1
:b shell 5
./bob
:i returncode 0
:b stdout 200
Creating directory: ./build
CMD: gcc -c ./src/main.c -o ./build/main.o -Wall -Wextra -O2
CMD: gcc -c ./src/other.c -o ./build/other.o -Wall -Wextra -O2
CMD: gcc -o main ./build/main.o ./build/other.o

:b stderr 0

