import "json.k" as json

export error_exception_thrown
export new
export throw
export matches
export catch
export assert_handled


error_exception_thrown:; return 40399


new:; func_args type
  this = std_new_dict()
  std_set_primitive(this, "exception", 1)
  kind = std_kind(type)
  if kind == 2 goto _is_string
  if kind == 3 goto _is_string
  if kind == 4 goto _is_list
  return this
  _is_string:
    std_set_primitive(this, type, 1)
    return this
  _is_list:
    std_for_each(type, std_this, "_for_each_type")
    return this


export _for_each_type
_for_each_type:; func_args t
  kind = std_kind(t)
  if kind == 2 goto _is_string2
  if kind == 3 goto _is_string2
  return
  _is_string2:
  std_set_primitive(this, t, 1)
  return


throw:; func_args exception
  kind = std_kind(exception)
  if kind != 5 goto _need_to_create_exception
  is_exception = std_exists(exception, "exception")
  if is_exception goto _skip_creating_exception

  _need_to_create_exception:
    exception = new(exception)

  _skip_creating_exception:
  g_current_exception = exception
  errno = error_exception_thrown()
  return


matches:; func_args type
  if_not g_current_exception goto _if1
    should_catch = 0
    kind = std_kind(type)
    if kind == 2 goto _if5
    if kind == 3 goto _if5
    goto _else5
    _if5:
      should_catch = std_exists(g_current_exception, type)
      goto _end5
    _else5:
    if_not kind == 4 goto _end5
      i = 0
      size = std_size(type)
      _for1:
        current_type = std_get(type, i)
        should_catch = std_exists(g_current_exception, current_type)
        if should_catch goto _end5
        i = i + 1
        if i < size goto _for1
    _end5:
    if_not should_catch goto _if1
      exception = g_current_exception
      g_current_exception = 0
      errno = 0
      return exception
  _if1:
  return 0


catch:; func_args type, name, entry_point
  exception = matches(type)
  if_not exception goto _if4
    std_call1(name, entry_point, exception)
  _if4:
  return


assert_handled:
  if_not g_current_exception goto _if2
    error = std_copy("Unhandled exception: ")
    json_string = json.dump(g_current_exception)
    if_not json_string goto _else1
      std_append(error, json_string)
      goto _end1
    _else1:;
      std_append(error, "<Unable to Display>")
    _end1:
    std_fatal_error(error)
  _if2:

  exception_thrown = error_exception_thrown()
  if_not errno == exception_thrown goto _if3
    std_fatal_error("An unhandled exception was thrown but was garbage collected before it could be displayed")
  _if3:

  exception = 0
  return
