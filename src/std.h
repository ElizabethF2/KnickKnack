#pragma once

#include "common.h"
#include "gc.h"
#include "api.h"

#include <time.h>

const static unsigned int std_error_not_found = 1;
const static unsigned int std_error_invalid_arg = 2;

void knickknack_std_print(knickknack_object* object);
void knickknack_std_print_int(int i);

knickknack_object* knickknack_std_new_string();
knickknack_object* knickknack_std_new_list();
knickknack_object* knickknack_std_new_dict();

knickknack_object* knickknack_internal_new_boxed_primitive(void* value);

unsigned int knickknack_std_size(knickknack_object* object);
void knickknack_std_set(knickknack_object* object, void* key_or_index, knickknack_object* value);
void knickknack_std_set_primitive(knickknack_object* object, void* key_or_index, void* value);
void knickknack_std_append_primitive(knickknack_object* object, void* value);

bool knickknack_std_exists(knickknack_object* object, void* key_or_index);
void* knickknack_std_get(knickknack_object* object, void* key_or_index);
knickknack_kind knickknack_std_kind(knickknack_object* object);

void knickknack_std_destroy(knickknack_object* object);
void knickknack_internal_destroy_shallow(knickknack_object* object);