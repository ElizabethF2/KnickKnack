export keys
export slice

export _keys_itr


keys:; func_args dict
  keys = std_new_list()
  std_for_each(dict, std_this, "_keys_itr")
  return keys

_keys_itr:; func_args key, value
  std_append(keys, key)
  return

slice:; func_args object, begin, end
  result = 0
  kind = std_kind(object)

  # create a new object to hold the result
  if kind == 2 goto _if4
  if kind == 3 goto _if4
  goto _if1
  _if4:
    # object is a string, make a new string
    result = std_new_string()
    if result == 0 goto _slice_end
    goto _if6
  _if1:
  if_not kind == 4 goto _slice_end
    # object is a list, make a new list
    result = std_new_list()
    if result == 0 goto _slice_end
  _if6:

  # clamp end if its bigger than the object's size
  max_end = std_size(object)
  if_not end > max_end goto _if5
    end = max_end
  _if5:

  # iterate through items in the object for the given range
  _slice_loop:
    if begin >= end goto _slice_end
    i = std_get(object, begin)
    kind = std_kind(i)
    
    # append the item to the result  
    if_not kind == 0 goto _if2
      std_append_primitive(result, i)
      goto _if3
    _if2:
      std_append(result, i)
    _if3:
    
    begin = begin + 1
    goto _slice_loop
    
  _slice_end:
  return result
