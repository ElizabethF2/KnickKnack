#include "compiler.h"

#ifdef KNICKKNACK_ENABLE_THREADS
  #include "threading.h"
#endif

knickknack_object* g_knickknack_globals = nullptr;
bool (*g_knickknack_loader_hook_exists)(std::string name) = nullptr;
bool (*g_knickknack_loader_hook_feed)(std::string name, knickknack_compiler_feeder* feeder) = nullptr;
FILE* g_knickknack_error_destination = stderr;
std::unordered_map<void*, std::tuple<knickknack_object*, void*, knickknack_object*, knickknack_object*>> g_knickknack_deferred_symbols {};

std::vector<std::string> g_knickknack_search_paths {".", "lib"};

knickknack_binary* knickknack_internal_try_load(std::string name)
{
  auto binary = (knickknack_binary*)knickknack_malloc_with_gc(sizeof(knickknack_binary));
  if (!binary)
  {
    KNICKKNACK_LOG_ERROR("%s: binary allocation failed\n", name.c_str());
    return nullptr;
  }

  auto compiler = knickknack_internal_compiler(name);
  auto succeeded = compiler.begin(binary);
  if (succeeded)
  {
    if (g_knickknack_loader_hook_exists && g_knickknack_loader_hook_feed && g_knickknack_loader_hook_exists(name))
    {
      succeeded = g_knickknack_loader_hook_feed(name, &compiler);
    }
    else
    {
      succeeded = knickknack_internal_feed_file(name, &compiler);
    }

    if (succeeded)
    {
      succeeded = compiler.end();
    }
  }

  if (!succeeded)
  {
    knickknack_internal_destroy_binary(binary);
    return nullptr;
  }

  return binary;
}

bool knickknack_internal_feed_file(std::string name, knickknack_compiler_feeder* feeder)
{
  FILE* fh;
  for (auto path : g_knickknack_search_paths)
  {
    fh = fopen((path + "/" + name).c_str(), "r");
    if (fh)
    {
      break;
    }
  }

  if (!fh)
  {
    KNICKKNACK_LOG_ERROR("%s: not found\n", name.c_str());
    return false;
  }

  char buffer[KNICKKNACK_BLOCK_SIZE];
  while (true)
  {
    auto read = fread(buffer, 1, KNICKKNACK_BLOCK_SIZE, fh);
    if (read < 1)
    {
      fclose(fh);
      return true;
    }

    for (unsigned int i = 0; i < read; ++i)
    {
      if (!feeder->feed_char(buffer[i]))
      {
        fclose(fh);
        return false;
      }
    }
  }
}

knickknack_internal_compiler::knickknack_internal_compiler(std::string name)
{
  m_name = name;
}

bool knickknack_internal_compiler::begin(knickknack_binary* binary)
{
  m_binary = binary;
  m_buffer_length = KNICKKNACK_BLOCK_SIZE;
  m_buffer_offset = 0;
  m_binary->buffer = knickknack_malloc_with_gc(m_buffer_length);
  if (!m_binary->buffer)
  {
    KNICKKNACK_LOG_ERROR("%s: buffer allocation failed\n", m_name.c_str());
    return false;
  }

  new (&m_binary->exports) std::unordered_map<std::string, void*>();
  new (&m_binary->literals) std::unordered_set<knickknack_object*>();

  new (&m_binary->deferred_symbols) std::vector<void*>();
  new (&m_binary->linked_binaries) std::unordered_set<std::string>();
  m_binary->refcount = 0;

  return true;
}

bool knickknack_internal_compiler::feed_char(char c)
{
  auto atomic_token = s_knickknack_internal_atomic_tokens.count(c);
  auto token_terminator = s_knickknack_internal_token_terminators.count(c);
  auto line_divider = s_knickknack_internal_line_dividers.count(c);
  
  if (!m_in_string && c == '#')
  {
    m_in_comment = true;
  }
  
  if (m_in_string)
  {
    m_token_buffer += c;
    if (m_escaped)
    {
      m_escaped = false;
    }
    else if (c == '\\')
    {
      m_escaped = true;
    }
    else if (c == '"')
    {
      m_in_string = false;
      m_tokens.push_back(m_token_buffer);
      m_token_buffer = "";
    }
  }
  else if (!m_in_comment && !line_divider && !isspace(c))
  {
     if (m_token_buffer.size() && (atomic_token || (((isSymbolChar(m_token_buffer[0]) != isSymbolChar(c))) && m_token_buffer[0] != '-')))
     {
       m_tokens.push_back(m_token_buffer);
       m_token_buffer.clear();
     }
     
     // Enqueue non-whitespace characters for the current token 
     m_token_buffer += c;
     if (c == '"')
     {
       m_in_string = true;
     }
  }

  // Push tokens to the list at the end of a token
  if (!m_in_string && (token_terminator || atomic_token) && m_token_buffer.size())
  {
    m_tokens.push_back(m_token_buffer);
    m_token_buffer.clear();
  }

  // Process all of the tokens in the list at the end of a line
  if (line_divider)
  {
    KNICKKNACK_RETURN_IF_FALSE(process_tokens());
    m_in_comment = false;
  }

  return true;
}

bool knickknack_internal_compiler::process_tokens()
{
  // Flush the token buffer
  if (m_token_buffer.size())
  {
    m_tokens.push_back(m_token_buffer);
    m_token_buffer = "";
  }

  auto token_count = m_tokens.size();
  if ((token_count == 3) && (m_tokens[1] == "="))
  {
    KNICKKNACK_RETURN_IF_FALSE(cmd_assign());
  }
  else if ((token_count == 2) && (m_tokens[1] == ":"))
  {
    KNICKKNACK_RETURN_IF_FALSE(cmd_label());
  }
  else if ((token_count == 1 || token_count == 2) && m_tokens[0] == "return")
  {
    KNICKKNACK_RETURN_IF_FALSE(cmd_return());
  }
  else if ((token_count == 2) && m_tokens[0] == "export")
  {
    KNICKKNACK_RETURN_IF_FALSE(cmd_export());
  }
  else if (((token_count > 2) && (m_tokens[1] == "(")) || ((token_count > 3) && (m_tokens[3] == "(")))
  {
    KNICKKNACK_RETURN_IF_FALSE(cmd_call());
  }
  else if ((token_count == 5) && (m_tokens[1] == "=") && s_knickknack_internal_math_ops.count(m_tokens[3]))
  {
    KNICKKNACK_RETURN_IF_FALSE(cmd_math());
  }
  else if (((token_count == 6) && ((m_tokens[0] == "if") || (m_tokens[0] == "if_not")) && (m_tokens[4] == "goto") && s_knickknack_internal_reverse_comp_ops.count(m_tokens[2])) ||
           ((token_count == 4) && ((m_tokens[0] == "if") || (m_tokens[0] == "if_not")) && (m_tokens[2] == "goto")))
  {
    KNICKKNACK_RETURN_IF_FALSE(cmd_if());
  }
  else if ((token_count == 2) && (m_tokens[0] == "goto"))
  {
    KNICKKNACK_RETURN_IF_FALSE(cmd_goto());
  }
  else if ((token_count > 1) && (m_tokens[0] == "func_args"))
  {
    KNICKKNACK_RETURN_IF_FALSE(cmd_func_args());
  }
  else if ((token_count == 2) && (m_tokens[0] == "push"))
  {
    KNICKKNACK_RETURN_IF_FALSE(cmd_push());
  }
  else if ((token_count == 2) && (m_tokens[0] == "pop"))
  {
    KNICKKNACK_RETURN_IF_FALSE(cmd_pop());
  }
  else if ((token_count == 2) && (m_tokens[0] == "asm"))
  {
    KNICKKNACK_RETURN_IF_FALSE(cmd_asm());
  }
  else if ((token_count == 4) && (m_tokens[0] == "import") && (m_tokens[2] == "as"))
  {
    KNICKKNACK_RETURN_IF_FALSE(cmd_import());
  }
  else if (token_count > 0)
  {
    KNICKKNACK_LOG_ERROR("%s: syntax error at:\n  ", m_name.c_str());
    fprint_token_queue();
    return false;
  }

  m_tokens.clear();
  return true;
}

void knickknack_internal_compiler::fprint_token_queue()
{
  for (const auto i : m_tokens)
  {
    KNICKKNACK_LOG_ERROR("%s ", i.c_str());
  }
  KNICKKNACK_LOG_ERROR("%s\n", "");
}

#if defined(__arm__)
  bool knickknack_internal_compiler::arm_align_for_32_bit_value()
  {
    if (m_buffer_offset % 4 != 0)
    {
      return write_buffer_multi("C046"); // NOP for padding
    }
    return true;
  }

  bool knickknack_internal_compiler::write_common_arm_load_constant_to_register_code(knickknack_internal_register reg)
  {
    KNICKKNACK_RETURN_IF_FALSE(arm_align_for_32_bit_value());

    KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x00));
    switch(reg)
    {
      case knickknack_internal_register::RES:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x48));
        break;
      case knickknack_internal_register::ARG1:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x49));
        break;
      case knickknack_internal_register::ARG2:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x4A));
        break;
      case knickknack_internal_register::ARG3:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x4B));
        break;
    }

    return write_buffer_multi("01E0");
  }

  int knickknack_internal_division_wrapper(int a, int b)
  {
    return a / b;
  }

  int knickknack_internal_modulo_wrapper(int a, int b)
  {
    return a % b;
  }

  // Wraps the operations that don't have corresponding opcodes and that
  // are implemented in software so that KnickKnack has an address to link to
  // Most optimizing compilers should replace the wrappers with the address of
  // their implementation
  // Also used to generate calls to the lazy linker stub
  // TODO: need to optimize for KNICKKNACK_ARM_WITH_4MB_OR_LESS_RAM
  bool knickknack_internal_compiler::arm_special_func_call(int func_addr)
  {
    // Store the address to ARG3, set LSB so we stay in THUMB
    KNICKKNACK_RETURN_IF_FALSE(write_common_arm_load_constant_to_register_code(knickknack_internal_register::ARG3));
    KNICKKNACK_RETURN_IF_FALSE(write_int_to_buffer((func_addr|1)));

    // Push LR, call ARG3, pop LR to ARG3, move ARG3 back to LR
    return write_buffer_multi("00B5984708BC9E46");
  }
#endif

bool knickknack_internal_compiler::cmd_import()
{
  auto script = unescape(m_tokens[1]);
  auto alias = m_tokens[3];
  
  KNICKKNACK_RETURN_IF_FALSE(validate_symbol(alias));
  m_imported_scripts[alias] = script;
  
  return true;
}

bool knickknack_internal_compiler::cmd_push()
{
  KNICKKNACK_RETURN_IF_FALSE(value_to_register(m_tokens[1], knickknack_internal_register::RES));
  return push_register(knickknack_internal_register::RES);
}

bool knickknack_internal_compiler::cmd_pop()
{
  KNICKKNACK_RETURN_IF_FALSE(pop_to_register(knickknack_internal_register::RES));
  return register_to_value(m_tokens[1], knickknack_internal_register::RES);
}

bool knickknack_internal_compiler::cmd_asm()
{
  return write_buffer_multi(m_tokens[1].c_str());
}

bool knickknack_internal_compiler::cmd_func_args()
{
  // Filter the args
  auto size = m_tokens.size();
  std::vector<std::string> args;
  for (unsigned int i = 1; i < size; ++i)
  {
    if (m_tokens[i].size() > 0 && !s_knickknack_internal_atomic_tokens.count(m_tokens[i][0]))
    {
      args.push_back(m_tokens[i]);
    }
  }

  // Figure out how many arguments are in registers
  #if defined(__X86__)
    const unsigned int c_reg_args = 0;
  #elif defined(__arm__)
    const unsigned int c_reg_args = 4;
  #endif

  #if defined(__arm__)
    // Need to save ARG3 first since it will be used as scratch to load the other args
    if (args.size() >= 4)
    {
      KNICKKNACK_RETURN_IF_FALSE(register_to_value(args[3], knickknack_internal_register::ARG3));
    }
    if (args.size() >= 3)
    {
      KNICKKNACK_RETURN_IF_FALSE(register_to_value(args[2], knickknack_internal_register::ARG2));
    }
    if (args.size() >= 2)
    {
      KNICKKNACK_RETURN_IF_FALSE(register_to_value(args[1], knickknack_internal_register::ARG1));
    }
    if (args.size() >= 1)
    {
      KNICKKNACK_RETURN_IF_FALSE(register_to_value(args[0], knickknack_internal_register::RES));
    }
  #endif

  // Load any remaining args from the stack
  auto arg_count = args.size();
  if (arg_count > c_reg_args)
  {
    for (unsigned int i = 0; i < arg_count; ++i)
    {
      // Load from SP + i to RES
      #if defined(__X86__)
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("8B4424"));
        KNICKKNACK_RETURN_IF_FALSE(write_buffer((i+1)<<2));
      #elif defined(__arm__)
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(i));
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x98));
      #endif
      
      KNICKKNACK_RETURN_IF_FALSE(register_to_value(args[i+c_reg_args], knickknack_internal_register::RES));
    }
  }

  return true;
}

bool knickknack_internal_compiler::cmd_goto()
{
  #if defined(__arm__)
    // Check if this is a known label
    // If it is and it's close enough, we can optimize this to 2 bytes
    if (m_label_offsets.find(m_tokens[1]) != m_label_offsets.end())
    {
      int delta = m_label_offsets[m_tokens[1]] - m_buffer_offset;
      if (delta >= -2048)
      {
        // Use the short, 2 byte instruction
        //int16_t instruction = (((delta-4)>>1) & 0x07F) | 0xE000;
        //KNICKKNACK_RETURN_IF_FALSE(write_buffer(((char*)&instruction)[]));
        //return write_buffer(((char*)&instruction)[0]);
      }
    }

    // Can't optimize, need to use 8 bytes
    KNICKKNACK_RETURN_IF_FALSE(arm_align_for_32_bit_value());
    KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("00480047"));
    
  #elif defined(__X86__)
    KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xE9));
  #endif

  return write_symbol_address_to_buffer(m_tokens[1], true /* isLabel */);
}

bool knickknack_internal_compiler::cmd_if()
{
  std::string op, label;
  auto is_bool = (m_tokens[2] == "goto");
  if (is_bool)
  {
    op = "!=";
    label = m_tokens[3];
  }
  else
  {
    op = m_tokens[2];
    label = m_tokens[5];
  }
  
  // Load values to registers
  KNICKKNACK_RETURN_IF_FALSE(value_to_register(m_tokens[1], knickknack_internal_register::RES));
  if (is_bool)
  {
    KNICKKNACK_RETURN_IF_FALSE(value_to_register("0", knickknack_internal_register::ARG1));
  }
  else
  {
    KNICKKNACK_RETURN_IF_FALSE(value_to_register(m_tokens[3], knickknack_internal_register::ARG1));
  }

  // Compare the values
  #if defined(__X86__)
    KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("39C8"));
  #elif defined(__arm__)
    KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("8842"));
  #endif

  // Reverse the op if this is an if_not
  if (m_tokens[0] == "if_not")
  {
    op = s_knickknack_internal_reverse_comp_ops[op];
  }

  #if defined(__X86__)
    // Conditional jump
    KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x0F));
    if (op == "==")
    {
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x84));
    }
    else if (op == "!=")
    {
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x85));
    }
    else if (op == "<")
    {
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x8C));
    }
    else if (op == ">")
    {
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x8F));
    }
    else if (op == "<=")
    {
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x8E));
    }
    else if (op == ">=")
    {
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x8D));
    }

    return write_symbol_address_to_buffer(label, true /* isLabel */);
  #elif defined(__arm__)
    // Check if the target address is within -256 bytes
    // If it isn't, place an unconditional jump to the target
    // that will be skipped if the condition ISN'T true
    bool can_optimize = false;
    int delta = 0;
    if (m_label_offsets.find(label) != m_label_offsets.end())
    {
      delta = m_label_offsets[label] - m_buffer_offset;
      if (delta >= -256)
      {
        can_optimize = true;
      }
      
      // TODO: handle 2046 delta special case
    }

    if (!can_optimize)
    {
      delta = 8;
      op = s_knickknack_internal_reverse_comp_ops[op];
    }

    KNICKKNACK_RETURN_IF_FALSE(write_buffer((int8_t)((delta-4)>>1)));
    if (op == "==")
    {
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xD0));
    }
    else if (op == "!=")
    {
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xD1));
    }
    else if (op == "<")
    {
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xDB));
    }
    else if (op == ">")
    {
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xDC));
    }
    else if (op == "<=")
    {
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xDD));
    }
    else if (op == ">=")
    {
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xDA));
    }

    if (!can_optimize)
    {
      // Load to r0, jump to r0
      KNICKKNACK_RETURN_IF_FALSE(arm_align_for_32_bit_value());
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("00480047"));
      KNICKKNACK_RETURN_IF_FALSE(write_symbol_address_to_buffer(label, true /* isLabel */));
    }

    return true;
  #endif
}

bool knickknack_internal_compiler::cmd_math()
{
  KNICKKNACK_RETURN_IF_FALSE(value_to_register(m_tokens[2], knickknack_internal_register::RES));
  KNICKKNACK_RETURN_IF_FALSE(value_to_register(m_tokens[4], knickknack_internal_register::ARG1));

  if (m_tokens[3] == "+")
  {
    #if defined(__X86__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("01C8"));
    #elif defined(__arm__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("4018"));
    #endif
  }
  else if (m_tokens[3] == "-")
  {
    #if defined(__X86__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("29C8"));
    #elif defined(__arm__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("401A"));
    #endif
  }
  else if (m_tokens[3] == "*")
  {
    #if defined(__X86__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("F7E1"));
    #elif defined(__arm__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("4843"));
    #endif
  }
  else if (m_tokens[3] == "/")
  {
    #if defined(__X86__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("31D2F7F1"));
    #elif defined(__arm__)
      KNICKKNACK_RETURN_IF_FALSE(arm_special_func_call((int)knickknack_internal_division_wrapper));
      return register_to_value(m_tokens[0], knickknack_internal_register::RES);
    #endif
  }
  else if (m_tokens[3] == "%")
  {
    #if defined(__X86__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("31D2F7F1"));
      return register_to_value(m_tokens[0], knickknack_internal_register::ARG2);
    #elif defined(__arm__)
      KNICKKNACK_RETURN_IF_FALSE(arm_special_func_call((int)knickknack_internal_modulo_wrapper));
      return register_to_value(m_tokens[0], knickknack_internal_register::RES);
    #endif
  }
  else if (m_tokens[3] == "|")
  {
    #if defined(__X86__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("09C8"));
    #elif defined(__arm__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("0843"));
    #endif
  }
  else if (m_tokens[3] == "&")
  {
    #if defined(__X86__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("21C8"));
    #elif defined(__arm__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("0840"));
    #endif
  }
  else if (m_tokens[3] == "^")
  {
    #if defined(__X86__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("31C8"));
    #elif defined(__arm__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("4840"));
    #endif
  }
  else if (m_tokens[3] == "<<")
  {
    #if defined(__X86__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("D3E0"));
    #elif defined(__arm__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("8840"));
    #endif
  }
  else if (m_tokens[3] == ">>")
  {
    #if defined(__X86__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("D3F8"));
    #elif defined(__arm__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("C840"));
    #endif
  }

  return register_to_value(m_tokens[0], knickknack_internal_register::RES);
}

bool knickknack_internal_compiler::cmd_assign()
{
  KNICKKNACK_RETURN_IF_FALSE(value_to_register(m_tokens[2], knickknack_internal_register::ARG1));
  return register_to_value(m_tokens[0], knickknack_internal_register::ARG1);
}

bool knickknack_internal_compiler::cmd_label()
{
  KNICKKNACK_RETURN_IF_FALSE(validate_symbol(m_tokens[0]));
  if ((m_label_offsets.find(m_tokens[0]) != m_label_offsets.end()) || (g_knickknack_builtins.find(m_tokens[0]) != g_knickknack_builtins.end()))
  {
    KNICKKNACK_LOG_ERROR("%s: duplicate declaration %s\n", m_name.c_str(), m_tokens[0].c_str());
    return false;
  }

  #if defined(__arm__)
    KNICKKNACK_RETURN_IF_FALSE(arm_align_for_32_bit_value());
  #endif

  m_label_offsets[m_tokens[0]] = m_buffer_offset;

  return true;
}

bool knickknack_internal_compiler::cmd_export()
{
  m_binary->exports[m_tokens[1]] = nullptr;
  return true;
}

bool knickknack_internal_compiler::cmd_return()
{
  if (m_tokens.size() == 2)
  {
    KNICKKNACK_RETURN_IF_FALSE(value_to_register(m_tokens[1], knickknack_internal_register::RES));
  }

  #if   defined(__X86__)
    return write_buffer(0xC3);
  #elif defined(__arm__)
    return write_buffer_multi("7047");
  #endif
}

bool knickknack_internal_compiler::cmd_call()
{
  // Get the symbol to be called
  auto save_result = (m_tokens[1] == "=");
  auto symbol = m_tokens[save_result ? 2 : 0];

  // Check if the symbol is a linked (imported) symbol
  size_t stub_offset;
  auto dot_pos = symbol.find(".");
  auto is_imported = (dot_pos != std::string::npos);
  if (is_imported)
  {
    // If it is, write the opcode for a call to the lazy linker
    // This will be replaced with a NOP slide once the function is initialized
    #if defined(__X86__)
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xE8));
      stub_offset = m_buffer_offset;
      KNICKKNACK_RETURN_IF_FALSE(write_int_to_buffer(0));
    #elif defined(__arm__)
      KNICKKNACK_RETURN_IF_FALSE(write_common_arm_load_constant_to_register_code(knickknack_internal_register::RES));
      stub_offset = m_buffer_offset;
      KNICKKNACK_RETURN_IF_FALSE(write_int_to_buffer(0));
      KNICKKNACK_RETURN_IF_FALSE(arm_special_func_call((int)knickknack_internal_lazy_linker_stub));
    #endif
  }

  // Filter the args
  auto end = m_tokens.size() - 1;
  std::vector<std::string> args;
  for (unsigned int i = (save_result ? 3 : 1); i < end; ++i)
  {
    auto value = m_tokens[i];
    if (value.size() > 0 && !s_knickknack_internal_atomic_tokens.count(value[0]))
    {
      args.push_back(value);
    }
  }

  // Figure out how many arguments will be stored in registers
  #if defined(__X86__)
    const unsigned int c_reg_args = 0;
  #elif defined(__arm__)
    const unsigned int c_reg_args = 4;
  #endif

  // If there are too many args to fit in registers, push them to the stack
  int stack_arg_count = args.size() - c_reg_args;
  if (stack_arg_count > 0)
  {
    int i = args.size() - 1;
    do
    {
      KNICKKNACK_RETURN_IF_FALSE(value_to_register(args[i], knickknack_internal_register::ARG1));
      KNICKKNACK_RETURN_IF_FALSE(push_register(knickknack_internal_register::ARG1));
    }
    while((--i) >= (int)c_reg_args);
  }
  else
  {
    stack_arg_count = 0;
  }

  #if defined(__X86__)
    // Write the call opcode for the actual call
    KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xE8));
  #elif defined(__arm__)
    // Write the code to load the address into RES
    // TODO: skip this if the address is close enough we can optimize
    KNICKKNACK_RETURN_IF_FALSE(write_common_arm_load_constant_to_register_code(knickknack_internal_register::RES));    
  #endif

  if (is_imported)
  {
    // If this is an imported symbol, store the symbol info for later
    auto alias = symbol.substr(0, dot_pos);
    auto entry_point = symbol.substr(dot_pos+1);
    KNICKKNACK_RETURN_IF_FALSE(validate_symbol(entry_point));
    if (m_imported_scripts.find(alias) == m_imported_scripts.end())
    {
      KNICKKNACK_LOG_ERROR("%s: unimported alias %s\n", m_name.c_str(), alias.c_str());
      return false;
    }
    m_deferred_lazy_symbols.push_back({stub_offset, m_buffer_offset, m_imported_scripts[alias], entry_point});

    // Write a placeholder address
    // This will be replaced  with an offset to the lazy linker stub later
    KNICKKNACK_RETURN_IF_FALSE(write_int_to_buffer(0));
  }
  else
  {
    KNICKKNACK_RETURN_IF_FALSE(write_symbol_address_to_buffer(symbol, true /* isLabel */));
  }

  #if defined(__arm__)
    // Copy the function address from RES to IP
    KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("8446"));

    // Load any args that will fit in registers
    if (args.size() >= 1)
    {
      KNICKKNACK_RETURN_IF_FALSE(value_to_register(args[0], knickknack_internal_register::RES));
    }
    if (args.size() >= 2)
    {
      KNICKKNACK_RETURN_IF_FALSE(value_to_register(args[1], knickknack_internal_register::ARG1));
    }
    if (args.size() >= 3)
    {
      KNICKKNACK_RETURN_IF_FALSE(value_to_register(args[2], knickknack_internal_register::ARG2));
    }
    if (args.size() >= 4)
    {
      KNICKKNACK_RETURN_IF_FALSE(value_to_register(args[3], knickknack_internal_register::ARG3));
    }

    // Save LR, call IP and restore LR (uses r3 for scratch)
    KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("00B5E04708BC9E46"));
  #endif

  // Save the result if a destination was specified
  if (save_result)
  {
    KNICKKNACK_RETURN_IF_FALSE(register_to_value(m_tokens[0], knickknack_internal_register::RES));
  }

  // Pop the arguments
  #if defined(__X86__)
    if (stack_arg_count > 0)
    {
      KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("83C4"));
      KNICKKNACK_RETURN_IF_FALSE(write_buffer(stack_arg_count<<2));
    }
  #elif defined(__arm__)
    for (int i = 0; i < stack_arg_count; ++i)
    {
      KNICKKNACK_RETURN_IF_FALSE(pop_to_register(knickknack_internal_register::ARG1));
    }
  #endif

  return true;
}

bool knickknack_internal_compiler::isSymbolChar(char c)
{
  return (isalnum(c) || c == '_' || c == '.');
}

bool knickknack_internal_compiler::isSymbolFirstChar(char c)
{
  return isSymbolChar(c) && !isdigit(c);
}

bool knickknack_internal_compiler::validate_symbol(std::string symbol)
{
  if (symbol.size() < 1)
  {
    KNICKKNACK_LOG_ERROR("%s: missing symbol at:\n  ", m_name.c_str());
    fprint_token_queue();
    return false;
  }
  if (!isSymbolFirstChar(symbol[0]))
  {
    KNICKKNACK_LOG_ERROR("%s: symbol name can't start with digit %s\n", m_name.c_str(), symbol.c_str());
    return false;
  }
  for (const char c : symbol)
  {
    if (!isSymbolChar(c))
    {
      KNICKKNACK_LOG_ERROR("%s: invalid symbol %s\n", m_name.c_str(), symbol.c_str());
      return false;
    }
  }
  return true;
}

bool knickknack_internal_compiler::write_buffer(char byte)
{
  if (m_buffer_offset == m_buffer_length)
  {
    m_buffer_length += KNICKKNACK_BLOCK_SIZE;
    m_binary->buffer = knickknack_realloc_with_gc(m_binary->buffer, m_buffer_length);
    if (!m_binary->buffer)
    {
      KNICKKNACK_LOG_ERROR("%s: buffer increase failed\n", m_name.c_str());
      return false;
    }
  }
  
  auto buffer = (char*)m_binary->buffer;
  buffer[m_buffer_offset++] = byte;
  return true;
}

bool knickknack_internal_compiler::write_buffer_multi(const char* bytes)
{
  const char* current = bytes;
  while(*current != 0)
  {
    char c = 0;
    
    for (unsigned int i = 0; i < 2; ++i)
    {
      c <<= 4;
      char h = current[i];
      if (h >= 'A' && h <= 'F')
      {
        c += h - 'A' + 10;
      }
      else if (h >= 'a' && h <= 'f')
      {
        c += h - 'a' + 10;
      }
      else if (h >= '0' && h <= '9')
      {
        c += h - '0';
      }
      else
      {
        KNICKKNACK_LOG_ERROR("%s: invalid hex string %s\n", m_name.c_str(), bytes);
        return false;
      }
    }

    KNICKKNACK_RETURN_IF_FALSE(write_buffer(c));
    current += 2;
  }

  return true;
}

bool knickknack_internal_compiler::write_int_to_buffer(int value)
{
  // Use int as array to ensure correct endianness is used
  auto arr = (char*)(&value);
  for (unsigned int i = 0; i < KNICKKNACK_VALUE_SIZE; ++i)
  {
    KNICKKNACK_RETURN_IF_FALSE(write_buffer(arr[i]));
  }
  return true;
}

std::string knickknack_internal_compiler::unescape(std::string str)
{
  auto s = str.substr(1,str.size()-2);
  std::string unescaped = "";
  bool escaped = false;
  
  for (const auto c : s)
  {
    if (escaped)
    {
      switch(c)
      {
        case 'n': unescaped += '\n'; break;
        case 'r': unescaped += '\r'; break;
        case 't': unescaped += '\t'; break;
        case '0': unescaped += '\0'; break;
        case '\\': unescaped += '\\'; break;
        default:
          unescaped += c;
          break;
      }
      escaped = false;
    }
    else if (c == '\\')
    {
      escaped = true;
    }
    else
    {
      unescaped += c;
    }
  }
  
  return unescaped;
}

bool knickknack_internal_compiler::write_string_literal_to_buffer(std::string str)
{
  m_deferred_literal_offsets[unescape(str)].push_back(m_buffer_offset);
  return write_int_to_buffer(0);
}

bool knickknack_internal_compiler::write_symbol_address_to_buffer(std::string symbol, bool isLabel)
{
  KNICKKNACK_RETURN_IF_FALSE(validate_symbol(symbol));
  
  if (isLabel)
  {
    m_deferred_label_offsets[symbol].push_back(m_buffer_offset);
    return write_int_to_buffer(0);
  }
  else
  {
    if (g_knickknack_builtins.find(symbol) != g_knickknack_builtins.end())
    {
      return write_int_to_buffer((int)g_knickknack_builtins[symbol]);
    }
    else
    {
      m_deferred_variable_offsets[symbol].push_back(m_buffer_offset);
      return write_int_to_buffer(0);
    }
  }
}

bool knickknack_internal_compiler::value_to_register(std::string value, knickknack_internal_register reg)
{
  auto c = value[0];
  auto digit = isdigit(c) || c == '-';
  auto string_literal = (c == '"');

  if (digit || string_literal)
  {
    #if defined(__X86__)
      switch(reg)
      {
        case knickknack_internal_register::RES:
          KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xB8));
          break;
        case knickknack_internal_register::ARG1:
          KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xB9));
          break;
        case knickknack_internal_register::ARG2:
          KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xBA));
          break;
      }
      
      if (digit)
      {
        return write_int_to_buffer(std::stoi(value));
      }
      else
      {
        return write_string_literal_to_buffer(value);
      }
    #elif defined(__arm__)
      int v;
      
      if (digit)
      {
        v = std::stoi(value);
      }
      
      if (digit && (v >= 0) && (v < 256))
      {
        // Small constant
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(v));
        switch(reg)
        {
          case knickknack_internal_register::RES:
            KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x20));
            break;
          case knickknack_internal_register::ARG1:
            KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x21));
            break;
          case knickknack_internal_register::ARG2:
            KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x22));
            break;
          case knickknack_internal_register::ARG3:
            KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x23));
            break;
        }
      }
      else
      {
        // Large constant
        write_common_arm_load_constant_to_register_code(reg);
        
        if (digit)
        {
          KNICKKNACK_RETURN_IF_FALSE(write_int_to_buffer(v));
        }
        else
        {
          return write_string_literal_to_buffer(value);
        }
      }
    #endif
  }
  else if (isSymbolChar(c))
  {
    #if defined(__X86__)
      switch(reg)
      {
        case knickknack_internal_register::RES:
          KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xA1));
          break;
        case knickknack_internal_register::ARG1:
          KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("8B0D"));
          break;
        case knickknack_internal_register::ARG2:
          KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("8B15"));
          break;
      }

      KNICKKNACK_RETURN_IF_FALSE(write_symbol_address_to_buffer(value, false /* isLabel */));
    #elif defined(__arm__)
      // Load symbol address to reg
      KNICKKNACK_RETURN_IF_FALSE(write_common_arm_load_constant_to_register_code(reg));
      KNICKKNACK_RETURN_IF_FALSE(write_symbol_address_to_buffer(value, false /* isLabel */));
      
      // Dereference the constant
      switch(reg)
      {
        case knickknack_internal_register::RES:
          KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("0068"));
          break;
        case knickknack_internal_register::ARG1:
          KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("0968"));
          break;
        case knickknack_internal_register::ARG2:
          KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("1268"));
          break;
        case knickknack_internal_register::ARG3:
          KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("1B68"));
          break;
      }
    #endif
  }
  else
  {
    KNICKKNACK_LOG_ERROR("%s: Unhandled case %s\n", m_name.c_str(), value.c_str());
    return false;
  }
  
  return true;
}

// NOTE: On ARM, this should only be used with ARG3 (r3) when setting up a function call
//       ARG3 should be saved first as r3 will be used for scratch with all other values
bool knickknack_internal_compiler::register_to_value(std::string value, knickknack_internal_register reg)
{
  #if defined(__X86__)

    switch(reg)
    {
      case knickknack_internal_register::RES:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(0xA3));
        break;
      case knickknack_internal_register::ARG1:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("890D"));
        break;
      case knickknack_internal_register::ARG2:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("8915"));
        break;
    }
    
    return write_symbol_address_to_buffer(value, false /* isLabel */);

  #elif defined(__arm__)

    // Pick another register to temporarily store the address
    auto temp_addr_reg = knickknack_internal_register::ARG3;
    if (reg == temp_addr_reg)
    {
      temp_addr_reg = knickknack_internal_register::ARG2;
    }

    if (reg == knickknack_internal_register::ARG3)
    {
      // Save the temp register's original value
      KNICKKNACK_RETURN_IF_FALSE(push_register(temp_addr_reg));
    }

    // Load symbol address to the temp register
    KNICKKNACK_RETURN_IF_FALSE(write_common_arm_load_constant_to_register_code(temp_addr_reg));
    KNICKKNACK_RETURN_IF_FALSE(write_symbol_address_to_buffer(value, false /* isLabel */));

    // Store the value: *temp_addr_reg = reg
    switch(reg)
    {
      case knickknack_internal_register::RES:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("1860"));
        break;
      case knickknack_internal_register::ARG1:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("1960"));
        break;
      case knickknack_internal_register::ARG2:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("1A60"));
        break;
      case knickknack_internal_register::ARG3:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("1360"));
        break;
    }

    if (reg == knickknack_internal_register::ARG3)
    {
      // Restore the original value of the temp register
      return pop_to_register(temp_addr_reg);
    }
    
    return true;

  #endif
}

bool knickknack_internal_compiler::push_register(knickknack_internal_register reg)
{
  #if defined(__X86__)
    switch(reg)
    {
      case knickknack_internal_register::RES:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x50));
        break;
      case knickknack_internal_register::ARG1:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x51));
        break;
      case knickknack_internal_register::ARG2:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x52));
        break;
    }
  #elif defined(__arm__)
    switch(reg)
    {
      case knickknack_internal_register::RES:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("01B4"));
        break;
      case knickknack_internal_register::ARG1:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("02B4"));
        break;
      case knickknack_internal_register::ARG2:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("04B4"));
        break;
      case knickknack_internal_register::ARG3:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("08B4"));
        break;
    }
  #endif
  
  return true;
}

bool knickknack_internal_compiler::pop_to_register(knickknack_internal_register reg)
{
  #if defined(__X86__)
    switch(reg)
    {
      case knickknack_internal_register::RES:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x58));
        break;
      case knickknack_internal_register::ARG1:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x59));
        break;
      case knickknack_internal_register::ARG2:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x5A));
        break;
    }
  #elif defined(__arm__)
    switch(reg)
    {
      case knickknack_internal_register::RES:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("01BC"));
        break;
      case knickknack_internal_register::ARG1:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("02BC"));
        break;
      case knickknack_internal_register::ARG2:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("04BC"));
        break;
      case knickknack_internal_register::ARG3:
        KNICKKNACK_RETURN_IF_FALSE(write_buffer_multi("08BC"));
        break;
    }
  #endif

  return true;
}

bool knickknack_internal_compiler::end()
{
  KNICKKNACK_RETURN_IF_FALSE(process_tokens());

  #if defined(__arm__)
    KNICKKNACK_RETURN_IF_FALSE(arm_align_for_32_bit_value());
  #endif

  // Allocate space for variables
  auto variable_section_offset = m_buffer_offset;
  auto variable_section_size = m_deferred_variable_offsets.size() * KNICKKNACK_VALUE_SIZE;
  for (unsigned int i = 0; i < variable_section_size; ++i)
  {
    KNICKKNACK_RETURN_IF_FALSE(write_buffer(0x00));
  }
  
  // Trim any excess allocated space from the buffer
  if (m_buffer_offset < m_buffer_length)
  {
    m_buffer_length = m_buffer_offset;
    m_binary->buffer = knickknack_realloc_with_gc(m_binary->buffer, m_buffer_length);
    if (!m_binary->buffer)
    {
      KNICKKNACK_LOG_ERROR("%s: buffer shrink failed\n", m_name.c_str());
      return false;
    }
  }

  // Ensure the buffer is aligned
  size_t alignment_offset = 0;
  #if defined(__arm__)
  {
    size_t alignment = 16;
    auto delta = (size_t)((unsigned int)m_binary->buffer % alignment);
    if (delta != 0)
    {
      // Resize the buffer so we have room to slide it to the right so it's aligned      
      m_buffer_length += alignment;
      m_binary->buffer = knickknack_realloc_with_gc(m_binary->buffer, m_buffer_length);
      if (!m_binary->buffer)
      {
        KNICKKNACK_LOG_ERROR("%s: buffer resize failed\n", m_name.c_str());
        return false;
      }
      
      // Realloc might have moved the buffer so we need to recalculate the delta
      // and check if we're still unaligned
      delta = (size_t)((unsigned int)m_binary->buffer % alignment);
      if (delta != 0)
      {
        // Since we're still unaligned, slide the buffer into alignment and store the new offset
        alignment_offset = alignment - delta;
        auto dst = (void*)((char*)m_binary->buffer + alignment_offset);
        memmove(dst, m_binary->buffer, m_buffer_length-alignment);
      }

      // Realloc may cause the buffer to move which can put us into alignment or change the alignment delta
      // As such, it's impossible to know how much extra space is needed until after realloc finishes
      // The excess space before & after the buffer is left allocated to avoid an infinite loop of reallocs
    }
  }
  #endif

  // Store the section pointers, accounting for alignment
  m_binary->variable_section = (void*)((char*)m_binary->buffer + alignment_offset + variable_section_offset);
  m_binary->end = (void*)((char*)m_binary->buffer + alignment_offset + m_buffer_offset);

  // Update deferred label offsets
  for (const auto kv : m_deferred_label_offsets)
  {
    if (m_label_offsets.find(kv.first) != m_label_offsets.end())
    {  
      auto label_offset = (int)m_label_offsets[kv.first];
      for (const auto offset : kv.second)
      {
        auto address = (void*)((unsigned int)m_binary->buffer + alignment_offset + offset);
        #if defined(__X86__)
          // On X86 write the delta to the label
          auto value = label_offset - (int)offset - KNICKKNACK_VALUE_SIZE;
        #elif defined(__arm__)
          // On ARM, write the exact address of the label
          // Need to set LSB so we stay in THUMB mode
          auto value = ((int)m_binary->buffer + alignment_offset + label_offset) | 1;
        #endif
        knickknack_internal_overwrite_address(address, value);
      }
    }
    else if (g_knickknack_builtins.find(kv.first) != g_knickknack_builtins.end())
    {
      auto label_address = (int)g_knickknack_builtins[kv.first];
      for (const auto offset : kv.second)
      {
        auto address = (void*)((int)m_binary->buffer + alignment_offset + offset);
        #if defined(__X86__)
          auto value = label_address - (int)address - KNICKKNACK_VALUE_SIZE;
        #elif defined(__arm__)
          auto value = label_address | 1;
        #endif
        knickknack_internal_overwrite_address(address, value);
      }
    }
    else
    {
      KNICKKNACK_LOG_ERROR("%s: undefined label %s\n", m_name.c_str(), kv.first.c_str());
      return false;
    }
  }

  // Update deferred variable offsets
  int current_variable_address = (int)m_binary->variable_section;
  for (const auto kv : m_deferred_variable_offsets)
  {
    for (const auto offset : kv.second)
    {
      auto address = (void*)((int)m_binary->buffer + alignment_offset + offset);
      knickknack_internal_overwrite_address(address, current_variable_address);
    }

    if (kv.first == "std_this")
    {
      m_deferred_literal_offsets[m_name].push_back((size_t)(current_variable_address-(int)m_binary->buffer+alignment_offset));
    }
    else if (kv.first == "std_globals")
    {
      {
        #ifdef KNICKKNACK_ENABLE_THREADS
          static KNICKKNACK_INIT_LOCK(s_mutex);
          auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(s_mutex);
        #endif
        
        if (!g_knickknack_globals)
        {
          g_knickknack_globals = knickknack_std_new_dict();
        }
      }
      knickknack_internal_overwrite_address((void*)current_variable_address, (int)g_knickknack_globals);
    }
    
    if (m_binary->exports.find(kv.first) != m_binary->exports.end())
    {
      m_binary->exports[kv.first] = (void*)current_variable_address;
    }

    current_variable_address += KNICKKNACK_VALUE_SIZE;
  }

  // Ensure literals are generated for this script, called scripts and entry points
  // This ensures that multiple calls to the same function won't need multiple copies of the same string
  if (m_deferred_lazy_symbols.size() > 0)
  {
    m_deferred_literal_offsets[m_name];
    for (auto tup : m_deferred_lazy_symbols)
    {
      auto callee_name = std::get<2>(tup);
      auto entry_point = std::get<3>(tup);
      
      m_deferred_literal_offsets[callee_name];
      m_deferred_literal_offsets[entry_point];
    }
  }

  // Allocate string literals and update deferred offsets
  std::unordered_map<std::string, knickknack_object*> temp_literal_map {};
  for (const auto kv : m_deferred_literal_offsets)
  {
    auto new_object = knickknack_std_new_string();
    auto str = knickknack_get_string_from_object(new_object);
    if (!str)
    {
      KNICKKNACK_LOG_ERROR("%s: Error allocating string literal\n", m_name.c_str());
      return false;
    }
    new_object->kind = knickknack_kind::STRING_LITERAL;
    {
      #ifdef KNICKKNACK_ENABLE_THREADS
        auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
      #endif
      g_knickknack_gc_heap.erase(new_object);
    }
    str->append(kv.first);
    m_binary->literals.insert(new_object);
    temp_literal_map[kv.first] = new_object;
    for (const auto offset : kv.second)
    {
      auto address = (void*)((int)m_binary->buffer + alignment_offset + offset);
      knickknack_internal_overwrite_address(address, (int)new_object);
    }
  }

  // Populate address offsets to the lazy linker stub and update the deferred symbols table
  for (auto tup : m_deferred_lazy_symbols)
  {
    auto stub_offset = std::get<0>(tup);
    auto func_offset = std::get<1>(tup);
    auto callee_name = std::get<2>(tup);
    auto entry_point = std::get<3>(tup);

    auto stub_address = (void*)((int)m_binary->buffer + alignment_offset + stub_offset);
    #if defined(__X86__)
      auto delta = (int)knickknack_internal_lazy_linker_stub - (int)stub_address - KNICKKNACK_VALUE_SIZE;
      knickknack_internal_overwrite_address(stub_address, delta);
    #elif defined(__arm__)
      knickknack_internal_overwrite_address(stub_address, (int)stub_address);
    #endif

    auto func_address = (void*)((int)m_binary->buffer + alignment_offset + func_offset);
    m_binary->deferred_symbols.push_back(stub_address);

    #ifdef KNICKKNACK_ENABLE_THREADS
      auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
    #endif

    g_knickknack_deferred_symbols[stub_address] = 
    {
      temp_literal_map[m_name],
      func_address,
      temp_literal_map[callee_name],
      temp_literal_map[entry_point]
    };
  }

  // Populate exports with the correct addresses
  for (const auto kv : m_binary->exports)
  {
    if (kv.second == nullptr)
    {
      if (m_label_offsets.find(kv.first) != m_label_offsets.end())
      {
        auto address = (void*)((int)m_binary->buffer + alignment_offset + m_label_offsets[kv.first]);
        m_binary->exports[kv.first] = address;
      }
      else
      {
        KNICKKNACK_LOG_ERROR("%s: undeclared export %s\n", m_name.c_str(), kv.first.c_str());
        return false;
      }
    }
  }

  // Make the buffer executable
  #if defined(__WIN32__)
    DWORD oldValue;
    if(!VirtualProtect(m_binary->buffer, m_buffer_length, PAGE_EXECUTE_READWRITE, &oldValue))
    {
      KNICKKNACK_LOG_ERROR("%s: Couldn't make buffer executable\n", m_name.c_str());
      return false;
    }
  #elif defined(__linux__)
    // XXX need to add Linux support
  #endif
  
  return true;
}

void knickknack_internal_destroy_binary(knickknack_binary* binary)
{
  if (binary)
  {
    free(binary->buffer);
    delete &binary->exports;
    
    for (const auto literal : binary->literals)
    {
      literal->kind = knickknack_kind::STRING;
      knickknack_internal_destroy_shallow(literal);
    }
    delete &binary->literals;
    
    for (const auto addr : binary->deferred_symbols)
    {
      g_knickknack_deferred_symbols.erase(addr);
    }
    delete &binary->deferred_symbols;
    
    delete &binary->linked_binaries;
    free(binary);
  }
}

void knickknack_internal_overwrite_address(void* address, int value)
{
  for (unsigned int i = 0; i < KNICKKNACK_VALUE_SIZE; ++i)
  {
    ((char*)address)[i] = value % 256;
    value >>= 8;
  }
}

#if defined(__X86__)
void knickknack_internal_lazy_linker_stub()
#elif defined(__arm__)
void knickknack_internal_lazy_linker_stub(void* stub_addr)
#endif
{
  // On ARM, g++ destroys LR in the function prologue so
  // stub_addr needs to be passed as an arg
  // __builtin_return_address works as expected on x86
  #if defined(__X86__)
    auto stub_addr = (void*)((unsigned int)__builtin_return_address(0) - KNICKKNACK_VALUE_SIZE);
  #endif

  {
    #ifdef KNICKKNACK_ENABLE_THREADS
      auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
    #endif

    if (g_knickknack_deferred_symbols.find(stub_addr) == g_knickknack_deferred_symbols.end())
    {
      KNICKKNACK_LOG_ERROR("invalid lazy linker address: %p\n", stub_addr);
      g_knickknack_fatal_error_hook();
    }
  }

  auto tup = g_knickknack_deferred_symbols[stub_addr];
  auto caller_name_literal = std::get<0>(tup);
  auto func_addr           = std::get<1>(tup);
  auto callee_name_literal = std::get<2>(tup);
  auto entry_point_literal = std::get<3>(tup);

  auto caller_name = *knickknack_get_readonly_string_from_object(caller_name_literal);
  auto callee_name = *knickknack_get_readonly_string_from_object(callee_name_literal);
  auto entry_point = *knickknack_get_readonly_string_from_object(entry_point_literal);

  // Load the script and verify the entry point
  auto callee = knickknack_internal_acquire_script(callee_name);
  if (callee->exports.find(entry_point) == callee->exports.end())
  {
    KNICKKNACK_LOG_ERROR("%s to %s: invalid lazy linker entry point: %s\n", caller_name.c_str(), callee_name.c_str(), entry_point.c_str());
    g_knickknack_fatal_error_hook();
  }

  // Update the call address
  #if defined(__X86__)
    auto delta = (int)callee->exports[entry_point] - (int)func_addr - KNICKKNACK_VALUE_SIZE;
    knickknack_internal_overwrite_address(func_addr, delta);
  #elif defined(__arm__)
    auto addr = (int)callee->exports[entry_point];
    knickknack_internal_overwrite_address(func_addr, addr|1);
  #endif

  // Replace the call to the stub with a NOP slide or branch so it isn't called again
  #if defined(__X86__)
    for (int i = -1; i < (int)KNICKKNACK_VALUE_SIZE; ++i)
    {
      ((char*)stub_addr)[i] = 0x90;
    }
  #elif defined(__arm__)
    ((char*)stub_addr)[-4] = 0x0A;
    ((char*)stub_addr)[-3] = 0xE0;
  #endif

  // Get the caller binary
  auto caller = knickknack_internal_acquire_script(caller_name);

  // Keep track of which scripts the caller is using so they're not destroyed by GC until the caller returns
  caller->linked_binaries.insert(knickknack_thread_local_name(callee_name));

  knickknack_internal_release_script(caller);
  knickknack_internal_release_script(callee);

  {
    #ifdef KNICKKNACK_ENABLE_THREADS
      auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
    #endif

    g_knickknack_deferred_symbols.erase(stub_addr);
  }

  return;
}
