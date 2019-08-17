.PHONY: all

#LUAINC:=/usr/include/lua5.3
#LUALIB:=/usr/lib/x86_64-linux-gnu/liblua5.3.a
LUAINC:=/usr/include/luajit-2.1
LUALIB:=/usr/lib/x86_64-linux-gnu/libluajit-5.1.a

CPP:=g++
CPPFLAGS:=-Wall -Wextra -O3 -I$(LUAINC) -g

all: cctest arraytest luatest

cctest: cctest.o stirbce.o $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $^ -lm -ldl

arraytest: arraytest.o stirbce.o $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $^ -lm -ldl

luatest: luatest.o stirbce.o $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $^ -lm -ldl

stirbce.o: opcodes.h
