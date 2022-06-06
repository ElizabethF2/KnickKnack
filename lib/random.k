export random
export randint
export choice
export next
export seed
export get_state
export set_state

# An adaptation of TinyMT32 from https://tools.ietf.org/id/draft-ietf-tsvwg-tinymt32-01.html
# Will adapt to any bit size 16 bit or greater but non 32 bit platforms will affect randomness
# Not suitable for cryptographic use

export std_init
std_init:
  mat1 = 36720 << 16
  mat1 = mat1 + 4590
  mat2 = 64632 << 16
  mat2 = mat2 + 65311
  tmat = 14227 << 16
  tmat = tmat + 65023

  i = std_now()
  i = i ^ std_this
  seed(i)

  return


_get_status:; func_args idx
  if_not idx == 0 goto _if2
    return status0
  _if2:
  if_not idx == 1 goto _if3
    return status1
  _if3:
  if_not idx == 2 goto _if4
    return status2
  _if4:
  return status3


_set_status:; func_args idx, v
  if_not idx == 0 goto _if5
    status0 = v
    return
  _if5:
  if_not idx == 1 goto _if6
    status1 = v
    return
  _if6:
  if_not idx == 2 goto _if7
    status2 = v
    return
  _if7:
  status3 = v
  return


seed:; func_args i
  status0 = i
  status1 = mat1
  status2 = mat2
  status3 = tmat

  c = 27655 << 16
  c = c + 35173
  i = 1
  _for2:
    j = i - 1
    j = j % 3
    j = _get_status(j)
    j = j * c
    j = j + i
    k = i - 1
    k = k % 3
    k = _get_status(k)
    k = k >> 30
    j = j ^ k
    k = i % 3
    k = _get_status(k)
    k = k ^ j
    j = i % 3
    _set_status(j, k)
    i = i + 1
    if i < 8 goto _for2

  mask = 32767 << 16
  mask = mask + 65535

  i = 0
  _for3:
    next()
    i = i + 1
    if i < 8 goto _for3

  return


next:
  y = status3
  x = status0
  x = x & mask
  t = status1
  x = x ^ t
  t = status2
  x = x ^ t
  t = x << 1
  x = x ^ t
  t = y >> 1
  t = t ^ x
  y = y ^ t
  status0 = status1
  status1 = status2
  t = y << 10
  status2 = x ^ t
  status3 = y
  t = y & 1
  if_not t goto _if1
    status1 = status1 ^ mat1
    status2 = status2 ^ mat2
  _if1:
  return


random:
  next()
  j = status3
  t = status2 >> 8
  k = status0 + t
  j = j ^ k
  t = k & 1
  t = t * -1
  t = t & tmat
  j = j ^ t
  return j


randint:; func_args lower, upper
  upper = upper + 1
  upper = upper - lower
  r = random()
  r = r % upper
  r = r + lower
  return r


choice:; func_args i
  r = std_size(i)
  r = r - 1
  r = randint(0, r)
  r = std_get(i, r)
  return r


get_state:
  r = std_new_list()
  i = 0
  _for4:
    v = _get_status(i)
    std_append_primitive(r, i, v)
    i = i + 1
    if i < 4 goto _for4
  return r


set_state:; func_args i
  j = 0
  _for5:
    v = std_get(i, j)
    _set_status(j, v)
    j = j + 1
    if j < 4 goto _for5
  return
