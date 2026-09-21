print("build longdepall: phony d2999")
for n in range(3000):
  deps = []
  for m in range(n):
    deps.append("d%d" % (m,))
  print("build d%d: phony %s " % (n, ' '.join(deps)))
