export set_precision

export from_fraction
export from_fraction_with_precision
export from_string
export from_string_with_precision
export to_string
export to_string_with_precision
export to_int
export to_int_with_precision

export multiply
export multiply_with_precision
export divide
export divide_with_precision

export std_init
std_init:
  ONE = 4096
  i = std_exists(std_globals, "_fixed_point_ONE")
  if_not i goto _if1
    ONE = std_get(std_globals, "_fixed_point_ONE")
  _if1:
  return

set_precision:; func_args i
  ONE = i
  std_set_primitive(std_globals, "_fixed_point_ONE", i)
  return

from_fraction_with_precision:; func_args n, d, o
  n = n * o
  n = n / d
  return n

from_fraction:; func_args n, d
  n = from_fraction_with_precision(n, d, ONE)
  return n

to_string_with_precision:; func_args i, o
  string = std_new_string()

  # special case for zero
  if_not i == 0 goto _if2
    std_append_primitive(string, 48)   # 48 = '0'
    return string
  _if2:

  length = 0
  ten = 10 * o
  backup_i = i

  # Push the digits to the left of the decimal point to the stack
  _while1:
    digit = i % ten
    digit = digit / o
    push digit
    length = length + 1
    i = i / ten
    if i > 0 goto _while1
  
  # Pop the digits and append them to the string
  _while2:
    pop digit
    digit = digit + 48   # 48 = '0'
    std_append_primitive(string, digit)
    length = length - 1
    if length != 0 goto _while2

  # Add the decimal point
  std_append_primitive(string, 46)

  # Append digits after the decimal
  i = backup_i % o
  len = 0
  _while3:
    i = i * 10
    digit = i / o
    digit = digit + 48
    std_append_primitive(string, digit)
    len = len + 1
    if len == 10 goto _break_while3
    i = i % o
    if i != 0 goto _while3
  _break_while3:

  return string

to_string_with_precision:; func_args i
  string = to_string(i, ONE)
  return string

from_string_with_precision:; func_args string, o
  result = 0
  idx = 0
  len = std_size(string)
  ten = 10 * o

  # Handle the digits before the decimal
  _for1:
    if idx >= len goto _for1_break
    i = std_get(string, idx)
    if i == 46 goto _for1_break   # 46 = '.'
    i = i - 48
    i = i * o
    result = result * ten
    result = result + i
    idx = idx + 1
    goto _for1
  _for1_break:

  idx = idx + 1
  m = o / 10

  # Handle digits after the decimal
  _for2:
    if idx >= len goto _for2_break
    i = std_get(string, idx)
    i = i - 48
    i = i * m
    result = result + i
    m = m / 10
    idx = idx + 1
    goto _for2
  _for2_break:

  return result

from_string:; func_args string
  result = from_string_with_precision(string, ONE)
  return result

to_int_with_precision:; func_args i, d, o
  d = from_fraction_with_precision(1, d, o)
  i = i / d
  return i

to_int:; func_args i, d
  i = to_int_with_precision(i, d, ONE)
  return i

multiply_with_precision:; func_args a, b, o
  a = a * b
  a = a / o
  return a

multiply:; func_args a, b
  a = multiply_with_precision(a, b, ONE)
  return a

divide_with_precision:; func_args a, b, o
  a = a * o
  a = a / b
  return a

divide:; func_args a, b
  a = divide_with_precision(a, b, ONE)
  return a
