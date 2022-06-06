import "print.k" as print
import "http.k" as http
import "json.k" as json
import "hilo.k" as hilo
import "io.k" as io
import "collections.k" as collections
import "convert.k" as convert
import "platform.k" as platform
import "threading.k" as threading

export main
export my_print_int
export this_test
export asm_test

string_io_test:
  std_print("Please enter your name: ")
  name = io.input()
  std_print("Hello ");std_print(name);std_print("!\n")
  return

math_test:
  dbg = 1
  dbg = dbg + 1
  dbg = 2 * dbg
  dbg = 16 / 3
  dbg = 16 % 3
  print.print2ln("Debug Output:", dbg)
  return

loop_test:
  i = 1
  loop1:
    print.print2ln("Iteration", i)
    i = i + 1
    if i < 9 goto loop1
  return

my_print_with_2_args:; func_args str_arg, int_arg
  std_print("MyPrint String: "); std_print(str_arg); std_print("\n")
  std_print("MyPrint Int: "); std_call("print.k", "repr", int_arg); std_print("\n")
  return

my_print_int:; func_args my_int
  std_print("MyPrint Int: "); std_call("print.k", "repr", my_int); std_print("\n")
  return

this_test:
  std_print("This is "); std_print(std_this); std_print("\n");
  return

gc_test:
  i = 0
  loop2:
    junk = std_new_string()
    i = i + 1
    if i < 9999 goto loop2
  return

complex_types_test:
  list = std_new_list()
  d = std_new_dict()
  std_set(d, "foo", "bar")
  res = std_get(d, "foo")
  print.print2ln("Res:", res)
  s = std_new_string()
  std_append_primitive(s, 72)
  std_append_primitive(s, 73)
  print.println(s)
  equal = std_equal(s, "HI")
  if_not equal != 0 goto if1
    std_print("They're equal!\n")
  if1:

  i = 0
  loop3:
    hilo.global_test()
    # std_gc()
    i = i + 1
    if i < 5 goto loop3
  
  return

# This will only work on x86
# Returns the highest cpuid calling parameter as an int primitive
# EAX, ECX & EDX are scratch registers, EBX must be preserved or things will break!
cpuid_max:
  asm 5331C00FA25B
    # push ebx
    # xor eax, eax
    # cpuid
    # pop ebx
  return

cpuid_name:
  name = std_new_string()
  asm 5331C00FA289D85B515250
    # push ebx
    # xor eax, eax
    # cpuid
    # mov eax, ebx
    # pop ebx
    # push ecx
    # push edx
    # push eax
  i = 0
  loop4:
    pop c
    j = 0
    loop5:
      std_append_primitive(name, c)
      c = c / 256
      j = j + 1
      if j < 4 goto loop5
    i = i + 1
    if i < 3 goto loop4
  return name

asm_test:
  supports_x86 = std_call("platform.k", "x86")
  if_not supports_x86 == 1 goto _if1
    max = cpuid_max()
    std_call2("print.k", "print2ln", "Max CPUID:", max)
    name = cpuid_name()
    std_call2("print.k", "print2ln", "Manufacturer Name:", name)
    goto _if2
  _if1: # else
    std_print("Skipping asm_test since this isn't an x86 processor...\n")
  _if2:
  return

lib_test:
  parent = std_new_list()
  i = 1
  _loop1:
    std_append_primitive(parent, i)
    i = i + 1
    if i < 9 goto _loop1
  std_append(parent, "foo")
  
  child = std_new_list()
  std_append(parent, child)
  std_append_primitive(child, 92)
  std_append_primitive(child, 83)

  child = std_new_dict()
  std_append(parent, child)
  std_set(child, "foo", "baz")

  #parent = std_copy("Hello world and all who inhabit it!")
  keys = std_call1("collections.k", "keys", child)
  # parent = std_call3("collections.k", "slice", parent, 3, 99)
  print.println(keys)
  return

socket_test:
  supported = std_call("platform.k", "sockets")
  if_not supported == 1 goto _if3
    http.test()
  _if3:

  std_call1("print.k", "println", "Sockets are unsupported, skipping...")
  return

nop:
  return

benchmark_test:
  nop()
  std_call("hilo.k", "nop")
  
  start = std_now()
  i = 0
  _for1:
    if i == 999999999 goto _for1_break
    nop()
    i = i + 1
    goto _for1
  _for1_break:
  end = std_now()
  time = end - start
  std_call2("print.k", "print2ln", "LOCAL:" time)
  
  #start = std_now()
  #i = 0
  #_for2:
  #  if i == 99999999 goto _for2_break
  #  std_call("hilo.k", "nop")
  #  i = i + 1
  #  goto _for2
  #_for2_break:
  #end = std_now()
  #time = end - start
  #std_call2("print.k", "print2ln", "STD_CALL:" time)
  
  start = std_now()
  i = 0
  _for3:
    if i == 999999999 goto _for3_break
    hilo.nop()
    i = i + 1
    goto _for3
  _for3_break:
  end = std_now()
  time = end - start
  std_call2("print.k", "print2ln", "LAZY:" time)

  return

old_stuff:
  # my_print_with_2_args("Hello World!", 92)
  # string_io_test()
  #math_test()
  #loop_test()
  #my_print_int(777)
  #this_test()
  #gc_test()
  complex_types_test()
  #std_call(std_this, "this_test")
  # hilo_game()
  # lib_test()
  # asm_test()

  # socket_test()
  # http.test("http://www.reddit.com/mtf.json")
  s = platform.sockets()
  print.print2ln("Sockets:", s)

  # s = "{\"x\": 92, \"z\":\"baz\", \"some list\": [2,3,4]}"
  # s = "{\"foo\": true, \"baz\":9, \"c\":\"baz\", \"92\": 2}"
  # obj = json.parse(s)
  # print.print2ln("Result:", obj)

  #std_call("hilo.k", "main")
  #std_gc()

  p = std_platform()
  print.print2ln("Platform:", p)
  
  s = std_new_string()
  std_append_primitive(s, 0)
  std_append_primitive(s, 1)
  std_append_primitive(s, 2)
  std_append_primitive(s, 3)
  sz = std_size(s)
  print.print2ln("S Size:", sz)
  i = std_get(s, 0)
  print.print2ln("S[0]:", i)

  list = std_new_dict()
  i = 0
  _for4:
    if_not i < 99 goto _for4_break
    r = std_randint(0,999)
    r = convert.int_to_string(r)
    std_set_primitive(list, r, i)
    i = i + 1
    goto _for4
  _for4_break:

  #list = collections.slice(list, 5, 33)
  j = json.dump(list)
  print.println(j)

  std_print("Go!\n")
  l = std_new_list()
  i = 0
  _loop2:
    i = std_copy("foobar")
    std_append(l, i)
    i = i + 1
    if i < 999999999 goto _loop2
  std_print("End!\n")
  
  #std_getchar()
  
  return

export thread_worker
thread_worker:
  std_print("Hello!\n")
  return

main:
  t = ext_new_thread(std_this, "thread_worker")
  ext_join_thread(t)
  
  hw = ext_hardware_concurrency()
  print.print2ln("HW:", hw)
  
  lock = threading.new_lock()
  threading.acquire_lock(lock)
  threading.release_lock(lock)
  threading.acquire_lock(lock)

  std_print("OK!\n")
  return
