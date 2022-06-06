export input
export input_int

input:
  buffer = std_new_string()
  if buffer == 0 goto _loop1_break
  _loop1:
    c = std_getchar()
    if c == 10 goto _loop1_break # 10 = \n
    std_append_primitive(buffer, c)
    goto _loop1
  _loop1_break:
  return buffer

input_int:
  i = 0
  _loop2:
    c = std_getchar()
    if c == 10 goto _loop2_break  # 10 = \n
    if c > 57 goto _error1        # 57 = '9'
    if c < 48 goto _error1        # 48 = '0'
    c = c - 48
    i = i * 10
    i = i + c
    goto _loop2
  _loop2_break:
  return i
  _error1:
  return 0
