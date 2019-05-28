all: sample_grader

sample_grader:autograder_main.c threads tls 
	g++ -g autograder_main.c threads.o tls.o -o sample_grader

test1:test1.cpp threads tls
	g++ -g test1.cpp threads.o tls.o -o test1

test3:test3.cpp threads tls
	g++ -g test3.cpp threads.o tls.o -o test3

threads:threads.cpp
	g++ -g -c threads.cpp -o threads.o

tls:tls.cpp
	g++ -g -c tls.cpp -o tls.o

clean:
	rm threads.o
	rm tls.o
	rm sample_grader

clean_test1:
	rm threads.o
	rm tls.o
	rm test1

clean_test3:
	rm threads.o
	rm tls.o
	rm test3

