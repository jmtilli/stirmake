all: stir

stir: stir.o stiryy.lex.o stiryy.tab.o yyutils.o
	cc -o stir stir.o stiryy.lex.o stiryy.tab.o yyutils.o

stir.o: stir.c
	cc -c -o stir.o stir.c

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
