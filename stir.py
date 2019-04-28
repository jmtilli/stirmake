import asyncio
import os
import sys
import signal

# This file has multi-targets, but not parallelization

global fork_count
fork_count = asyncio.Semaphore(2)

rules = []
rules_by_tgt = {}

limit = 2

async def execseries(cmds):
  global fork_count
  for cmd in cmds:
    await fork_count.acquire()
    try:
      proc = await asyncio.create_subprocess_exec(cmd[0], *cmd[1:])
      stdout, stderr = await proc.communicate()
      if stdout:
        sys.stdout.write(stdout)
      if stderr:
        sys.stderr.write(stderr)
    finally:
      fork_count.release()

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
  async def evaluate(self):
    depchgd = False
    promises = []
    for dep in self.deps:
      if dep in rules_by_tgt:
        promises.append(rules_by_tgt[dep].evaluate())
    for promise in promises:
      await promise
    if self.phony:
      print("execphony: " + repr(self.tgts))
      print("execseries: " + repr(self.cmds))
      await execseries(self.cmds)
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
      await execseries(self.cmds)
    return depchgd


allrule = Rule(["all"], ["a.txt", "b.txt"], [["echo", "foo"]], phony=True)
#rule2 = Rule(["a.txt", "b.txt"], ["c.txt"], [["touch", "a.txt", "b.txt"]])
rule2 = Rule(["a.txt", "b.txt"], ["c.txt"], [["touch", "a.txt"], ["touch", "b.txt"]])

rules.append(allrule)
rules.append(rule2)
rules_by_tgt["all"] = allrule
rules_by_tgt["a.txt"] = rule2
rules_by_tgt["b.txt"] = rule2

#await allrule.evaluate()
#asyncio.run(allrule.evaluate())

loop = asyncio.get_event_loop()
loop.run_until_complete(allrule.evaluate())
