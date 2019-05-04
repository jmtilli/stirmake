all: stir inc stirmain

CFLAGS=-O3 -Wall -g
CPPFLAGS=-O3 -Wall -g

stirmain: stirmain.o stiryy.lex.o stiryy.tab.o yyutils.o
	c++ -o stirmain stirmain.o stiryy.lex.o stiryy.tab.o yyutils.o incyyutils.o incyy.lex.o incyy.tab.o

stir: stir.o stiryy.lex.o stiryy.tab.o yyutils.o
	cc -o stir stir.o stiryy.lex.o stiryy.tab.o yyutils.o

inc: inc.o incyy.lex.o incyy.tab.o incyyutils.o
	cc -o inc inc.o incyy.lex.o incyy.tab.o incyyutils.o

stir.o: stir.c
	cc -c -o stir.o stir.c
inc.o: inc.c
	cc -c -o inc.o inc.c

stiryy.lex.d: stiryy.tab.h stiryy.lex.h
stiryy.lex.o: stiryy.tab.h stiryy.lex.h
stiryy.tab.d: stiryy.tab.h stiryy.lex.h
stiryy.tab.o: stiryy.tab.h stiryy.lex.h

STIRYY.LEX.INTERMEDIATE: stiryy.l
	mkdir -p intermediatestore
	flex --outfile=intermediatestore/stiryy.lex.c --header-file=intermediatestore/stiryy.lex.h stiryy.l
	touch STIRYY.LEX.INTERMEDIATE
STIRYY.TAB.INTERMEDIATE: stiryy.y
	mkdir -p intermediatestore
	bison --defines=intermediatestore/stiryy.tab.h --output=intermediatestore/stiryy.tab.c stiryy.y
	touch STIRYY.TAB.INTERMEDIATE
stiryy.lex.c: STIRYY.LEX.INTERMEDIATE
	cp intermediatestore/stiryy.lex.c .
stiryy.lex.h: STIRYY.LEX.INTERMEDIATE
	cp intermediatestore/stiryy.lex.h .
stiryy.tab.c: STIRYY.TAB.INTERMEDIATE
	cp intermediatestore/stiryy.tab.c .
stiryy.tab.h: STIRYY.TAB.INTERMEDIATE
	cp intermediatestore/stiryy.tab.h .

incyy.lex.d: incyy.tab.h incyy.lex.h
incyy.lex.o: incyy.tab.h incyy.lex.h
incyy.tab.d: incyy.tab.h incyy.lex.h
incyy.tab.o: incyy.tab.h incyy.lex.h

INCYY.LEX.INTERMEDIATE: incyy.l
	mkdir -p intermediatestore
	flex --outfile=intermediatestore/incyy.lex.c --header-file=intermediatestore/incyy.lex.h incyy.l
	touch INCYY.LEX.INTERMEDIATE
INCYY.TAB.INTERMEDIATE: incyy.y
	mkdir -p intermediatestore
	bison --defines=intermediatestore/incyy.tab.h --output=intermediatestore/incyy.tab.c incyy.y
	touch INCYY.TAB.INTERMEDIATE
incyy.lex.c: INCYY.LEX.INTERMEDIATE
	cp intermediatestore/incyy.lex.c .
incyy.lex.h: INCYY.LEX.INTERMEDIATE
	cp intermediatestore/incyy.lex.h .
incyy.tab.c: INCYY.TAB.INTERMEDIATE
	cp intermediatestore/incyy.tab.c .
incyy.tab.h: INCYY.TAB.INTERMEDIATE
	cp intermediatestore/incyy.tab.h .
