#pragma once

#include "common.h"
#include "std.h"
#include "api.h"

#include <unordered_set>

static std::unordered_set<char> s_knickknack_internal_atomic_tokens {':', '(', ')', ','};
static std::unordered_set<char> s_knickknack_internal_token_terminators {'+', '/', '*', ' ', '\t'};
static std::unordered_set<char> s_knickknack_internal_line_dividers {'\n', '\r', ';'};
static std::unordered_set<std::string> s_knickknack_internal_math_ops {"+", "-", "*", "/", "%", "|", "&", "^", "<<", ">>", "f+", "f-", "f*", "f/"};
static std::unordered_map<std::string, std::string> s_knickknack_internal_reverse_comp_ops {{"==", "!="}, {"!=","=="}, {"<",">="}, {">", "<="}, {">=", "<"}, {"<=", ">"}};

enum class knickknack_internal_register
{
  RES,     // EAX, r0
  ARG1,    // ECX, r1
  ARG2,    // EDX, r2

  #if defined(__arm__)
  ARG3,    // r3  (ARM only)
  #endif
};

class knickknack_internal_compiler : public knickknack_compiler_feeder
{
  public:
    knickknack_internal_compiler(std::string name);
    bool begin(knickknack_binary* output);
    bool feed_char(char c);
    bool end();
  private:
    bool process_tokens();
    void fprint_token_queue();
    bool cmd_assign();
    bool cmd_label();
    bool cmd_export();
    bool cmd_return();
    bool cmd_import();
    bool cmd_call();
    bool cmd_func_args();
    bool cmd_math();
    bool cmd_if();
    bool cmd_goto();
    bool cmd_push();
    bool cmd_pop();
    bool cmd_asm();

    std::string unescape(std::string str);
    bool validate_symbol(std::string symbol);
    bool isSymbolChar(char c);
    bool isSymbolFirstChar(char c);
    
    bool write_buffer(char byte);
    bool write_buffer_multi(const char* bytes);
    bool write_int_to_buffer(int value);
    bool write_string_literal_to_buffer(std::string str);
    bool write_symbol_address_to_buffer(std::string symbol, bool isLabel);
    
    bool value_to_register(std::string value, knickknack_internal_register reg);
    bool register_to_value(std::string value, knickknack_internal_register reg);
    bool push_register(knickknack_internal_register reg);
    bool pop_to_register(knickknack_internal_register reg);

    #if defined(__arm__)
      bool arm_align_for_32_bit_value();
      bool write_common_arm_load_constant_to_register_code(knickknack_internal_register reg);
      bool arm_special_func_call(int func_addr);
    #endif

    std::string m_name;
    knickknack_binary* m_binary = nullptr;
    size_t m_buffer_length;
    size_t m_buffer_offset;

    std::string m_token_buffer {};
    std::vector<std::string> m_tokens {};
    std::unordered_map<std::string, size_t> m_label_offsets {};
    std::unordered_map<std::string, std::vector<size_t>> m_deferred_label_offsets {};
    std::unordered_map<std::string, std::vector<size_t>> m_deferred_variable_offsets {};
    std::unordered_map<std::string, std::vector<size_t>> m_deferred_literal_offsets {};
    std::vector<std::tuple<size_t, size_t, std::string, std::string>> m_deferred_lazy_symbols {};
    std::unordered_map<std::string, std::string> m_imported_scripts {};

    bool m_in_comment = false;
    bool m_in_string = false;
    bool m_escaped = false;
};

bool knickknack_internal_feed_file(std::string name, knickknack_compiler_feeder* feeder);
knickknack_binary* knickknack_internal_try_load(std::string name);
void knickknack_internal_destroy_binary(knickknack_binary* binary);
void knickknack_internal_overwrite_address(void* address, int value);

#if defined(__X86__)
  void knickknack_internal_lazy_linker_stub();
#elif defined(__arm__)
  void knickknack_internal_lazy_linker_stub(void* stub_addr);
#endif
