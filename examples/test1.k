import "random.k" as random
import "print.k" as print
import "convert.k" as convert
import "json.k" as json
import "fixed_point.k" as fp

main3:
  total = 999999
  i = 0
  _for2:
    r = random.randint(0, 100)
    i = i + 1
    if i < total goto _for2
  return

main2:
  #j = json.parse("[1.5]")
  #j = std_get(j, 0)
  print.print2ln("DBG:", j)
  k = std_kind(j)
  print.print2ln("Kind:", k)
  main3()

  print.println("")
  print.println("Finding odd numbers between 1 and 100:")
  i = 1
  _for3:
    print.println(i)
    i = i + 2
    if i <= 100 goto _for3

  return

main:
  i = fp.from_fraction(1,1)
  j = fp.from_string("0.5")
  i = i + j
  i = fp.to_string(i)
  print.println(i)
  return

export main
