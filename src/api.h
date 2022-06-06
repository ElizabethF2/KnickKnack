#pragma once

#include "common.h"
#include "std.h"
#include "compiler.h"

void knickknack_init();

std::string knickknack_thread_local_name(std::string name);
bool knickknack_script_exists(std::string name);
bool knickknack_entry_point_exists(std::string name, std::string entry_point);

knickknack_binary* knickknack_internal_acquire_script(std::string name);
void knickknack_internal_release_script(knickknack_binary* binary);

void* knickknack_call(std::string name, std::string entry_point);
void* knickknack_call1(std::string name, std::string entry_point, void* arg1);
void* knickknack_call2(std::string name, std::string entry_point, void* arg1, void* arg2);
void* knickknack_call3(std::string name, std::string entry_point, void* arg1, void* arg2, void* arg3);

knickknack_object* knickknack_new_capsule(void (*destructor)(void*), void* data);
void* knickknack_get_data_from_capsule(void (*destructor)(void*), knickknack_object* object);
std::string* knickknack_get_string_from_object(knickknack_object* object);
const std::string* knickknack_get_readonly_string_from_object(knickknack_object* object);
