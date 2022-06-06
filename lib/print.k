export print
export println

export print2
export print2ln

export print3
export print3ln

export char

export _repr_each_list_item
export _repr_each_dict_item


print:; func_args object
  kind = std_kind(object)

  # Primitive
  if_not kind == 0 goto _if1
    object = std_call1("convert.k", "int_to_string", object)
    std_print(object)
    std_destroy(object)
    return
  _if1:
  
  # String
  if kind == 2 goto _if2
  if kind == 3 goto _if2
  goto _if3
  _if2:
    std_print(object)
    return
  _if3:
  
  # List
  if_not kind == 4 goto _if4
    std_print("[")
    index = 0
    size = std_size(object)
    std_for_each(object, std_this, "_repr_each_list_item")
    std_print("]")
    return
  _if4:
  
  # Dict
  if_not kind == 5 goto _if6
    std_print("{")
    index = 0
    size = std_size(object)
    std_for_each(object, std_this, "_repr_each_dict_item")
    std_print("}")
    return
  _if6:
  
  # Capsule
  if_not kind == 6 goto _if8
    std_print("< capsule ")
    object = std_call1("convert.k", "int_to_string", object)
    std_print(object)
    std_destroy(object)
    std_print(" >")
    return
  _if8:

  return


_repr_each_list_item:; func_args item
  # Print the item
  push index; push size
  print(item)
  pop size; pop index
  
  # Print a comma and space after the item if it's not the last one
  index = index + 1
  if_not index < size goto _if5
    std_print(", ")
  _if5:
  
  return

_repr_each_dict_item:; func_args key, value
  # Print the key
  std_print("\"")
  std_print(key)
  std_print("\": ")
  
  # Print the value
  push index; push size
  print(value)
  pop size; pop index
  
  # Print a comma and space after the item if it's not the last one
  index = index + 1
  if_not index < size goto _if7
    std_print(", ")
  _if7:
  
  return

println:; func_args object
  print(object)
  std_print("\n")
  return

print2:; func_args i1, i2
  print(i1)
  std_print(" ")
  print(i2)
  return
  
print2ln:; func_args i1, i2
  print2(i1, i2)
  std_print("\n")
  return

print3:; func_args i1, i2, i3
  print(i1)
  std_print(" ")
  print(i2)
  std_print(" ")
  print(i3)
  return

print3ln:; func_args i1, i2, i3
  print3(i1, i2, i3)
  std_print("\n")
  return

char:; func_args c
  s = std_new_string()
  std_append_primitive(s, c)
  std_print(s)
  std_destroy(s)
  return
