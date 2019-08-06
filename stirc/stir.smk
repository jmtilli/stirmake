.SUFFIXES:

$SRC_LIB = ["yyutils.c", "incyyutils.c"]
$SRC = [@$(SRC_LIB), "stir.c", "inc.c", "stirmake.c"]

$SRC_CPP_LIB = ["stirbce.cc"]
$SRC_CPP = [@$(SRC_CPP_LIB), "stirmain.cc", "arraytest.cc", "luatest.cc", "cctest.cc", "streamtest.cc", "globtest.cc", "bttest.cc", "jsontest.cc"]

$LEX_LIB = ["stiryy.l", "incyy.l"]
$LEX = $(LEX_LIB)

$YACC_LIB = ["stiryy.y", "incyy.y"]
$YACC = $(YACC_LIB)

@function $sufsuball($list, $suf1, $suf2)
  @locvar $ret = []
  @locvar $idx = 0
  @while($idx < $list[])
    @append($ret, @sufsub($list[$idx], $suf1, $suf2))
    $idx = $idx + 1
  @endwhile
  @return $ret
@endfunction

@function $LEXGEN_LIB()
  @return $sufsuball($LEX_LIB<>, ".l", ".lex.c")
@endfunction
@function $LEXGEN()
  @return $sufsuball($LEX<>, ".l", ".lex.c")
@endfunction

@function $YACCGEN_LIB()
  @return $sufsuball($YACC_LIB<>, ".y", ".tab.c")
@endfunction
@function $YACCGEN()
  @return $sufsuball($YACC<>, ".y", ".tab.c")
@endfunction

$GEN_LIB = [@$LEXGEN_LIB, @$YACCGEN_LIB]
$GEN = [@$LEXGEN, @$YACCGEN]

@function $OBJ_LIB()
  @return $sufsuball($SRC_LIB<>, ".c", ".o")
@endfunction
@function $OBJ()
  @return $sufsuball($SRC<>, ".c", ".o")
@endfunction

@function $OBJ_CPP_LIB()
  @return $sufsuball($SRC_CPP_LIB<>, ".cc", ".o")
@endfunction
@function $OBJ_CPP()
  @return $sufsuball($SRC_CPP<>, ".cc", ".o")
@endfunction

@function $OBJGEN_LIB()
  @return $sufsuball($GEN_LIB<>, ".c", ".o")
@endfunction
@function $OBJGEN()
  @return $sufsuball($GEN<>, ".c", ".o")
@endfunction

@function $ASM_LIB()
  @return $sufsuball($SRC_LIB<>, ".c", ".s")
@endfunction
@function $ASM()
  @return $sufsuball($SRC<>, ".c", ".s")
@endfunction

@function $ASMGEN_LIB()
  @return $sufsuball($GEN_LIB<>, ".c", ".s")
@endfunction
@function $ASMGEN()
  @return $sufsuball($GEN<>, ".c", ".s")
@endfunction

@function $DEP_LIB()
  @return $sufsuball($SRC_LIB<>, ".c", ".d")
@endfunction
@function $DEP()
  @return $sufsuball($SRC<>, ".c", ".d")
@endfunction

@function $DEP_CPP_LIB()
  @return $sufsuball($SRC_CPP_LIB<>, ".cc", ".d")
@endfunction
@function $DEP_CPP()
  @return $sufsuball($SRC_CPP<>, ".cc", ".d")
@endfunction

@function $DEPGEN_LIB()
  @return $sufsuball($GEN_LIB<>, ".c", ".d")
@endfunction
@function $DEPGEN()
  @return $sufsuball($GEN<>, ".c", ".d")
@endfunction

@function $subbuild($dir)
  @return [$(MAKE), "-C", $dir]
@endfunction

@function $headers()
  @return $filterOut($filterOut(@glob("*.h"), ".lex.h"), ".tab.h")
@endfunction

$WCCMD = ["wc", "-l", @$(LEX), @$(YACC), @$(SRC_CPP), @$(SRC), @$headers()]
$WCCCMD = ["wc", "-l", @$(LEX), @$(YACC), @$(SRC), @$headers()]

@phonyrule
all: stirmain stir inc arraytest luatest cctest streamtest globtest bttest jsontest build_abce stirmake


@phonyrule
build_abce: @recdep abce
@	$subbuild("abce")

wc:
@	$WCCMD

wcc:
@	$WCCCMD

#$LUAINC = "/usr/include/lua5.3"
#$LUALIB = "/usr/lib/x86_64-linux-gnu/liblua5.3.a"
$LUAINC = "/usr/include/luajit-2.1"
$LUALIB = "/usr/lib/x86_64-linux-gnu/libluajit-5.1.a"

#$CC = "gcc"
#$CPP = "g++"
$CC = "clang"
$CPP = "clang++"
$CFLAGS = ["-O3", "-Wall", "-g", "-I", $(LUAINC)]
$CPPFLAGS = ["-O3", "-Wall", "-g", "-I", $(LUAINC)]

# FIXME I got here, will have to fix these as well:

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
