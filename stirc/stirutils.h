#ifndef _STIRUTILS_H_
#define _STIRUTILS_H_
#include <stddef.h>
#include <time.h>

char *calc_forward_path(char *storcwd, size_t upcnt);
char *dir_up(char *old);
char *myitoa(int i);
int my_get_nprocs(void);
int utimensat_both_emul(const char *pathname, struct timespec time, int l,
                        int forwards);

#endif
