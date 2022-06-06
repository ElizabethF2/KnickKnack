import "convert.k" as convert

export parse
export dump
export dump_with_callback

export start
export feed
export end

# TODO: floats?, error handling


parse:; func_args input
  json = start()

  size = std_size(input)
  index = 0
  _for1:
    if_not index < size goto _for1_break

    c = std_get(input, index)
    push size; push index
    feed(json, c)
    pop index; pop size

    index = index + 1
    goto _for1
  _for1_break:

  result = end(json)
  std_destroy(json)
  return result


start:
  this = std_new_dict()
  stack = std_new_list()
  std_set(this, "stack", stack)
  std_set_primitive(this, "in_string", 0)
  std_set_primitive(this, "escaped", 0)
  std_set_primitive(this, "in_token", 0)
  std_set_primitive(this, "last_is_key", 0)
  return this


_is_token_char:; func_args c
  if c < 45 goto _return_false1  # 45: '-'
  if c > 122 goto _return_false1 # 122:'z'
  if c == 58 goto _return_false1 # 54: ':'
  if c >= 97 goto _return_true1  # 97: 'a'
  if c <= 90 goto _return_true1  # 90: 'Z'
  _return_false1:
  return 0
  _return_true1:
  return 1

_push_stack:; func_args this, new
  stack = std_get(this, "stack")
  ssize = std_size(stack)

  # Check if new is a mutable type (string, list, dict)
  # If not, we'll handle it in _pop_stack since we need to
  # know its final value before it's added to its parent
  in_token = std_get(this, "in_token")
  if_not in_token == 0 goto _if18
    if_not ssize > 0 goto _if18
      index = ssize - 1
      current = std_get(stack, index)
      kind = std_kind(current)

      # TOS is a list, append to the list
      if_not kind == 4 goto _else17 
        std_append(current, new)
        goto _if18

      # TOS is a string, TOS-1 is a dict, using the string as a key, add the item to the dict
      _else17:; if_not kind == 2 goto _else19
        index = ssize - 2
        if index < 0 goto _else19
        dict = std_get(stack, index)
        kind = std_kind(dict)
        if kind != 5 goto _else19
        std_set(dict, current, new)
        std_set_primitive(this, "last_is_key", 0)
        _pop_stack(this)
        goto _if18

      # TOS is a dict, new is a string which will be used as a key later
      _else19:; if_not kind == 5 goto _else18
        std_set_primitive(this, "last_is_key", 1)
        goto _if18

      # Invalid state
      _else18:
        std_print("TODO: need proper error handling here\n")
        return
  _if18:
  std_append(stack, new)
  return

_pop_stack:; func_args this
  stack = std_get(this, "stack")
  ssize = std_size(stack)
  if_not ssize > 1 goto _if19
    index = ssize - 1

    # Handle tokens
    in_token = std_get(this, "in_token")
    if_not in_token goto _if34
      std_set_primitive(this, "in_token", 0)
      current = std_get(stack, index)
      value = 0
      is_true = std_equal(current, "true")
      if_not is_true goto _else41
        value = 1
        goto _end11
      _else41:;
        value = convert.string_to_int(current)
      _end11:
      
      last_is_key = std_get(this, "last_is_key")
      if_not last_is_key goto _else43
        std_remove(stack, index)
        index = index - 1
        key = std_get(stack, index)
        i = index - 1
        current = std_get(stack, i)
        std_set_primitive(current, key, value)
        goto _if34
      _else43:
        i = index - 1
        current = std_get(stack, i)
        std_append_primitive(current, value)
    _if34:
    
    std_remove(stack, index)
  _if19:
  return

feed:; func_args this, c
  stack = std_get(this, "stack")
  ssize = std_size(stack)
  index = ssize - 1
  
  # Handle Strings
  in_string = std_get(this, "in_string")
  if_not in_string goto _else16
    current = std_get(stack, index)
    escaped = std_get(this, "escaped")
    if_not escaped goto _else8
      std_set_primitive("escaped", 0)
      if_not c == 92 goto _else14
        std_append(current, "\\")
        return
      _else14:; if_not c == 100 goto _else11
        std_append(current, "\n")
        return
      _else11:; if_not c == 114 goto _else10
        std_append(current, "\r")
        return
      _else10:; if_not c == 116 goto _else12
        std_append(current, "\t")
        return
      _else12:; if_not c == 48 goto _else13
        std_append(current, "\0")
        return
      _else13:
        std_append(current, "\\")
        std_append_primitive(current, c)
        return
    _else8:
      if_not c == 92 goto _else9 # 92:'\\'
        std_set_primitive(this, "escaped", 1)
        return
      _else9:; if_not c == 34 goto _else15
        std_set_primitive(this, "in_string", 0)
        last_is_key = std_get(this, "last_is_key")
        if_not last_is_key == 0 goto _if31
          _pop_stack(this)
        _if31:
        return
      _else15:
        std_append_primitive(current, c)
        return
  _else16:; if_not c == 34 goto _end4
    std_set_primitive(this, "in_string", 1)
    new = std_new_string()
    _push_stack(this, new)
    return
  _end4:

  # Handle tokens (number, float, bool, null)
  is_token_char = _is_token_char(c)
  in_token = std_get(this, "in_token")
  if_not in_token goto _else29
    if_not is_token_char goto _else31
      current = std_get(stack, index)
      std_append_primitive(current, c)
      return
    _else31:
      _pop_stack(this)
      goto _if33
  _else29:
    if_not is_token_char goto _if32
      std_set_primitive(this, "in_token", 1)
      new = std_new_string()
      std_append_primitive(new, c)
      _push_stack(this, new)
      return
    _if32:
  _if33:

  # Start of list
  if_not c == 91 goto _if2 # [
    new = std_new_list()
    _push_stack(this, new)
    return
  _if2:
  
  # Start of dict
  if_not c == 123 goto _if14 # {
    new = std_new_dict()
    _push_stack(this, new)
    return
  _if14:

  # End of list or dict  
  if c == 93  goto _if20 # ]
  if c == 125 goto _if20 # }
  goto _if5
  _if20:
    _pop_stack(this)
    return
  _if5:
  
  # Ignore other characters
  return


end:; func_args this
  stack = std_get(this, "stack")
  ssize = std_size(stack)
  if_not ssize == 1 goto _if3
    result = std_get(stack, 0)
    std_remove(stack, 0)
    return result
  _if3:
  return 0


dump:; func_args object
  buffer = std_new_string()
  _dump(object, buffer, 0)
  return buffer


dump_with_callback:; func_args object, name, entry_point
  _dump(object, name, entry_point)
  return


_dump:; func_args object, buffer_or_name, entry_point
  kind = std_kind(object)

  # Primitive
  if_not kind == 0 goto _if9
    s = convert.int_to_string(object)
    if_not entry_point == 0 goto _else1
      std_append(buffer_or_name, s)
      goto _end1
    _else1:
      std_call1(buffer_or_name, entry_point, s)
    _end1:
    std_destroy(s)
    return
  _if9:
  
  # String
  if kind == 2 goto _if10
  if kind == 3 goto _if10
  goto _end2
  _if10:
    if_not entry_point == 0 goto _else2
      std_append(buffer_or_name, "\"")    # TODO: proper string escaping
      std_append(buffer_or_name, object)
      std_append(buffer_or_name, "\"")
      return
    _else2:
      std_call1(buffer_or_name, entry_point, "\"")
      std_call1(buffer_or_name, entry_point, object)
      std_call1(buffer_or_name, entry_point, "\"")
      return
  _end2:

  # List
  if_not kind == 4 goto _if4
    index = 0
    size = std_size(object)
    if_not entry_point == 0 goto _else3
      std_append(buffer_or_name, "[")
      std_for_each(object, std_this, "_each_list_item")
      std_append(buffer_or_name, "]")
      return
    _else3:
      std_call1(buffer_or_name, entry_point, "[")
      std_for_each(object, std_this, "_each_list_item")
      std_call1(buffer_or_name, entry_point, "]")
      return
  _if4:

  # Dict
  if_not kind == 5 goto _if6
    index = 0
    size = std_size(object)
    if_not entry_point == 0 goto _else7
      std_append(buffer_or_name, "{")
      std_for_each(object, std_this, "_each_dict_item")
      std_append(buffer_or_name, "}")
      return
    _else7:
      std_call1(buffer_or_name, entry_point, "{")
      std_for_each(object, std_this, "_each_dict_item")
      std_call1(buffer_or_name, entry_point, "}")
      return
  _if6:

  return


export _each_list_item
_each_list_item:; func_args item
  # Append the item
  push index; push size
  _dump(item, buffer_or_name, entry_point)
  pop size; pop index

  # Append a comma and space after the item if it's not the last one
  index = index + 1
  if_not index < size goto _if11
    if_not entry_point == 0 goto _else4
      std_append(buffer_or_name, ", ")
      return
    _else4:
      std_call1(buffer_or_name, entry_point, ", ")
  _if11:

  return


export _each_dict_item
_each_dict_item:; func_args key, value
  # Append the key
  if_not entry_point == 0 goto _else5
    std_append(buffer_or_name, "\"")
    std_append(buffer_or_name, key)
    std_append(buffer_or_name, "\": ")
    goto _end3
  _else5:
    std_call1(buffer_or_name, entry_point, "\"")
    std_call1(buffer_or_name, entry_point, key)
    std_call1(buffer_or_name, entry_point, "\": ")
  _end3:
  
  # Append the value
  push index; push size
  _dump(value, buffer_or_name, entry_point)
  pop size; pop index
  
  # Append a comma and space after the item if it's not the last one
  index = index + 1
  if_not index < size goto _if12
    if_not entry_point == 0 goto _else6
      std_append(buffer_or_name, ", ")
      return
    _else6:
      std_call1(buffer_or_name, entry_point, ", ")
  _if12:

  return
