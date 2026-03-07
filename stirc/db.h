#ifndef _DB_H_
#define _DB_H_

#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#include "stiryy.h"
#include "const.h"
#include "dbyy.h"

struct cmd {
  char ***args;
};

struct tsdbe {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  mysize_t stringtabidx;
  struct timespec ts;
  struct timespec tsnew;
  off_t sz;
  off_t sznew;
  int seen;
};

struct dbe {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  mysize_t tgtidx; // key
  mysize_t diridx; // non-key
  struct cmd cmds; // non-key
};

struct tsdb {
  struct abce_rb_tree_nocmp byname[DB_SIZE];
  struct linked_list_head ll;
};

struct db {
  struct abce_rb_tree_nocmp byname[DB_SIZE];
  struct linked_list_head ll;
};

extern struct tsdb tsdb;
extern struct db db;
extern int usetsdb;

int tsszstoresource(struct tsdb *tsdb, mysize_t stringtabidx, struct timespec ts, off_t sz);
int tsszstoretarget(struct tsdb *tsdb, mysize_t stringtabidx, struct timespec ts, off_t sz);

struct tsdbe *get_tsdbe(mysize_t stringtabidx);
struct dbe *get_dbe(mysize_t stringtabidx);

void ins_dbe(struct db *db, struct dbe *dbe);
void maybe_del_dbe(struct db *db, mysize_t tgtidx);

void ins_tsdbe(struct tsdb *tsdb, struct tsdbe *tsdbe);
void maybe_del_tsdbe(struct tsdb *tsdb, mysize_t tgtidx);

#endif
