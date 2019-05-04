#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
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
#include "incyyutils.h"

int self_pipe_fd[2];

int jobserver_fd[2];

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

class Rule {
  public:
    bool phony;
    bool executed;
    bool executing;
    bool queued;
    std::unordered_set<std::string> tgts;
    std::unordered_set<std::string> deps;
    Cmd cmd;
    int ruleid;

    Rule(): phony(false), executed(false), executing(false), queued(false) {}
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
    o << *it;
  }
  o << "])";
  return o;
}

int children = 0;
const int limit = 2;

std::vector<Rule> rules;

std::unordered_map<std::string, int> ruleid_by_tgt;
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
    if (ruleid_by_tgt.find(*it) != ruleid_by_tgt.end())
    {
      better_cycle_detect(ruleid_by_tgt[*it], parents, no_cycles, cntr);
    }
  }
  parents[cur] = false;
  no_cycles[cur] = true;
}

void better_cycle_detect(int cur)
{
  std::vector<bool> no_cycles(rules.size());
  std::vector<bool> parents(rules.size());
  int cntr = 0;
  better_cycle_detect(cur, parents, no_cycles, cntr);
  std::cout << "BETTER CNTR " << cntr << std::endl;
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
      std::copy(it->second.second.begin(), it->second.second.end(),
                std::inserter(r.deps, r.deps.begin()));
      r.phony = it->second.first;
      for (auto it2 = r.deps.begin(); it2 != r.deps.end(); it2++)
      {
        if (ruleids_by_dep.find(*it2) == ruleids_by_dep.end())
        {
          ruleids_by_dep[*it2] = std::unordered_set<int>();
        }
        ruleids_by_dep[*it2].insert(r.ruleid);
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
      if (ruleids_by_dep.find(*it2) == ruleids_by_dep.end())
      {
        ruleids_by_dep[*it2] = std::unordered_set<int>();
      }
      ruleids_by_dep[*it2].insert(r.ruleid);
    }
  }
}

void add_rule(const std::vector<std::string> &tgts,
              const std::vector<std::string> &deps,
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
    if (ruleids_by_dep.find(*it) == ruleids_by_dep.end())
    {
      ruleids_by_dep[*it] = std::unordered_set<int>();
    }
    ruleids_by_dep[*it].insert(r.ruleid);
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

void do_exec(int ruleid)
{
  Rule &r = rules.at(ruleid);
  std::cout << "do_exec " << ruleid << std::endl;
  if (!r.queued)
  {
    int has_to_exec = 0;
    if (!r.phony && r.deps.size() > 0)
    {
      int seen_nonphony = 0;
      int seen_tgt = 0;
      struct timespec st_mtim, st_mtimtgt;
      for (auto it = r.deps.begin(); it != r.deps.end(); it++)
      {
        struct stat statbuf;
        if (ruleid_by_tgt.find(*it) != ruleid_by_tgt.end())
        {
          if (rules.at(ruleid_by_tgt[*it]).phony)
          {
            has_to_exec = 1;
            continue;
          }
        }
        if (stat(it->c_str(), &statbuf) != 0)
        {
          perror("can't stat");
          fprintf(stderr, "file was: %s\n", it->c_str());
          abort();
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
    if (has_to_exec)
    {
      std::cout << "do_exec: has_to_exec " << ruleid << std::endl;
      ruleids_to_run.push_back(ruleid);
      r.queued = true;
    }
    else
    {
      std::cout << "do_exec: mark_executed " << ruleid << std::endl;
      r.queued = true;
      mark_executed(ruleid);
    }
  }
}

void consider(int ruleid)
{
  Rule &r = rules.at(ruleid);
  int toexecute = 0;
  std::cout << "considering " << r << std::endl;
  if (r.executed)
  {
    std::cout << "already execed " << r << std::endl;
    return;
  }
  if (r.executing)
  {
    std::cout << "already execing " << r << std::endl;
    return;
  }
  r.executing = true;
  for (auto it = r.deps.begin(); it != r.deps.end(); it++)
  {
    if (ruleid_by_tgt.find(*it) != ruleid_by_tgt.end())
    {
      consider(ruleid_by_tgt[*it]);
      if (!rules.at(ruleid_by_tgt[*it]).executed)
      {
        std::cout << "rule " << ruleid_by_tgt[*it] << " not executed, executing rule " << ruleid << std::endl;
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

void reconsider(int ruleid)
{
  Rule &r = rules.at(ruleid);
  int toexecute = 0;
  std::cout << "reconsidering " << r << std::endl;
  if (r.executed)
  {
    std::cout << "already execed " << r << std::endl;
    return;
  }
  if (!r.executing)
  {
    std::cout << "rule not executing " << r << std::endl;
    return;
  }
  for (auto it = r.deps.begin(); it != r.deps.end(); it++)
  {
    int dep = ruleid_by_tgt[*it];
    if (!rules.at(dep).executed)
    {
      std::cout << "rule " << ruleid_by_tgt[*it] << " not executed, executing rule " << ruleid << std::endl;
      toexecute = 1;
      break;
    }
  }
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
      reconsider(*it2);
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

int main(int argc, char **argv)
{
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
  if (!f)
  {
    std::terminate();
  }
  stiryydoparse(f, &stiryy);
  fclose(f);

  for (auto it = stiryy.rules; it != stiryy.rules + stiryy.rulesz; it++)
  {
    std::vector<std::string> tgt;
    std::vector<std::string> dep;
    std::vector<std::string> cmd;
    std::copy(it->deps, it->deps+it->depsz, std::back_inserter(dep));
    std::copy(it->targets, it->targets+it->targetsz, std::back_inserter(tgt));
    if (tgt.size() > 0) // FIXME chg to if (1)
    {
      std::cout << "ADDING RULE" << std::endl;
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

  better_cycle_detect(0);

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
