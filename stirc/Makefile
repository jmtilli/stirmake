.SUFFIXES:

SRC_LIB := yyutils.c incyyutils.c
SRC := $(SRC_LIB) stir.c inc.c stirmake.c

SRC_CPP_LIB := stirbce.cc
SRC_CPP := $(SRC_CPP_LIB) stirmain.cc arraytest.cc luatest.cc cctest.cc streamtest.cc globtest.cc bttest.cc jsontest.cc

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

all: stirmain stir inc arraytest luatest cctest streamtest globtest bttest jsontest abce stirmake

.PHONY: abce

abce:
	cd abce && $(MAKE)

wc:
	wc -l $(LEX) $(YACC) $(SRC_CPP) $(SRC) $(filter-out %.lex.h %.tab.h,$(wildcard *.h))

wcc:
	wc -l $(LEX) $(YACC) $(SRC) $(filter-out %.lex.h %.tab.h,$(wildcard *.h))

#LUAINC:=/usr/include/lua5.3
#LUALIB:=/usr/lib/x86_64-linux-gnu/liblua5.3.a
LUAINC:=/usr/include/luajit-2.1
LUALIB:=/usr/lib/x86_64-linux-gnu/libluajit-5.1.a

#CC=gcc
#CPP=g++
CC=clang
CPP=clang++
CFLAGS=-O3 -Wall -g -I$(LUAINC)
CPPFLAGS=-O3 -Wall -g -I$(LUAINC)

stirmain: stirmain.o libstirmake.a Makefile $(LUALIB) abce
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) abce/libabce.a -lm -ldl

stirmake: stirmake.o libstirmake.a Makefile $(LUALIB) abce
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) abce/libabce.a -lm -ldl

arraytest: arraytest.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

streamtest: streamtest.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

globtest: globtest.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

bttest: bttest.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

jsontest: jsontest.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

luatest: luatest.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

cctest: cctest.o libstirmake.a Makefile $(LUALIB)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

stir: stir.o libstirmake.a Makefile $(LUALIB) abce
	$(CC) $(CCFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) abce/libabce.a -lm -ldl

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

stiryy.lex.c: stiryy.l Makefile
	flex --outfile=stiryy.lex.c --header-file=/dev/null stiryy.l
stiryy.lex.h: stiryy.l Makefile
	flex --outfile=/dev/null --header-file=stiryy.lex.h stiryy.l
stiryy.tab.c: stiryy.y Makefile
	byacc -d -p stiryy -b stiryy -o .tmpc.stiryy.tab.c stiryy.y
	rm .tmpc.stiryy.tab.h
	mv .tmpc.stiryy.tab.c stiryy.tab.c
stiryy.tab.h: stiryy.y Makefile
	byacc -d -p stiryy -b stiryy -o .tmph.stiryy.tab.c stiryy.y
	rm .tmph.stiryy.tab.c
	mv .tmph.stiryy.tab.h stiryy.tab.h

incyy.lex.d: incyy.tab.h incyy.lex.h
incyy.lex.o: incyy.tab.h incyy.lex.h
incyy.tab.d: incyy.tab.h incyy.lex.h
incyy.tab.o: incyy.tab.h incyy.lex.h

incyy.lex.c: incyy.l Makefile
	flex --outfile=incyy.lex.c --header-file=/dev/null incyy.l
incyy.lex.h: incyy.l Makefile
	flex --outfile=/dev/null --header-file=incyy.lex.h incyy.l
incyy.tab.c: incyy.y Makefile
	byacc -d -p incyy -b incyy -o .tmpc.incyy.tab.c incyy.y
	rm .tmpc.incyy.tab.h
	mv .tmpc.incyy.tab.c incyy.tab.c
incyy.tab.h: incyy.y Makefile
	byacc -d -p incyy -b incyy -o .tmph.incyy.tab.c incyy.y
	rm .tmph.incyy.tab.c
	mv .tmph.incyy.tab.h incyy.tab.h

-include *.d
