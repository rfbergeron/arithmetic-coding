GPPWARN     = -Wall -Wextra -Werror -Wpedantic -Wshadow -Wold-style-cast
GPPOPTS     = ${GPPWARN}
COMPILECPP  = g++ -ggdb -std=c++17
MAKEDEPCPP  = g++ -std=c++17 -MM ${GPPOPTS}

EXECBIN     = arcode
ALLMODS     = ${MODULES} ${EXECBIN}
SOURCELIST  = ${foreach MOD, ${ALLMODS}, ${MOD}.h ${MOD}.tcc ${MOD}.cpp}
ALLSOURCE   = ${wildcard ${SOURCELIST}} ${MKFILE}
CPPSOURCE   = ${wildcard ${MODULES:=.cpp}}
SOURCES     = main.cpp debug.cpp
HEADERS     = debug.h arthcoder.h
TEMPLATES   = arthcoder.tcc
OBJECTS     = main.o debug.o
CLEANOBJS   = ${OBJLIBS} ${EXECBIN}

all: ${EXECBIN}

${EXECBIN} : ${OBJECTS}
	${COMPILECPP} -o $@ ${OBJECTS}

gdb : ${OBJECTS}
	${COMPILECPP} -DNDEBUG -o ${EXECBIN} ${OBJECTS}

%.o: %.cpp ${HEADERS}
	${COMPILECPP} -c $< ${HEADERS}

clean:
	- rm ${OBJECTS} ${EXECBIN}
	${GMAKE} rmtest

rmtest:
	- ./cleardata.sh

again: ${SOURCES} ${HEADERS}
	${GMAKE} clean all

test:
	- ./arcode encode entropy entropy.yeet
	- ./arcode encode 0p3r4t0r 0p3r4t0r.yeet
	- ./arcode decode entropy.yeet entropy.decode
	- ./arcode decode 0p3r4t0r.yeet 0p3r4t0r.decode

format:
	- clang-format -i --style=Google ${SOURCES} ${HEADERS}
