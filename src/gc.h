#pragma once

#include "common.h"
#include "api.h"
#include "compiler.h"

#include <unordered_map>

extern std::unordered_map<knickknack_object*, bool> g_knickknack_gc_heap;
extern int knickknack_remaining_bytes_until_gc;

void knickknack_gc_mark_in_use(knickknack_object* object);
void knickknack_std_gc(bool remove_scripts);
void knickknack_internal_gc_mark_binary_in_use(knickknack_binary* binary);

void* knickknack_malloc_with_gc(size_t size);
void* knickknack_realloc_with_gc(void* ptr, size_t new_size);
