print("build all: phony dep%d" % (30000,))
for n in range(30000):
  print("build dep%d: phony dep%d" % (n+1, n))
print("build dep0: phony")
