import "io.k" as io
import "print.k" as print
import "random.k" as random

export main
main:
  target = random.randint(1, 100)
  std_print("Try to guess the number between 1 and 100!\n")
  game_loop1:
    std_print("Your guess: ")
    guess = io.input_int()
    if_not guess > target goto if1
      std_print("Try Again! Guess lower.\n")
      goto game_loop1
    if1:
    if_not guess < target goto if2
      std_print("Try Again! Guess higher.\n")
      goto game_loop1
    if2:
  std_print("Correct! You win!\n");
  return

export global_test
global_test:
  exists = std_exists(std_globals, "foo")
  if exists != 0 goto already_exists
    std_set_primitive(std_globals, "foo", 1)
  already_exists:
  value = std_get(std_globals, "foo")
  value = value + 1
  std_set_primitive(std_globals, "foo", value)
  print.print2ln("DBG Global --->", value)
  return

export nop
nop:
  return
