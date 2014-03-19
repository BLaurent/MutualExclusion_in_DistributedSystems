all:node

node:node.o
	g++ node.o -o node -lm -lpthread

node.o:node.cpp
	g++ -c node.cpp node.h

clean:
	rm -f *.o *.h.gch node debug_* cs_*
	touch *
