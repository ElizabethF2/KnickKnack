import "collections.k" as collections

export index
export index_from
export contains
export startswith
export endswith
export split
export split_delim

index:; func_args haystack, needle
  i = index_from(haystack, needle, 0)
  return i

index_from:; func_args haystack, needle, from
  hsize = std_size(haystack)
  nsize = std_size(needle)
  
  hindex = hsize - from
  if_not hindex < nsize goto _if1
    return -1
  _if1:
  
  if_not nsize == 0 goto _if9
    return 0
  _if9:

  hindex = from
  nindex = 0
  _for1:
    if hindex == hsize goto _for1_break

    hchar = std_get(haystack, hindex)
    nchar = std_get(needle, nindex)

    if_not hchar == nchar goto _if2
      nindex = nindex + 1
      if_not nindex == nsize goto _if3
        hindex = hindex - nsize
        hindex = hindex + 1
        return hindex
    _if2: # else
      nindex = 0
    _if3:

    hindex = hindex + 1
    goto _for1
  _for1_break:

  return -1


contains:; func_args haystack, needle
  i = index_from(haystack, needle, 0)
  if_not i == -1 goto _if4
    return 0
  _if4:
    return 1


startswith:; func_args haystack, needle
  hsize = std_size(haystack)
  nsize = std_size(needle)
  
  if_not hsize < nsize goto _if5
    return 0
  _if5:
  
  i = 0
  _for2:
    if i == nsize goto _for2_break
    
    hchar = std_get(haystack, i)
    nchar = std_get(needle, i)
    
    if_not hchar != nchar goto _if6
      return 0
    _if6:
    
    i = i + 1
    goto _for2
  _for2_break:
  
  return 1


endswith:; func_args haystack, needle
  hsize = std_size(haystack)
  nsize = std_size(needle)

  if_not hsize < nsize goto _if7
    return 0
  _if7:
  
  i = 0
  _for3:
    if i == nsize goto _for3_break
    
    hindex = hsize - i
    hchar = std_get(haystack, hindex)
    nindex = nsize - i
    nchar = std_get(needle, nindex)
    
    if_not hchar != nchar goto _if8
      return 0
    _if8:
    
    i = i + 1
    goto _for3
  _for3_break:
  
  return 1

split:; func_args string
  result = split_delim(string, " ")
  return result


split_delim:; func_args string, delim
  result = std_new_list()
  slen = std_size(string)
  dlen = std_size(delim)
  current = 0
  end = slen - 1
  _while1:
    next = index_from(string, delim, current)
    if next == -1 goto _while1_break
    chunk = collections.slice(string, current, next)
    std_append(result, chunk)
    current = next + dlen
    goto _while1
  _while1_break:
  if_not current < end goto _if10
    chunk = collections.slice(string, current, slen)
    std_append(result, chunk)
  _if10:
  return result
