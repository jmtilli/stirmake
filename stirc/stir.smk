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
    @append($ret, @D$sufsub($list[$idx], $suf1, $suf2)) # FIXME @sufsub
    $idx = $idx + 1
  @endwhile
  @return $ret
@endfunction

$LEXGEN_LIB = $sufsuball($LEX_LIB<>, ".l", ".lex.c")
$LEXGEN = $sufsuball($LEX<>, ".l", ".lex.c")
$YACCGEN_LIB = $sufsuball($YACC_LIB<>, ".y", ".tab.c")
$YACCGEN = $sufsuball($YACC<>, ".y", ".tab.c")

$GEN_LIB = [@$LEXGEN_LIB, @$YACCGEN_LIB]
$GEN = [@$LEXGEN, @$YACCGEN]

$OBJ_LIB = $sufsuball($SRC_LIB<>, ".c", ".o")
$OBJ = $sufsuball($SRC<>, ".c", ".o")
$OBJ_CPP_LIB = $sufsuball($SRC_CPP_LIB<>, ".cc", ".o")
$OBJ_CPP = $sufsuball($SRC_CPP<>, ".cc", ".o")
$OBJGEN_LIB = $sufsuball($GEN_LIB<>, ".c", ".o")
$OBJGEN = $sufsuball($GEN<>, ".c", ".o")
$ASM_LIB = $sufsuball($SRC_LIB<>, ".c", ".s")
$ASM = $sufsuball($SRC<>, ".c", ".s")
$ASMGEN_LIB = $sufsuball($GEN_LIB<>, ".c", ".s")
$ASMGEN = $sufsuball($GEN<>, ".c", ".s")
$DEP_LIB = $sufsuball($SRC_LIB<>, ".c", ".d")
$DEP = $sufsuball($SRC<>, ".c", ".d")
$DEP_CPP_LIB = $sufsuball($SRC_CPP_LIB<>, ".cc", ".d")
$DEP_CPP = $sufsuball($SRC_CPP<>, ".cc", ".d")
$DEPGEN_LIB = $sufsuball($GEN_LIB<>, ".c", ".d")
$DEPGEN = $sufsuball($GEN<>, ".c", ".d")

@function $subbuild($dir)
  @return [@D$(MAKE), "-C", $dir]
@endfunction

@function $headers()
  @return @D$filterOut(@D$filterOut(@D$glob("*.h"), ".lex.h"), ".tab.h") # FIXME @filterOut, @glob
@endfunction

$WCCMD = ["wc", "-l", @$(LEX), @$(YACC), @$(SRC_CPP), @$(SRC), @$headers()]
$WCCCMD = ["wc", "-l", @$(LEX), @$(YACC), @$(SRC), @$headers()]

@phonyrule: all: stirmain stir inc arraytest luatest cctest streamtest globtest bttest jsontest build_abce stirmake


@phonyrule: build_abce: @recdep abce
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

stirmain: stirmain.o libstirmake.a Makefile $(LUALIB) abce
@	[$(CPP), @$(CPPFLAGS), "-o", $@, @$filter(".o", $^), @$filter(".a", $^), "abce/libabce.a", "-lm", "-ldl"]

stirmake: stirmake.o libstirmake.a Makefile $(LUALIB) abce
@	[$(CPP), @$(CPPFLAGS), "-o", $@, @$filter(".o", $^), @$filter(".a", $^), "abce/libabce.a", "-lm", "-ldl"]

arraytest: arraytest.o libstirmake.a Makefile $(LUALIB)
@	[$(CPP), @$(CPPFLAGS), "-o", $@, @$filter(".o", $^), @$filter(".a", $^), "-lm", "-ldl"]

streamtest: streamtest.o libstirmake.a Makefile $(LUALIB)
@	[$(CPP), @$(CPPFLAGS), "-o", $@, @$filter(".o", $^), @$filter(".a", $^), "-lm", "-ldl"]

globtest: globtest.o libstirmake.a Makefile $(LUALIB)
@	[$(CPP), @$(CPPFLAGS), "-o", $@, @$filter(".o", $^), @$filter(".a", $^), "-lm", "-ldl"]

bttest: bttest.o libstirmake.a Makefile $(LUALIB)
@	[$(CPP), @$(CPPFLAGS), "-o", $@, @$filter(".o", $^), @$filter(".a", $^), "-lm", "-ldl"]

jsontest: jsontest.o libstirmake.a Makefile $(LUALIB)
@	[$(CPP), @$(CPPFLAGS), "-o", $@, @$filter(".o", $^), @$filter(".a", $^), "-lm", "-ldl"]

luatest: luatest.o libstirmake.a Makefile $(LUALIB)
@	[$(CPP), @$(CPPFLAGS), "-o", $@, @$filter(".o", $^), @$filter(".a", $^), "-lm", "-ldl"]

cctest: cctest.o libstirmake.a Makefile $(LUALIB)
@	[$(CPP), @$(CPPFLAGS), "-o", $@, @$filter(".o", $^), @$filter(".a", $^), "-lm", "-ldl"]

stir: stir.o libstirmake.a Makefile $(LUALIB) abce
@	[$(CC), @$(CFLAGS), "-o", $@, @$filter(".o", $^), @$filter(".a", $^), "abce/libabce.a", "-lm", "-ldl"]

inc: inc.o libstirmake.a Makefile $(LUALIB)
@	[$(CC), @$(CFLAGS), "-o", $@, @$filter(".o", $^), @$filter(".a", $^), "-lm", "-ldl"]

libstirmake.a: $(OBJ_LIB) $(OBJGEN_LIB) $(OBJ_CPP_LIB) Makefile
@	["rm", "-f", $@]
@	["ar", "rvs", $@, @$filter(".o", $^)]

@patrule: $(OBJ): '%.o': '%.c' '%.d' Makefile
@	[$(CC), @$(CFLAGS), "-c", "-o", $@, $<]
@	[$(CC), @$(CFLAGS), "-c", "-S", "-o", @strappend($*, ".s"), $<]
@patrule: $(OBJ_CPP): '%.o': '%.cc' '%.d' Makefile
@	[$(CPP), @$(CPPFLAGS), "-c", "-o", $@, $<]
@	[$(CPP), @$(CPPFLAGS), "-c", "-S", "-o", @strappend($*, ".s"), $<]
@patrule: $(OBJGEN): '%.o': '%.c' '%.h' '%.d' Makefile
@	[$(CC), @$(CFLAGS), "-Wno-sign-compare", "-Wno-missing-prototypes", "-c", "-o", $@, $<]
@	[$(CC), @$(CFLAGS), "-Wno-sign-compare", "-Wno-missing-prototypes", "-c", "-S", "-o", @strappend($*, ".s"), $<]

@patrule: $(DEP): '%.d': '%.c' Makefile
@	[$(CC), @$(CFLAGS), "-MM", "-MP", "-MT", @strappend(@strappend(@strappend($*, ".d "), $*), " .o"), "-o", $@, $<]
@patrule: $(DEP_CPP): '%.d': '%.cc' Makefile
@	[$(CPP), @$(CPPFLAGS), "-MM", "-MP", "-MT", @strappend(@strappend(@strappend($*, ".d "), $*), " .o"), "-o", $@, $<]
@patrule: $(DEPGEN): '%.d': '%.c' '%.h' Makefile
@	[$(CC), @$(CFLAGS), "-MM", "-MP", "-MT", @strappend(@strappend(@strappend($*, ".d "), $*), " .o"), "-o", $@, $<]

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

@cdepincludescurdir $(DEP)
@cdepincludescurdir $(DEP_CPP)
@cdepincludescurdir $(DEPGEN)

#-include *.d
