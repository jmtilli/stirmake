.PHONY: all


CPP:=g++
CPPFLAGS:=-Wall -Wextra -O3 -I/usr/include/lua5.3 -g

all: cctest arraytest luatest

cctest: cctest.o stirbce.o /usr/lib/x86_64-linux-gnu/liblua5.3.a
	$(CPP) $(CPPFLAGS) -o $@ $^ -lm -ldl

arraytest: arraytest.o stirbce.o /usr/lib/x86_64-linux-gnu/liblua5.3.a
	$(CPP) $(CPPFLAGS) -o $@ $^ -lm -ldl

luatest: luatest.o stirbce.o /usr/lib/x86_64-linux-gnu/liblua5.3.a
	$(CPP) $(CPPFLAGS) -o $@ $^ -lm -ldl

stirbce.o: opcodes.h
