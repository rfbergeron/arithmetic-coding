GPPWARN     = -Wall -Wextra -Werror -Wpedantic -Wshadow -Wold-style-cast
GPPOPTS     = ${GPPWARN}
COMPILECPP  = g++ -std=c++17
MAKEDEPCPP  = g++ -std=c++17 -MM ${GPPOPTS}

MODULES     = debug
EXECBIN     = arcode
ALLMODS     = ${MODULES} ${EXECBIN}
SOURCELIST  = ${foreach MOD, ${ALLMODS}, ${MOD}.h ${MOD}.tcc ${MOD}.cpp}
ALLSOURCE   = ${wildcard ${SOURCELIST}} ${MKFILE}
CPPSOURCE   = ${wildcard ${MODULES:=.cpp}}
OBJECTS     = arthcoding.cpp debug.h debug.cpp
EXECOBJS    = arthcoding.o ${OBJLIBS}
CLEANOBJS   = ${OBJLIBS} ${EXECBIN}

all: ${EXECBIN}

${EXECBIN} : ${OBJECTS}
		${COMPILECPP} -o $@ ${OBJECTS}

%.o: %.cpp
	${COMPILECPP} -c $<

clean:
	- rm ${CLEANOBJS}
	${GMAKE} rmtest

rmtest:
	- ./cleardata.sh

again: ${ALLSOURCE}
	${GMAKE} clean all

test:
	- ./arcode encode entropy entropy.yeet
	- ./arcode encode 0p3r4t0r 0p3r4t0r.yeet
	- ./arcode decode entropy.yeet entropy.decode
	- ./arcode decode 0p3r4t0r.yeet 0p3r4t0r.decode

format:
	- clang-format -i --style=Google ${OBJECTS}
