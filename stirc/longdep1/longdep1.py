print("longdepall: d2999")
for n in range(3000):
  deps = []
  for m in range(n):
    deps.append("d%d" % (m,))
  print("d%d: %s " % (n, ' '.join(deps)))
