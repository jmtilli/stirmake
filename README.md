# Stirmake

![stirmake](stirmake.png)

Stirmake is a rewrite of the venerable make. It is intended to make build
systems easy to understand and maintain for mere mortals. At the same time, it
is intended to once and for all eliminate the need for frequent `make clean`.

To understand the motivation behind stirmake, one needs to first understand why
recursive make is harmful[1]. Then one needs to attempt to create a
non-recursive build system using make[2], finding it's very hard to do so, and
the result is an almost unmaintainable mess. One also could explore the
alternatives for make such as Makefile autogenerators (hint: a chain is as
strong as its weakest link, and a weak, not necessarily the weakest, link is
make), and standalone alternatives. The standalone alternatives include SCons,
Rake and Shake of which SCons and Rake are implemented in a slow interpreted
language, and Shake uses a strange Haskell-based input syntax.

## Feature set

* Fast C implementation, with performance comparable to make
* Intuitive syntax, similar to make
* Support for parallelism
* Support for including sub-Stirfiles in subdirectories
* Support for including Stirfiles past project boundaries, controlling what is visible to subprojects
* Executes shell commands in subdirectories, like recursive make and unlike inclusive whole-project make
* Proper data types; finally, filenames can have spaces
* Programmablity with a custom language
* TODO: LuaJIT integration
* Multiple targets per rules
* Dependency on a whole directory hierarchy, using its latest mtime
* Compatibility with `gcc -M` format dependency files
* TODO: Conditional compilaton
* Build command database, with dependency on build command
* Automatically deduced cleaning rules
* Fast bytecode based variable expansion
* Many sanity checks; some fatal errors, some helpful suggestions
* Support for invoking build tool in any directory, much like you can invoke git in any directory and it automatically detects where the `.git` top-level repository is located
* Full GNU make jobserver integration, allowing parallel sub-makes
* TODO: Modification of rules on the fly, so that the built system can affect the build system

## Building stirmake

## Data model

Bad programmers start from algorithms. Good programmers start from data
structures. "Show me your flowchart and conceal your tables, and I shall
continue to be mystified. Show me your tables, and I won't usually need your
flowchart; it'll be obvious." An example of a tool created by a good programmer
is Git, where the data model supports merges, being superior to CVS that does
not.

Stirmake aims to have good data structures from day one. Specifically:

* Variables have proper data types; the data model is very similar to JSON
* There is recursive nested scoping; variables in sub-Stirfiles affect only sub-sub-Stirfiles included in the sub-Stirfiles, but not the main-Stirfile
* Delayed evaluation is created using functions
* Rules have 1..N targets and 0..M dependencies
* Each rule has 0..K commands to execute, with executed commands stored to database

As example of true data typing, see:

```
$SRC = ["foo.c", "bar.c", "baz.c"]
```

As example of delayed evaluation:

```
$CCCMD<> = ["cc", "-Wall", "-c", $<]
```

where the marker `<>` means create a function from the expression.

As example of nested scoping:

```
@beginholeyscope
  $CCCMD = @LP $CCCMD
  @dirinclude "subproject"
@endscope
```

Now `$CCCMD` is visible to the subproject but `$SRC` is not. The `@LP` means
access lexical parent scope.

As example of rules, a Makefile is incapable of representing the following
Stirfile because the rule has 2 targets:

```
stiryy.tab.c stiryy.tab.h: stiryy.y
        byacc -d -p stiryy -o stiryy.tab.c stiryy.y
```

The best we can do with make is the following (ugh):

```
stiryy.tab.c: stiryy.y Makefile
        byacc -d -p stiryy -o .tmpc.stiryy.tab.c stiryy.y
        rm .tmpc.stiryy.tab.h
        mv .tmpc.stiryy.tab.c stiryy.tab.c
stiryy.tab.h: stiryy.y Makefile
        byacc -d -p stiryy -o .tmph.stiryy.tab.c stiryy.y
        rm .tmph.stiryy.tab.c
        mv .tmph.stiryy.tab.h stiryy.tab.h
```

Further minor details of the data model:

* Dependency can be: (i) normal, (ii) recursive, (iii) order-only
* Rule can be: (a) normal, (b) recursive target rule, (c) phony, (d) maybe-rule, (e) dist-rule

Recursive dependencies depend on the latest mtime within an entire hierarchy.
Order-only dependencies are executed only if the file/directory does not exist,
and a changed mtime does not cause execution of the rule.

Recursive target rules depend on targets inside a recursive dependency
hierarchy. Example:

```
@rectgtrule: subproj/bin/cmd subproj/lib/libsp.a: @recdep subproj
        make -C subproj
```

Phony-rules are well-known from make.

Maybe-rules do not check that the target is always updated. Example:
```
@maybe: foo: Makefile.foo foo.c
        make -f Makefile.foo
```

Dist-rules mean the rule creates a final binary ended for end-user and not some
intermediate object file. The only difference it has is for automatically
cleaning object files and binaries: dist-rule creates a binary instead of
object file.

## Nesting Stirfiles

## Invoking stirmake

Suppose there is the following hierarchy:

* `project/Stirfile`
    * `project/dir/Stirfile`
        * `project/dir/subproj/Stirfile`
            * `project/dir/subproj/subdir/Stirfile`

...and suppose `subproj` is a git submodule.

Suppose one is currently at `project/dir/subproj/subdir/`.

Then one invokes `stirmake` as `stirmake -a` (synonym: `smka`) to build
something relative to the entire project hierarchy, or `stirmake -p` (synonym:
`smkp`) to build something relative to the subproject, or `stirmake -t`
(synonym: `smkt`) to build something relative to the current directory.

As an example, the following are equal:

* `cd project/dir/subproj/subdir; smkt ../all`
* `cd project/dir/subproj/subdir; smkp all`
* `cd project/dir/subproj/subdir; smka project/dir/subproj/all`

where it is assumed that all of the Stirfiles include a phony target `all`.

If the target is not given, stirmake automatically uses the first target within
the project hierarchy / subproject / subdirectory.

### Autoclean

Cleaning should never be necessary. However, stirmake automatically figures out
the rules for cleaning object files and binaries. To clean, do some of these:

* `smka -bc`: clean binaries and object files of whole project hierarchy, then exit
* `smka -b`: clean binaries of whole project hierarchy, then exit
* `smka -c`: clean object files of whole project hierarchy, then exit
* `smka -bc all`: clean binaries and object files of whole project hierarchy, then build phony target `all`
* `smkp -bc`: clean binaries and object files of whole project
* `smkt -bc`: clean binaries and object files of current directory

What is currently missing is hooks for automatically cleaning results of
sub-makes. TODO do this someday

### Parallel builds

Parallel build uses the familiar syntax: `stirmake -j8`

### Other useful arguments

Stirmake has a debug mode that is enabled by using the `-d` command line
argument. It is very verbose and explains what stirmake does and why it does
that.

## Importing make auto-dependencies

## Build command database

## Examples

## LuaJIT integration

## GNU make jobserver integration

## References

1. Miller, P.A. (1998), Recursive Make Considered Harmful, AUUGN Journal of AUUG Inc., 19(1), pp. 14-25, http://aegis.sourceforge.net/auug97.pdf
2. Mokhov, A., Mitchell, N., Peyton Jones, S., Marlow, S. (2016), Non-recursive make considered harmful: build systems at scale, ACM SIGPLAN Notices - Haskell '16, 51(12), pp. 170-181, https://www.microsoft.com/en-us/research/wp-content/uploads/2016/03/hadrian.pdf
