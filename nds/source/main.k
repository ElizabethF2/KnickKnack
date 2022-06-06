export main

import "print.k" as print
import "json.k" as json

main:
  i = 0
  asm C046C046
  _while1:
    print.print2ln("Iteration:", i)
    i = i + 1
    if i > 9 goto _while1_break
    asm BEBE
    goto _while1
  _while1_break:
  return 0
