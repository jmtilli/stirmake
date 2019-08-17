#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include "yyutils.h"
#include "opcodes.h"
#include "engine.h"
#include "incyyutils.h"

int debug = 0;

int self_pipe_fd[2];

int jobserver_fd[2];

std::unordered_map<std::string, int> ruleid_by_tgt;

class Cmd {
  public:
    std::vector<std::string> args;
    Cmd() {}
    Cmd(const std::vector<std::string> &v): args(v) {}
};

int ts_cmp(struct timespec ta, struct timespec tb)
{
  if (ta.tv_sec > tb.tv_sec)
  {
    return 1;
  }
  if (ta.tv_sec < tb.tv_sec)
  {
    return -1;
  }
  if (ta.tv_nsec > tb.tv_nsec)
  {
    return 1;
  }
  if (ta.tv_nsec < tb.tv_nsec)
  {
    return -1;
  }
  return 0;
}

class Dep {
  public:
    std::string name;
    bool recursive;

    Dep(std::string name, bool recursive = false):
      name(name), recursive(recursive)
    {
    }

    bool operator==(const Dep &that) const
    {
      return this->name == that.name;
    }
};

namespace std {
    template <>
        class hash<Dep>{
        public :
            size_t operator()(const Dep &dep ) const
            {
                return hash<string>()(dep.name);
            }
    };
};

class Rule {
  public:
    bool phony;
    bool executed;
    bool executing;
    bool queued;
    std::unordered_set<std::string> tgts;
    std::unordered_set<Dep> deps;
    std::unordered_set<int> deps_remain;
    Cmd cmd;
    int ruleid;

    Rule(): phony(false), executed(false), executing(false), queued(false) {}

    void calc_deps_remain(void)
    {
      for (auto it = deps.begin(); it != deps.end(); it++)
      {
        if (ruleid_by_tgt.find(it->name) != ruleid_by_tgt.end())
        {
          deps_remain.insert(ruleid_by_tgt[it->name]);
        }
      }
    }
};
std::ostream &operator<<(std::ostream &o, const Rule &r)
{
  bool first = true;
  o << "Rule(";
  o << r.ruleid;
  o << ",[";
  for (auto it = r.tgts.begin(); it != r.tgts.end(); it++)
  {
    if (first)
    {
      first = false;
    }
    else
    {
      o << ",";
    }
    o << *it;
  }
  first = true;
  o << "],[";
  for (auto it = r.deps.begin(); it != r.deps.end(); it++)
  {
    if (first)
    {
      first = false;
    }
    else
    {
      o << ",";
    }
    o << it->name;
  }
  o << "])";
  return o;
}

int children = 0;
const int limit = 2;

std::vector<Rule> rules;

std::unordered_map<std::string, std::unordered_set<int>> ruleids_by_dep;


void better_cycle_detect(int cur, std::vector<bool> &parents, std::vector<bool> &no_cycles, int &cntr)
{
  cntr++;
  if (no_cycles[cur])
  {
    return;
  }
  if (parents[cur])
  {
    std::cerr << "cycle found" << std::endl;
    for (size_t i = 0; i < rules.size(); i++)
    {
      if (parents[i])
      {
        std::cerr << " rule in cycle: " << rules[i] << std::endl;
      }
    }
    exit(1);
  }
  parents[cur] = true;
  for (auto it = rules[cur].deps.begin(); it != rules[cur].deps.end(); it++)
  {
    if (ruleid_by_tgt.find(it->name) != ruleid_by_tgt.end())
    {
      better_cycle_detect(ruleid_by_tgt[it->name], parents, no_cycles, cntr);
    }
  }
  parents[cur] = false;
  no_cycles[cur] = true;
}

std::vector<bool> better_cycle_detect(int cur)
{
  std::vector<bool> no_cycles(rules.size());
  std::vector<bool> parents(rules.size());
  int cntr = 0;
  better_cycle_detect(cur, parents, no_cycles, cntr);
  std::cout << "BETTER CNTR " << cntr << std::endl;
  return no_cycles;
}


std::unordered_map<std::string, std::pair<bool, std::unordered_set<std::string> > > add_deps;

void add_dep(const std::vector<std::string> &tgts,
             const std::vector<std::string> &deps,
             bool phony)
{
  for (auto tgt = tgts.begin(); tgt != tgts.end(); tgt++)
  {
    if (add_deps.find(*tgt) == add_deps.end())
    {
      add_deps[*tgt] = std::make_pair(false, std::unordered_set<std::string>());
    }
    if (phony)
    {
      add_deps[*tgt].first = true;
    }
    add_deps[*tgt].second.rehash(deps.size());
    for (auto dep = deps.begin(); dep != deps.end(); dep++)
    {
      add_deps[*tgt].second.insert(*dep);
    }
  }
}

void process_additional_deps(void)
{
  for (auto it = add_deps.begin(); it != add_deps.end(); it++)
  {
    if (ruleid_by_tgt.find(it->first) == ruleid_by_tgt.end())
    {
      Rule r;
      r.ruleid = rules.size();
      //std::cout << "adding tgt " << it->first << std::endl;
      ruleid_by_tgt[it->first] = r.ruleid;
      r.tgts.insert(it->first);
      r.deps.rehash(r.deps.size() + it->second.second.size());
      std::copy(it->second.second.begin(), it->second.second.end(),
                std::inserter(r.deps, r.deps.begin()));
      r.phony = it->second.first;
      for (auto it2 = r.deps.begin(); it2 != r.deps.end(); it2++)
      {
        if (ruleids_by_dep.find(it2->name) == ruleids_by_dep.end())
        {
          ruleids_by_dep[it2->name] = std::unordered_set<int>();
        }
        ruleids_by_dep[it2->name].insert(r.ruleid);
        //std::cout << " dep: " << *it2 << std::endl;
      }
      rules.push_back(r);
      //std::cout << "added Rule: " << r << std::endl;
      continue;
    }
    Rule &r = rules[ruleid_by_tgt[it->first]];
    if (it->second.first)
    {
      r.phony = true;
    }
    //std::cout << "modifying rule " << r << std::endl;
    std::copy(it->second.second.begin(), it->second.second.end(),
              std::inserter(r.deps, r.deps.begin()));
    //std::cout << "modified rule " << r << std::endl;
    for (auto it2 = r.deps.begin(); it2 != r.deps.end(); it2++)
    {
      if (ruleids_by_dep.find(it2->name) == ruleids_by_dep.end())
      {
        ruleids_by_dep[it2->name] = std::unordered_set<int>();
      }
      ruleids_by_dep[it2->name].insert(r.ruleid);
    }
  }
}

void add_rule(const std::vector<std::string> &tgts,
              const std::vector<Dep> &deps,
              const std::vector<std::string> &cmdargs,
              bool phony)
{
  Rule r;
  Cmd c(cmdargs);
  r.phony = phony;
  if (tgts.size() <= 0)
  {
    abort();
  }
  if (phony && tgts.size() != 1)
  {
    abort();
  }
  r.tgts.rehash(tgts.size());
  r.deps.rehash(deps.size());
  std::copy(tgts.begin(), tgts.end(), std::inserter(r.tgts, r.tgts.begin()));
  std::copy(deps.begin(), deps.end(), std::inserter(r.deps, r.deps.begin()));
  //r.tgts = tgts;
  //r.deps = deps;
  r.cmd = c;
  r.ruleid = rules.size();
  rules.push_back(r);
  for (auto it = tgts.begin(); it != tgts.end(); it++)
  {
    if (ruleid_by_tgt.find(*it) != ruleid_by_tgt.end())
    {
      std::cerr << "duplicate rule" << std::endl;
      exit(1);
    }
    ruleid_by_tgt[*it] = r.ruleid;
  }
  for (auto it = deps.begin(); it != deps.end(); it++)
  {
    if (ruleids_by_dep.find(it->name) == ruleids_by_dep.end())
    {
      ruleids_by_dep[it->name] = std::unordered_set<int>();
    }
    ruleids_by_dep[it->name].insert(r.ruleid);
  }
}

std::vector<int> ruleids_to_run;

std::unordered_map<pid_t, int> ruleid_by_pid;

pid_t fork_child(int ruleid)
{
  std::vector<char*> args;
  pid_t pid;
  Cmd cmd = rules.at(ruleid).cmd;

  pid = fork();
  if (pid < 0)
  {
    abort();
  }
  else if (pid == 0)
  {
    for (auto it = cmd.args.begin(); it != cmd.args.end(); it++)
    {
      args.push_back(strdup(it->c_str()));
    }
    args.push_back(NULL);
    close(self_pipe_fd[0]);
    close(self_pipe_fd[1]);
    // FIXME check for make
    close(jobserver_fd[0]);
    close(jobserver_fd[1]);
    execvp(args[0], &args[0]);
    //write(1, "Err\n", 4);
    _exit(1);
  }
  else
  {
    children++;
    ruleid_by_pid[pid] = ruleid;
    return pid;
  }
}

void mark_executed(int ruleid);

struct timespec rec_mtim(const char *name)
{
  struct timespec max;
  struct stat statbuf;
  DIR *dir = opendir(name);
  //std::cout << "Statting " << name << std::endl;
  if (stat(name, &statbuf) != 0)
  {
    printf("Can't open file %s\n", name);
    exit(1);
  }
  max = statbuf.st_mtim;
  if (lstat(name, &statbuf) != 0)
  {
    printf("Can't open file %s\n", name);
    exit(1);
  }
  if (ts_cmp(statbuf.st_mtim, max) > 0)
  {
    max = statbuf.st_mtim;
  }
  if (dir == NULL)
  {
    printf("Can't open dir %s\n", name);
    exit(1);
  }
  for (;;)
  {
    struct dirent *de = readdir(dir);
    struct timespec cur;
    std::string nam2(name);
    if (de == NULL)
    {
      break;
    }
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
    {
      continue;
    }
    nam2 += std::string("/") + de->d_name;
    //if (de->d_type == DT_DIR)
    if (0)
    {
      cur = rec_mtim(nam2.c_str());
    }
    else
    {
      if (stat(nam2.c_str(), &statbuf) != 0)
      {
        printf("Can't open file %s\n", nam2.c_str());
        exit(1);
      }
      cur = statbuf.st_mtim;
      if (lstat(nam2.c_str(), &statbuf) != 0)
      {
        printf("Can't open file %s\n", nam2.c_str());
        exit(1);
      }
      if (ts_cmp(statbuf.st_mtim, cur) > 0)
      {
        cur = statbuf.st_mtim;
      }
    }
    if (ts_cmp(cur, max) > 0)
    {
      //std::cout << "nam2 file new " << nam2 << std::endl;
      max = cur;
    }
    if ((statbuf.st_mode & S_IFMT) == S_IFDIR)
    {
      cur = rec_mtim(nam2.c_str());
      if (ts_cmp(cur, max) > 0)
      {
        //std::cout << "nam2 dir new " << nam2 << std::endl;
        max = cur;
      }
    }
  }
  closedir(dir);
  return max;
}

void do_exec(int ruleid)
{
  Rule &r = rules.at(ruleid);
  if (debug)
  {
    std::cout << "do_exec " << ruleid << std::endl;
  }
  if (!r.queued)
  {
    int has_to_exec = 0;
    if (!r.phony && r.deps.size() > 0)
    {
      int seen_nonphony = 0;
      int seen_tgt = 0;
      struct timespec st_mtim = {}, st_mtimtgt = {};
      for (auto it = r.deps.begin(); it != r.deps.end(); it++)
      {
        struct stat statbuf;
        //std::cout << "it " << *it << std::endl;
        if (ruleid_by_tgt.find(it->name) != ruleid_by_tgt.end())
        {
          //std::cout << "ruleid by tgt: " << *it << std::endl;
          //std::cout << "ruleid by tgt- " << ruleid_by_tgt[*it] << std::endl;
          if (rules.at(ruleid_by_tgt[it->name]).phony)
          {
            has_to_exec = 1;
            //std::cout << "phony" << std::endl;
            continue;
          }
          //std::cout << "nonphony" << std::endl;
        }
        if (it->recursive)
        {
          struct timespec st_rectim = rec_mtim(it->name.c_str());
          if (!seen_nonphony || ts_cmp(st_rectim, st_mtim) > 0)
          {
            st_mtim = st_rectim;
          }
          seen_nonphony = 1;
          continue;
        }
        if (stat(it->name.c_str(), &statbuf) != 0)
        {
          has_to_exec = 1;
          break;
          //perror("can't stat");
          //fprintf(stderr, "file was: %s\n", it->c_str());
          //abort();
        }
        if (!seen_nonphony || ts_cmp(statbuf.st_mtim, st_mtim) > 0)
        {
          st_mtim = statbuf.st_mtim;
        }
        seen_nonphony = 1;
        if (lstat(it->name.c_str(), &statbuf) != 0)
        {
          has_to_exec = 1;
          break;
          //perror("can't lstat");
          //fprintf(stderr, "file was: %s\n", it->c_str());
          //abort();
        }
        if (!seen_nonphony || ts_cmp(statbuf.st_mtim, st_mtim) > 0)
        {
          st_mtim = statbuf.st_mtim;
        }
        seen_nonphony = 1;
      }
      for (auto it = r.tgts.begin(); it != r.tgts.end(); it++)
      {
        struct stat statbuf;
        if (stat(it->c_str(), &statbuf) != 0)
        {
          has_to_exec = 1;
          break;
        }
        if (!seen_tgt || ts_cmp(statbuf.st_mtim, st_mtimtgt) < 0)
        {
          st_mtimtgt = statbuf.st_mtim;
        }
        seen_tgt = 1;
      }
      if (!has_to_exec)
      {
        if (!seen_tgt)
        {
          abort();
        }
        if (seen_nonphony && ts_cmp(st_mtimtgt, st_mtim) < 0)
        {
          has_to_exec = 1;
        }
      }
    }
    else if (r.phony)
    {
      has_to_exec = 1;
    }
    if (has_to_exec && r.cmd.args.size() > 0)
    {
      if (debug)
      {
        std::cout << "do_exec: has_to_exec " << ruleid << std::endl;
      }
      ruleids_to_run.push_back(ruleid);
      r.queued = true;
    }
    else
    {
      if (debug)
      {
        std::cout << "do_exec: mark_executed " << ruleid << std::endl;
      }
      r.queued = true;
      mark_executed(ruleid);
    }
  }
}

void consider(int ruleid)
{
  Rule &r = rules.at(ruleid);
  int toexecute = 0;
  if (debug)
  {
    std::cout << "considering " << r << std::endl;
  }
  if (r.executed)
  {
    if (debug)
    {
      std::cout << "already execed " << r << std::endl;
    }
    return;
  }
  if (r.executing)
  {
    if (debug)
    {
      std::cout << "already execing " << r << std::endl;
    }
    return;
  }
  r.executing = true;
  for (auto it = r.deps.begin(); it != r.deps.end(); it++)
  {
    if (ruleid_by_tgt.find(it->name) != ruleid_by_tgt.end())
    {
      consider(ruleid_by_tgt[it->name]);
      if (!rules.at(ruleid_by_tgt[it->name]).executed)
      {
        if (debug)
        {
          std::cout << "rule " << ruleid_by_tgt[it->name] << " not executed, executing rule " << ruleid << std::endl;
        }
        toexecute = 1;
      }
    }
  }
/*
  if (r.phony)
  {
    r.executed = true;
    break;
  }
*/
  if (!toexecute && !r.queued)
  {
    do_exec(ruleid);
    //ruleids_to_run.push_back(ruleid);
    //r.queued = true;
  }
/*
  ruleids_to_run.push_back(ruleid);
  r.executed = true;
*/
}

void reconsider(int ruleid, int ruleid_executed)
{
  Rule &r = rules.at(ruleid);
  int toexecute = 0;
  if (debug)
  {
    std::cout << "reconsidering " << r << std::endl;
  }
  if (r.executed)
  {
    if (debug)
    {
      std::cout << "already execed " << r << std::endl;
    }
    return;
  }
  if (!r.executing)
  {
    if (debug)
    {
      std::cout << "rule not executing " << r << std::endl;
    }
    return;
  }
  r.deps_remain.erase(ruleid_executed);
  if (r.deps_remain.size() > 0)
  {
    toexecute = 1;
  }
#if 0
  for (auto it = r.deps.begin(); it != r.deps.end(); it++)
  {
    int dep = ruleid_by_tgt[*it];
    if (!rules.at(dep).executed)
    {
      if (debug)
      {
        std::cout << "rule " << ruleid_by_tgt[*it] << " not executed, executing rule " << ruleid << std::endl;
      }
      toexecute = 1;
      break;
    }
  }
#endif
  if (!toexecute && !r.queued)
  {
    do_exec(ruleid);
    //ruleids_to_run.push_back(ruleid);
    //r.queued = true;
  }
}

void mark_executed(int ruleid)
{
  Rule &r = rules.at(ruleid);
  if (r.executed)
  {
    abort();
  }
  if (!r.executing)
  {
    abort();
  }
  r.executed = true;
  if (ruleid == 0)
  {
    return;
  }
  for (auto it = r.tgts.begin(); it != r.tgts.end(); it++)
  {
    std::unordered_set<int> &s = ruleids_by_dep[*it];
    for (auto it2 = s.begin(); it2 != s.end(); it2++)
    {
      reconsider(*it2, ruleid);
    }
  }
}

/*
.PHONY: all

all: l1g.txt

l1g.txt: l2a.txt l2b.txt
	./touchs1 l1g.txt

l2a.txt l2b.txt: l3c.txt l3d.txt l3e.txt
	./touchs1 l2a.txt l2b.txt

l3c.txt: l4f.txt
	./touchs1 l3c.txt

l3d.txt: l4f.txt
	./touchs1 l3d.txt

l3e.txt: l4f.txt
	./touchs1 l3e.txt
 */

void set_nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL);
  if (flags < 0)
  {
    std::terminate();
  }
  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0)
  {
    std::terminate();
  }
}

void sigchld_handler(int x)
{
  write(self_pipe_fd[1], ".", 1);
}

void pathological_test(void)
{
  int rule;
  std::vector<std::string> v_rules;
  std::string rulestr;
  //std::ofstream mf("Makefile");
  //mf << "all: d2999" << std::endl;
  for (rule = 0; rule < 3000; rule++)
  {
    std::ostringstream oss;
    std::vector<std::string> v_rule;
    oss << rule;
    rulestr = oss.str();
    v_rule.push_back(rulestr);
    add_dep(v_rule, v_rules, 0);
#if 0
    mf << "d" << rulestr << ": ";
    for (auto it = v_rules.begin(); it != v_rules.end(); it++)
      mf << "d" << *it << " ";
    mf << std::endl;
#endif
    v_rules.push_back(rulestr);
  }
  process_additional_deps();
  std::cout << "starting DFS2" << std::endl;
  auto cl1 = std::chrono::steady_clock::now();
  better_cycle_detect(ruleid_by_tgt[rulestr]);
  auto cl2 = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(cl2 - cl1);
  std::cout << "ending DFS2 in " << ms.count() << " ms" << std::endl;
  //mf.close();
  exit(0);
}

void stack_conf(void)
{
  const rlim_t stackSize = 16 * 1024 * 1024;
  struct rlimit rl;
  int result;
  result = getrlimit(RLIMIT_STACK, &rl);
  if (result != 0)
  {
    std::terminate();
  }
  if (rl.rlim_cur < stackSize)
  {
    rl.rlim_cur = stackSize;
    result = setrlimit(RLIMIT_STACK, &rl);
    if (result != 0)
    {
      std::terminate();
    }
  }
}

stringtab st;
std::vector<memblock> all_scopes; // FIXME needed?
std::vector<memblock> scope_stack;

extern "C"
size_t symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen)
{
  std::string str(symbol, symlen);
  return st.add(str);
}

#if 0
extern "C"
size_t stiryy_add_fun_sym(struct stiryy *stiryy, const char *symbol, int maybe, size_t loc)
{
  size_t old = (size_t)-1;
  memblock &mb = scope_stack.back();
  if (mb.type != memblock::T_SC)
  {
    std::terminate();
  }
  scope *sc = mb.u.sc;
  std::string str(symbol);
  if (sc->vars.find(str) != sc->vars.end()
      //&& sc->vars[str].type == memblock::T_F
     )
  {
    old = st.addNonString(sc->vars[str]);
    if (maybe)
    {
      return old;
    }
    //old = sc->vars[str].u.d;
  }
  sc->vars[str] = memblock(loc, true);
  return old;
}
#endif

int main(int argc, char **argv)
{
  all_scopes.push_back(memblock(new scope()));
  scope_stack.push_back(all_scopes.back());
#if 0
  std::vector<std::string> v_all{"all"};
  std::vector<std::string> v_l1g{"l1g.txt"};
  std::vector<std::string> v_l2ab{"l2a.txt", "l2b.txt"};
  std::vector<std::string> v_l2a{"l2a.txt"};
  std::vector<std::string> v_l3cde{"l3c.txt", "l3d.txt", "l3e.txt"};
  std::vector<std::string> v_l3cd{"l3c.txt", "l3d.txt"};
  std::vector<std::string> v_l3c{"l3c.txt"};
  std::vector<std::string> v_l3d{"l3d.txt"};
  std::vector<std::string> v_l3e{"l3e.txt"};
  std::vector<std::string> v_l4f{"l4f.txt"};

  std::vector<std::string> argt_l2ab{"./touchs1", "l2a.txt", "l2b.txt"};
  std::vector<std::string> argt_l3c{"./touchs1", "l3c.txt"};
  std::vector<std::string> argt_l3d{"./touchs1", "l3d.txt"};
  std::vector<std::string> argt_l3e{"./touchs1", "l3e.txt"};
  std::vector<std::string> argt_l1g{"./touchs1", "l1g.txt"};
  std::vector<std::string> arge_all{"echo", "all"};

  pathological_test();

  add_rule(v_all, v_l1g, arge_all, 1);
  add_rule(v_l1g, v_l2ab, argt_l1g, 0);
  add_rule(v_l2ab, v_l3cd, argt_l2ab, 0);
  add_rule(v_l3c, v_l4f, argt_l3c, 0);
  add_rule(v_l3d, v_l4f, argt_l3d, 0);
  add_rule(v_l3e, v_l4f, argt_l3e, 0);
  add_dep(v_l2a, v_l3e, 0);
#endif
  FILE *f = fopen("Stirfile", "r");
  struct stiryy stiryy = {};

  stiryy_init(&stiryy);

  if (!f)
  {
    std::terminate();
  }
  stiryydoparse(f, &stiryy);
  fclose(f);

  std::vector<memblock> stack;

  stack.push_back(memblock(-1, false, true));
  stack.push_back(memblock(-1, false, true));
  size_t ip = scope_stack.back().u.sc->vars["MYFLAGS"].u.d + 9;
  std::cout << "to become ip: " << ip << std::endl;

  microprogram_global = stiryy.bytecode;
  microsz_global = stiryy.bytesz;
  st_global = &st;
  engine(scope_stack.back().u.sc->lua, scope_stack.back(),
         stack, ip);

  std::cout << "STACK SIZE: " << stack.size() << std::endl;
  std::cout << "DUMP: ";
  stack.back().dump();
  std::cout << std::endl;
  exit(0);

  stack_conf();

  ruleid_by_tgt.rehash(stiryy.rulesz);
  for (auto it = stiryy.rules; it != stiryy.rules + stiryy.rulesz; it++)
  {
    std::vector<std::string> tgt;
    std::vector<Dep> dep;
    std::vector<std::string> cmd;
    //std::copy(it->deps, it->deps+it->depsz, std::back_inserter(dep));
    for (auto it2 = it->deps; it2 != it->deps+it->depsz; it2++)
    {
      dep.push_back(Dep(it2->name, it2->rec));
    }
    std::copy(it->targets, it->targets+it->targetsz, std::back_inserter(tgt));
    if (tgt.size() > 0) // FIXME chg to if (1)
    {
      if (debug)
      {
        std::cout << "ADDING RULE" << std::endl;
      }
      add_rule(tgt, dep, cmd, 0);
    }
  }

  for (size_t i = 0; i < stiryy.cdepincludesz; i++)
  {
    struct incyy incyy = {};
    f = fopen(stiryy.cdepincludes[i], "r");
    if (!f)
    {
      std::terminate();
    }
    incyydoparse(f, &incyy);
    for (auto it = incyy.rules; it != incyy.rules + incyy.rulesz; it++)
    {
      std::vector<std::string> tgt;
      std::vector<std::string> dep;
      std::copy(it->deps, it->deps+it->depsz, std::back_inserter(dep));
      std::copy(it->targets, it->targets+it->targetsz, std::back_inserter(tgt));
      add_dep(tgt, dep, 0);
    }
    fclose(f);
    incyy_free(&incyy);
  }
  stiryy_free(&stiryy);

  //add_dep(v_l3e, v_l1g, 0); // offending rule

  process_additional_deps();

  std::vector<bool> no_cycles = better_cycle_detect(0);

  for (auto it = rules.begin(); it != rules.end(); it++)
  {
    it->calc_deps_remain();
  }

  // Delete unreachable rules from ruleids_by_dep
#if 1
  for (auto it = ruleids_by_dep.begin(); it != ruleids_by_dep.end(); )
  {
    if (no_cycles[ruleid_by_tgt[it->first]])
    {
      for (auto it2 = it->second.begin(); it2 != it->second.end(); )
      {
        if (no_cycles[*it2])
        {
          it2++;
        }
        else
        {
          it2 = it->second.erase(it2);
        }
      }
      it->second.rehash(it->second.size());
      it++;
    }
    else
    {
      it = ruleids_by_dep.erase(it);
    }
  }
  // Delete unreachable rules from ruleid_by_tgt
  for (auto it = ruleid_by_tgt.begin(); it != ruleid_by_tgt.end(); )
  {
    if (no_cycles[it->second])
    {
      it++;
    }
    else
    {
      it = ruleid_by_tgt.erase(it);
    }
  }
  ruleid_by_tgt.rehash(ruleid_by_tgt.size());
#endif

  if (pipe(self_pipe_fd) != 0)
  {
    std::terminate();
  }
  set_nonblock(self_pipe_fd[0]);
  set_nonblock(self_pipe_fd[1]);
  if (pipe(jobserver_fd) != 0)
  {
    std::terminate();
  }
  set_nonblock(jobserver_fd[0]);
  set_nonblock(jobserver_fd[1]);

  for (int i = 0; i < limit - 1; i++)
  {
    write(jobserver_fd[1], ".", 1);
  }

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = sigchld_handler;
  sigaction(SIGCHLD, &sa, NULL);

  consider(0);

  while (!ruleids_to_run.empty())
  {
    if (children)
    {
      char ch;
      if (read(jobserver_fd[0], &ch, 1) != 1)
      {
        break;
      }
    }
    std::cout << "forking1 child" << std::endl;
    fork_child(ruleids_to_run.back());
    ruleids_to_run.pop_back();
  }

/*
  while (children < limit && !ruleids_to_run.empty())
  {
    std::cout << "forking1 child" << std::endl;
    fork_child(ruleids_to_run.back());
    ruleids_to_run.pop_back();
  }
*/

  int maxfd = 0;
  if (self_pipe_fd[0] > maxfd)
  {
    maxfd = self_pipe_fd[0];
  }
  if (jobserver_fd[0] > maxfd)
  {
    maxfd = jobserver_fd[0];
  }
  char chbuf[100];
  while (children > 0)
  {
    int wstatus = 0;
    fd_set readfds;
    FD_SET(self_pipe_fd[0], &readfds);
    if (!ruleids_to_run.empty())
    {
      FD_SET(jobserver_fd[0], &readfds);
    }
    select(maxfd+1, &readfds, NULL, NULL, NULL);
    std::cout << "SELECT RETURNED" << std::endl;
    if (read(self_pipe_fd[0], chbuf, 100))
    {
      for (;;)
      {
        pid_t pid;
        pid = waitpid(-1, &wstatus, WNOHANG);
        if (pid == 0)
        {
          break;
        }
        if (wstatus != 0 && pid > 0)
        {
          if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
          {
            std::cout << "error exit status" << std::endl;
            std::cout << WIFEXITED(wstatus) << std::endl;
            std::cout << WIFSIGNALED(wstatus) << std::endl;
            std::cout << WEXITSTATUS(wstatus) << std::endl;
            return 1;
          }
        }
        if (children <= 0 && ruleids_to_run.empty())
        {
          if (pid < 0 && errno == ECHILD)
          {
            if (rules.at(0).executed)
            {
              return 0;
            }
            else
            {
              std::cerr << "can't execute rule 0" << std::endl;
              abort();
            }
          }
          abort();
        }
        if (pid < 0)
        {
          if (errno == ECHILD)
          {
            break;
          }
          abort();
        }
        int ruleid = ruleid_by_pid[pid];
        if (ruleid_by_pid.erase(pid) != 1)
        {
          abort();
        }
        mark_executed(ruleid);
        children--;
        if (children != 0)
        {
          write(jobserver_fd[1], ".", 1);
        }
      }
    }
#if 0
    while (children < limit && !ruleids_to_run.empty())
    {
      std::cout << "forking child" << std::endl;
      fork_child(ruleids_to_run.back());
      ruleids_to_run.pop_back();
    }
#endif
    while (!ruleids_to_run.empty())
    {
      if (children)
      {
        char ch;
        if (read(jobserver_fd[0], &ch, 1) != 1)
        {
          break;
        }
      }
      std::cout << "forking child" << std::endl;
      fork_child(ruleids_to_run.back());
      ruleids_to_run.pop_back();
    }
  }
  return 0;
}