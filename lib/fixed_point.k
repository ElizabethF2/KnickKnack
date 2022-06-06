export set_precision

export from_fraction
export from_string
export to_string
export to_int

export multiply
export divide

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

from_fraction:; func_args n, d
  n = n * ONE
  n = n / d
  return n

to_string:; func_args i
  string = std_new_string()

  # special case for zero
  if_not i == 0 goto _if2
    std_append_primitive(string, 48)   # 48 = '0'
    return string
  _if2:

  length = 0
  TEN = 10 * ONE
  backup_i = i

  # Push the digits to the left of the decimal point to the stack
  _while1:
    digit = i % TEN
    digit = digit / ONE
    push digit
    length = length + 1
    i = i / TEN
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
  i = backup_i % ONE
  len = 0
  _while3:
    i = i * 10
    digit = i / ONE
    digit = digit + 48
    std_append_primitive(string, digit)
    len = len + 1
    if len == 10 goto _break_while3
    i = i % ONE
    if i != 0 goto _while3
  _break_while3:

  return string


from_string:; func_args string
  result = 0
  idx = 0
  len = std_size(string)
  TEN = 10 * ONE

  # Handle the digits before the decimal
  _for1:
    if idx >= len goto _for1_break
    i = std_get(string, idx)
    if i == 46 goto _for1_break   # 46 = '.'
    i = i - 48
    i = i * ONE
    result = result * TEN
    result = result + i
    idx = idx + 1
    goto _for1
  _for1_break:

  idx = idx + 1
  m = ONE / 10

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


to_int:; func_args i, d
  d = from_fraction(1, d)
  i = i / d
  return i

multiply:; func_args a, b
  a = a * b
  a = a / ONE
  return a

divide:; func_args a, b
  a = a * ONE
  a = a / b
  return a
