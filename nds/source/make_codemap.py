import os

os.chdir(os.path.dirname(os.path.realpath(__file__)))

buffer = 'static std::unordered_map<std::string, std::string> s_code_map = { '

replace_map = [('\\','\\\\'), ('"', '\\"'), ('\n','\\n')]

for base_path in ['.', '..\\..\\lib']:
    for file in os.listdir(base_path):
      if file.endswith('.k'):
        with open(os.path.join(base_path, file)) as f:
          code = f.read()
        for old, new in replace_map:
          code = code.replace(old, new)
        buffer += '{"'+file+'", "'+code+'"}, '

buffer += '};'

with open('codemap.h', 'w') as f:
  f.write(buffer)
