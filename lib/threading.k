export new_lock
export acquire_lock
export release_lock

export new_queue
export queue_pop
export queue_push

export atomic_get

new_lock:
  this = std_new_dict()
  return this

acquire_lock:; func_args this
  _while1:
    old = ext_test_and_set(this, "__knickknack_internal_lock")
    if old == 0 goto _while1_break
    std_destroy(old)
    ext_wait(1000)
    goto _while1
  _while1_break:
  return

release_lock:; func_args this
  std_remove(this, "__knickknack_internal_lock")
  ext_notify()
  return

new_queue:
  this = std_new_dict()
  queue = std_new_list()
  std_set(this, "queue", queue)
  lock = new_lock()
  std_set(this, "lock", lock)
  return this

queue_push:; func_args this, item
  lock = std_get(this, "lock")
  queue = std_get(this, queue)
  acquire_lock(lock)
  std_append(queue, item)
  release_lock(lock)
  return

queue_pop:; func_args this
  lock = std_get(this, "lock")
  queue = std_get(this, queue)
  acquire_lock(lock)
  size = std_size(queue)
  if_not size > 0 goto _if2
    i = size - 1
    item = std_get(queue, i)
    std_remove(queue, i)
    return item
  _if2:
  return 0

atomic_get:; func_args object, key_or_index
  acquire_lock(std_global)
  value = std_get(object, key_or_index)
  release_lock(std_global)
  return value
