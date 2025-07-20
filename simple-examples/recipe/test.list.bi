:i count 1
:b shell 5
./bob
:i returncode 0
:b stdout 172
CMD: gcc -c ./src/main.c -o ./build/main.o -Wall -Wextra -O2
CMD: gcc -c ./src/other.c -o ./build/other.o -Wall -Wextra -O2
CMD: gcc -o main ./build/main.o ./build/other.o

:b stderr 401
Could not stat source file: No such file or directory
./src/main.c: In function ‘main’:
./src/main.c:6:14: warning: unused parameter ‘argc’ [-Wunused-parameter]
    6 | int main(int argc, char * argv[]) {
      |          ~~~~^~~~
./src/main.c:6:27: warning: unused parameter ‘argv’ [-Wunused-parameter]
    6 | int main(int argc, char * argv[]) {
      |                    ~~~~~~~^~~~~~

