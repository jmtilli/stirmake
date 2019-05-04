print("all: dep%d" % (30000,))
for n in range(30000):
  print("dep%d: dep%d" % (n+1, n))
print("dep0:")
