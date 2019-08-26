.SUFFIXES:

SRC_LIB := yyutils.c incyyutils.c dbyyutils.c canon.c stirtrap.c jsonyyutils.c syncbuf.c
SRC := $(SRC_LIB) stir.c inc.c stirmake.c jsonyytest.c

LEX_LIB := stiryy.l incyy.l dbyy.l jsonyy.l
LEX := $(LEX_LIB)

YACC_LIB := stiryy.y incyy.y dbyy.y jsonyy.y
YACC := $(YACC_LIB)

LEXGEN_LIB := $(patsubst %.l,%.lex.c,$(LEX_LIB))
LEXGEN := $(patsubst %.l,%.lex.c,$(LEX))

YACCGEN_LIB := $(patsubst %.y,%.tab.c,$(YACC_LIB))
YACCGEN := $(patsubst %.y,%.tab.c,$(YACC))

GEN_LIB := $(patsubst %.l,%.lex.c,$(LEX_LIB)) $(patsubst %.y,%.tab.c,$(YACC_LIB))
GEN := $(patsubst %.l,%.lex.c,$(LEX)) $(patsubst %.y,%.tab.c,$(YACC))

OBJ_LIB := $(patsubst %.c,%.o,$(SRC_LIB))
OBJ := $(patsubst %.c,%.o,$(SRC))

OBJGEN_LIB := $(patsubst %.c,%.o,$(GEN_LIB))
OBJGEN := $(patsubst %.c,%.o,$(GEN))

ASM_LIB := $(patsubst %.c,%.s,$(SRC_LIB))
ASM := $(patsubst %.c,%.s,$(SRC))

ASMGEN_LIB := $(patsubst %.c,%.s,$(GEN_LIB))
ASMGEN := $(patsubst %.c,%.s,$(GEN))

DEP_LIB := $(patsubst %.c,%.d,$(SRC_LIB))
DEP := $(patsubst %.c,%.d,$(SRC))

DEPGEN_LIB := $(patsubst %.c,%.d,$(GEN_LIB))
DEPGEN := $(patsubst %.c,%.d,$(GEN))

.PHONY: all wc

all: jsonyytest stir inc abce stirmake smka smkt smkp

.PHONY: abce

abce:
	cd abce && $(MAKE)

wc:
	wc -l $(LEX) $(YACC) $(SRC) $(filter-out %.lex.h %.tab.h,$(wildcard *.h))

#LUAINC:=/usr/include/lua5.3
#LUALIB:=/usr/lib/x86_64-linux-gnu/liblua5.3.a
LUAINC:=/usr/include/luajit-2.1
LUALIB:=/usr/lib/x86_64-linux-gnu/libluajit-5.1.a

#CC=gcc
CC=clang
CFLAGS=-O3 -Wall -g -I$(LUAINC)

smka:
	rm -f smka
	ln -s stirmake smka
smkt:
	rm -f smkt
	ln -s stirmake smkt
smkp:
	rm -f smkp
	ln -s stirmake smkp

stirmake: stirmake.o libstirmake.a Makefile $(LUALIB) abce
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) abce/libabce.a -lm -ldl

stir: stir.o libstirmake.a Makefile $(LUALIB) abce
	$(CC) $(CCFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) abce/libabce.a -lm -ldl

jsonyytest: jsonyytest.o libstirmake.a Makefile $(LUALIB) abce
	$(CC) $(CCFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) abce/libabce.a -lm -ldl

inc: inc.o libstirmake.a Makefile $(LUALIB)
	$(CC) $(CCFLAGS) -o $@ $(filter %.o,$^) $(filter %.a,$^) -lm -ldl

libstirmake.a: $(OBJ_LIB) $(OBJGEN_LIB) Makefile
	rm -f $@
	ar rvs $@ $(filter %.o,$^)

$(OBJ): %.o: %.c %.d Makefile
	$(CC) $(CFLAGS) -c -o $*.o $*.c
	$(CC) $(CFLAGS) -c -S -o $*.s $*.c
$(OBJGEN): %.o: %.c %.h %.d Makefile
	$(CC) $(CFLAGS) -c -o $*.o $*.c -Wno-sign-compare -Wno-missing-prototypes
	$(CC) $(CFLAGS) -c -S -o $*.s $*.c -Wno-sign-compare -Wno-missing-prototypes

$(DEP): %.d: %.c Makefile
	$(CC) $(CFLAGS) -MM -MP -MT "$*.d $*.o" -o $*.d $*.c
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

dbyy.lex.d: dbyy.tab.h dbyy.lex.h
dbyy.lex.o: dbyy.tab.h dbyy.lex.h
dbyy.tab.d: dbyy.tab.h dbyy.lex.h
dbyy.tab.o: dbyy.tab.h dbyy.lex.h

dbyy.lex.c: dbyy.l Makefile
	flex --outfile=dbyy.lex.c --header-file=/dev/null dbyy.l
dbyy.lex.h: dbyy.l Makefile
	flex --outfile=/dev/null --header-file=dbyy.lex.h dbyy.l
dbyy.tab.c: dbyy.y Makefile
	byacc -d -p dbyy -b dbyy -o .tmpc.dbyy.tab.c dbyy.y
	rm .tmpc.dbyy.tab.h
	mv .tmpc.dbyy.tab.c dbyy.tab.c
dbyy.tab.h: dbyy.y Makefile
	byacc -d -p dbyy -b dbyy -o .tmph.dbyy.tab.c dbyy.y
	rm .tmph.dbyy.tab.c
	mv .tmph.dbyy.tab.h dbyy.tab.h

jsonyy.lex.d: jsonyy.tab.h jsonyy.lex.h
jsonyy.lex.o: jsonyy.tab.h jsonyy.lex.h
jsonyy.tab.d: jsonyy.tab.h jsonyy.lex.h
jsonyy.tab.o: jsonyy.tab.h jsonyy.lex.h

jsonyy.lex.c: jsonyy.l Makefile
	flex --outfile=jsonyy.lex.c --header-file=/dev/null jsonyy.l
jsonyy.lex.h: jsonyy.l Makefile
	flex --outfile=/dev/null --header-file=jsonyy.lex.h jsonyy.l
jsonyy.tab.c: jsonyy.y Makefile
	byacc -d -p jsonyy -b jsonyy -o .tmpc.jsonyy.tab.c jsonyy.y
	rm .tmpc.jsonyy.tab.h
	mv .tmpc.jsonyy.tab.c jsonyy.tab.c
jsonyy.tab.h: jsonyy.y Makefile
	byacc -d -p jsonyy -b jsonyy -o .tmph.jsonyy.tab.c jsonyy.y
	rm .tmph.jsonyy.tab.c
	mv .tmph.jsonyy.tab.h jsonyy.tab.h

-include *.d
