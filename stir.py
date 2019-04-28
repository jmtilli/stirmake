import os
import sys
import signal

# This file has multi-targets, but not parallelization

global fork_count
fork_count = 0

def handler(signum, frame):
  global fork_count
  assert fork_count > 0
  fork_count -= 1

signal.signal(signal.SIGCHLD, handler)

rules = []
rules_by_tgt = {}

limit = 2

def execcmd(cmd):
  global fork_count
  while fork_count > limit:
    signal.pause()
  pid = os.fork()
  if pid == 0:
    os.execvp(cmd[0], cmd)
    sys.exit(1)
  else:
    fork_count += 1

def execseries(cmds):
  global fork_count
  while fork_count > limit:
    signal.pause()
  pid = os.fork()
  if pid == 0:
    signal.signal(signal.SIGCHLD, signal.SIG_DFL)
    for cmd in cmds:
      if os.fork() == 0:
        os.execvp(cmd[0], cmd)
        sys.exit(1)
      else:
        pid,stat = os.wait()
        if stat:
          sys.exit(1)
    sys.exit(0)
  else:
    fork_count += 1

class Rule(object):
  def __init__(self, tgts, deps, cmds, phony=False):
    assert type(tgts) == list
    assert type(deps) == list
    assert type(cmds) == list
    if phony:
      assert len(tgts) == 1
    self.tgts = tgts
    self.deps = deps
    self.cmds = cmds
    self.phony = not not phony
  def evaluate(self):
    depchgd = False
    for dep in self.deps:
      if dep in rules_by_tgt and rules_by_tgt[dep].evaluate():
        depchgd = True
    if self.phony:
      print("execphony: " + repr(self.tgts))
      print("execseries: " + repr(self.cmds))
      execseries(self.cmds)
      #for cmd in self.cmds: # Problem: sequentialization
      #  print("execcmd: " + repr(cmd))
      #  execcmd(cmd)
      return True
    if not depchgd:
      tgtseen = False
      depseen = False
      tgttime = 0
      deptime = 0
      for tgt in self.tgts:
        try:
          mtime = os.stat(tgt).st_mtime
          if not tgtseen or mtime < tgttime:
            tgttime = mtime
          tgtseen = True
        except OSError:
          tgtseen = False
          break
      for dep in self.deps:
        mtime = os.stat(dep).st_mtime
        if not depseen or mtime > deptime:
          deptime = mtime
        depseen = True
      if not tgtseen or (depseen and deptime > tgttime):
        depchgd = True
    if depchgd:
      print("exec: " + repr(self.tgts))
      print("execseries: " + repr(self.cmds))
      execseries(self.cmds)
      #for cmd in self.cmds: # Problem: sequentialization
      #  print("execcmd: " + repr(cmd))
      #  execcmd(cmd)
    return depchgd


allrule = Rule(["all"], ["a.txt", "b.txt"], [["echo", "foo"]], phony=True)
#rule2 = Rule(["a.txt", "b.txt"], ["c.txt"], [["touch", "a.txt", "b.txt"]])
rule2 = Rule(["a.txt", "b.txt"], ["c.txt"], [["touch", "a.txt"], ["touch", "b.txt"]])

rules.append(allrule)
rules.append(rule2)
rules_by_tgt["all"] = allrule
rules_by_tgt["a.txt"] = rule2
rules_by_tgt["b.txt"] = rule2

allrule.evaluate()
