print("build all: phony dep30000", end=' ')
for n in range(30000):
  print("dep" + str(n), end=' ')
print()
for n in range(30000):
  print("build dep%d: phony dep0" % (n+1))
print("build dep0: phony")
