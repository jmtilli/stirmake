print("all: dep30000", end=' ')
for n in range(30000):
  print("dep" + str(n), end=' ')
print()
for n in range(30000):
  print("dep%d: dep0" % (n+1))
print("dep0:")
