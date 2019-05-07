.SUFFIXES:

SRC_LIB := yyutils.c incyyutils.c
SRC := $(SRC_LIB) stir.c inc.c

SRC_CPP_LIB := stirbce.cc
SRC_CPP := $(SRC_CPP_LIB) stirmain.cc arraytest.cc luatest.cc cctest.cc streamtest.cc globtest.cc bttest.cc

LEX_LIB := stiryy.l incyy.l
LEX := $(LEX_LIB)

YACC_LIB := stiryy.y incyy.y
YACC := $(YACC_LIB)

LEXGEN_LIB := $(patsubst %.l,%.lex.c,$(LEX_LIB))
LEXGEN := $(patsubst %.l,%.lex.c,$(LEX))

YACCGEN_LIB := $(patsubst %.y,%.tab.c,$(YACC_LIB))
YACCGEN := $(patsubst %.y,%.tab.c,$(YACC))

GEN_LIB := $(patsubst %.l,%.lex.c,$(LEX_LIB)) $(patsubst %.y,%.tab.c,$(YACC_LIB))
GEN := $(patsubst %.l,%.lex.c,$(LEX)) $(patsubst %.y,%.tab.c,$(YACC))

OBJ_LIB := $(patsubst %.c,%.o,$(SRC_LIB))
OBJ := $(patsubst %.c,%.o,$(SRC))

OBJ_CPP_LIB := $(patsubst %.cc,%.o,$(SRC_CPP_LIB))
OBJ_CPP := $(patsubst %.cc,%.o,$(SRC_CPP))

OBJGEN_LIB := $(patsubst %.c,%.o,$(GEN_LIB))
OBJGEN := $(patsubst %.c,%.o,$(GEN))

ASM_LIB := $(patsubst %.c,%.s,$(SRC_LIB))
ASM := $(patsubst %.c,%.s,$(SRC))

ASMGEN_LIB := $(patsubst %.c,%.s,$(GEN_LIB))
ASMGEN := $(patsubst %.c,%.s,$(GEN))

DEP_LIB := $(patsubst %.c,%.d,$(SRC_LIB))
DEP := $(patsubst %.c,%.d,$(SRC))

DEP_CPP_LIB := $(patsubst %.cc,%.d,$(SRC_CPP_LIB))
DEP_CPP := $(patsubst %.cc,%.d,$(SRC_CPP))

DEPGEN_LIB := $(patsubst %.c,%.d,$(GEN_LIB))
DEPGEN := $(patsubst %.c,%.d,$(GEN))

.PHONY: all wc

all: stirmain stir inc arraytest luatest cctest streamtest globtest bttest

wc:
	wc -l $(LEX) $(YACC) $(SRC_CPP) $(SRC) $(filter-out %.lex.h %.tab.h,$(wildcard *.h))

#LUAINC:=/usr/include/lua5.3
#LUALIB:=/usr/lib/x86_64-linux-gnu/liblua5.3.a
LUAINC:=/usr/include/luajit-2.1
LUALIB:=/usr/lib/x86_64-linux-gnu/libluajit-5.1.a

CC=gcc
CPP=g++
CFLAGS=-O3 -Wall -g -I$(LUAINC)
CPPFLAGS=-O3 -Wall -g -I$(LUAINC)

stirmain: stirmain.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

arraytest: arraytest.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

streamtest: streamtest.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

globtest: globtest.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

bttest: bttest.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

luatest: luatest.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

cctest: cctest.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

stir: stir.o libstirmake.a Makefile $(LUALIB)
	$(CC) $(CCFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

inc: inc.o libstirmake.a Makefile $(LUALIB)
	$(CC) $(CCFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

libstirmake.a: $(OBJ_LIB) $(OBJGEN_LIB) $(OBJ_CPP_LIB) Makefile
	rm -f $@
	ar rvs $@ $(filter %.o,$^)

$(OBJ): %.o: %.c %.d Makefile
	$(CC) $(CFLAGS) -c -o $*.o $*.c
	$(CC) $(CFLAGS) -c -S -o $*.s $*.c
$(OBJ_CPP): %.o: %.cc %.d Makefile
	$(CPP) $(CPPFLAGS) -c -o $*.o $*.cc
	$(CPP) $(CPPFLAGS) -c -S -o $*.s $*.cc
$(OBJGEN): %.o: %.c %.h %.d Makefile
	$(CC) $(CFLAGS) -c -o $*.o $*.c -Wno-sign-compare -Wno-missing-prototypes
	$(CC) $(CFLAGS) -c -S -o $*.s $*.c -Wno-sign-compare -Wno-missing-prototypes

$(DEP): %.d: %.c Makefile
	$(CC) $(CFLAGS) -MM -MP -MT "$*.d $*.o" -o $*.d $*.c
$(DEP_CPP): %.d: %.cc Makefile
	$(CPP) $(CPPFLAGS) -MM -MP -MT "$*.d $*.o" -o $*.d $*.cc
$(DEPGEN): %.d: %.c %.h Makefile
	$(CC) $(CFLAGS) -MM -MP -MT "$*.d $*.o" -o $*.d $*.c

stiryy.lex.d: stiryy.tab.h stiryy.lex.h
stiryy.lex.o: stiryy.tab.h stiryy.lex.h
stiryy.tab.d: stiryy.tab.h stiryy.lex.h
stiryy.tab.o: stiryy.tab.h stiryy.lex.h

STIRYY.LEX.INTERMEDIATE: stiryy.l Makefile
	mkdir -p intermediatestore
	flex --outfile=intermediatestore/stiryy.lex.c --header-file=intermediatestore/stiryy.lex.h stiryy.l
	touch STIRYY.LEX.INTERMEDIATE
STIRYY.TAB.INTERMEDIATE: stiryy.y Makefile
	mkdir -p intermediatestore
	bison --defines=intermediatestore/stiryy.tab.h --output=intermediatestore/stiryy.tab.c stiryy.y
	touch STIRYY.TAB.INTERMEDIATE
stiryy.lex.c: STIRYY.LEX.INTERMEDIATE Makefile
	cp intermediatestore/stiryy.lex.c .
stiryy.lex.h: STIRYY.LEX.INTERMEDIATE Makefile
	cp intermediatestore/stiryy.lex.h .
stiryy.tab.c: STIRYY.TAB.INTERMEDIATE Makefile
	cp intermediatestore/stiryy.tab.c .
stiryy.tab.h: STIRYY.TAB.INTERMEDIATE Makefile
	cp intermediatestore/stiryy.tab.h .

incyy.lex.d: incyy.tab.h incyy.lex.h
incyy.lex.o: incyy.tab.h incyy.lex.h
incyy.tab.d: incyy.tab.h incyy.lex.h
incyy.tab.o: incyy.tab.h incyy.lex.h

INCYY.LEX.INTERMEDIATE: incyy.l Makefile
	mkdir -p intermediatestore
	flex --outfile=intermediatestore/incyy.lex.c --header-file=intermediatestore/incyy.lex.h incyy.l
	touch INCYY.LEX.INTERMEDIATE
INCYY.TAB.INTERMEDIATE: incyy.y Makefile
	mkdir -p intermediatestore
	bison --defines=intermediatestore/incyy.tab.h --output=intermediatestore/incyy.tab.c incyy.y
	touch INCYY.TAB.INTERMEDIATE
incyy.lex.c: INCYY.LEX.INTERMEDIATE Makefile
	cp intermediatestore/incyy.lex.c .
incyy.lex.h: INCYY.LEX.INTERMEDIATE Makefile
	cp intermediatestore/incyy.lex.h .
incyy.tab.c: INCYY.TAB.INTERMEDIATE Makefile
	cp intermediatestore/incyy.tab.c .
incyy.tab.h: INCYY.TAB.INTERMEDIATE Makefile
	cp intermediatestore/incyy.tab.h .

-include *.d
