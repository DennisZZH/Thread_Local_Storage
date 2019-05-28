all: sample_grader

sample_grader:autograder_main.c library 
	g++ autograder_main.c library.o -g -o sample_grader

library:threads.cpp tls.cpp
	g++ -g -c threads.cpp tls.cpp -o library.o

clean:
	rm library.o
	rm sample_grader
