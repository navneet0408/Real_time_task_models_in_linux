scheduler: ../bin/scheduler.o
	gcc -o ../bin/Scheduler ../bin/scheduler.o -lpthread

../bin/scheduler.o: scheduler.c
	gcc -pthread -c -o ../bin/scheduler.o scheduler.c

clean:
	rm -rf ../bin/*
