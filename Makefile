CWARN ::= -Wall -Wextra -Wpedantic -Wno-shadow
CFLAGS ::= -std=c++17
EXE ::= build/arcode
SRC ::= src/main.cpp src/debug.cpp
HDR ::= src/debug.h src/arthcoder.h
TPL ::= src/arthcoder.tcc
OBJ ::= ${SRC:src/%.cpp=build/%.o}

.PHONY: all debug sanitize clean format again test ci check

all: CFLAGS += -O2 -DNDEBUG
all: build ${EXE}

sanitize: CFLAGS += -O2 -DNDEBUG -fsanitize=address,undefined
sanitize: build ${EXE}

debug: CFLAGS += -Og -p -g -fsanitize=address,undefined -fstack-protector-all
debug: build ${EXE}

build:
	mkdir -p $@

${EXE} : ${OBJ}
	${CXX} ${CFLAGS} ${LDFLAGS} ${CWARN} -o $@ $^

build/%.o: src/%.cpp ${HDR} ${TPL}
	${CXX} ${CFLAGS} ${CWARN} -o $@ -c $<

clean:
	rm -f ${OBJ} ${EXE}

again:
	${MAKE} clean all

test:
	./test.sh

clean-test:
	./clean-test.sh

format:
	clang-format -i --style=Google ${SRC} ${HDR} ${TPL}

check:
	clang-format --Werror --dry-run --style=Google ${SRC} ${HDR} ${TPL}

ci:
	${MAKE} check
	git add ${SRC} ${HDR} ${TPL} Makefile test.sh clean-test.sh .gitignore
