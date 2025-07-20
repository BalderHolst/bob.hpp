:i count 1
:b shell 5
./bob
:i returncode 0
:b stdout 2963
CMD: python3 -c import time;print('Job 0 started...'); time.sleep(0.000000); print('Job + 0 finished!')
CMD: python3 -c import time;print('Job 1 started...'); time.sleep(0.050000); print('Job + 1 finished!')
CMD: python3 -c import time;print('Job 2 started...'); time.sleep(0.100000); print('Job + 2 finished!')
Job 0 started...
Job + 0 finished!
CMD: python3 -c import time;print('Job 3 started...'); time.sleep(0.150000); print('Job + 3 finished!')
Job 1 started...
Job + 1 finished!
CMD: python3 -c import time;print('Job 4 started...'); time.sleep(0.200000); print('Job + 4 finished!')
Job 2 started...
Job + 2 finished!
CMD: python3 -c import time;print('Job 5 started...'); time.sleep(0.250000); print('Job + 5 finished!')
Job 3 started...
Job + 3 finished!
CMD: python3 -c import time;print('Job 6 started...'); time.sleep(0.300000); print('Job + 6 finished!')
Job 4 started...
Job + 4 finished!
CMD: python3 -c import time;print('Job 7 started...'); time.sleep(0.350000); print('Job + 7 finished!')
Job 5 started...
Job + 5 finished!
CMD: python3 -c import time;print('Job 8 started...'); time.sleep(0.400000); print('Job + 8 finished!')
Job 6 started...
Job + 6 finished!
CMD: python3 -c import time;print('Job 9 started...'); time.sleep(0.450000); print('Job + 9 finished!')
Job 7 started...
Job + 7 finished!
CMD: python3 -c import time;print('Job 10 started...'); time.sleep(0.500000); print('Job + 10 finished!')
Job 8 started...
Job + 8 finished!
CMD: python3 -c import time;print('Job 11 started...'); time.sleep(0.550000); print('Job + 11 finished!')
Job 9 started...
Job + 9 finished!
CMD: python3 -c import time;print('Job 12 started...'); time.sleep(0.600000); print('Job + 12 finished!')
Job 10 started...
Job + 10 finished!
CMD: python3 -c import time;print('Job 13 started...'); time.sleep(0.650000); print('Job + 13 finished!')
Job 11 started...
Job + 11 finished!
CMD: python3 -c import time;print('Job 14 started...'); time.sleep(0.700000); print('Job + 14 finished!')
Job 12 started...
Job + 12 finished!
CMD: python3 -c import time;print('Job 15 started...'); time.sleep(0.750000); print('Job + 15 finished!')
Job 13 started...
Job + 13 finished!
CMD: python3 -c import time;print('Job 16 started...'); time.sleep(0.800000); print('Job + 16 finished!')
Job 14 started...
Job + 14 finished!
CMD: python3 -c import time;print('Job 17 started...'); time.sleep(0.850000); print('Job + 17 finished!')
Job 15 started...
Job + 15 finished!
CMD: python3 -c import time;print('Job 18 started...'); time.sleep(0.900000); print('Job + 18 finished!')
Job 16 started...
Job + 16 finished!
CMD: python3 -c import time;print('Job 19 started...'); time.sleep(0.950000); print('Job + 19 finished!')
Job 17 started...
Job + 17 finished!
CMD: python3 -c import time;print('Job 20 started...'); time.sleep(1.000000); print('Job + 20 finished!')
Job 18 started...
Job + 18 finished!
Job 19 started...
Job + 19 finished!
Job 20 started...
Job + 20 finished!

:b stderr 0

