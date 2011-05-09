import sys
from _read_struc import read_struc
header,structures = read_struc(sys.argv[1])


energies = []
for l1,l2 in structures:
  for ll in l1:
    if ll.startswith("## Energy:"):
      e = float(ll[10:])
      energies.append(e)
assert len(energies) == len(structures)
strucs = zip(range(1,len(structures)+1), energies, structures)

strucs.sort(key=lambda v:v[1])
for h in header: print h
stnr = 0
for st in strucs:
  r,e,s = st
  stnr += 1
  l1,l2 = s
  print "#"+str(stnr)
  print "##"+str(r) + " => sort"
  for l in l1: print l
  for l in l2: print l

