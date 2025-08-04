:i count 2
:b shell 5
./bob
:i returncode 0
:b stdout 60
CMD: g++ src/main.cpp src/add.cpp -o main -Wall -Wextra -O2

:b stderr 0

:b shell 6
./main
:i returncode 0
:b stdout 26
Adding 10 and 20 gives 30

:b stderr 0

