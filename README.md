# KnickKnack

KnickKnack is a toy programming language and accompanying language runtime. It is designed to be embedded within a game engine running on older, resource constrained game systems such as the Nintendo DS. It includes a garbage collector. The runtime dynamically loads modules and compiles them to machine code as functions from said modules are called and will unload them when memory is needed, which results in KnickKnack code having both a small memory footprint and a fast runtime speed. KnickKnack code can be called from C/C++ code and vice-versa. The current implementation runs on the Nintendo DS (ARM9), Windows (x86) and Linux (x86) with other platforms potentially planned for the future. 


## Building

To build for the Nintendo DS, install Python; devkitARM and libnds then run:

```
cd nds/source
python make_codemap.py
cd ..
make
```

To build for Windows, install g++ via MinGW then run:

```
cd win
build.bat
```


## Usage

The standalone version of the KnickKnack runtime takes the path to a script as its only argument. KnickKnack will call the scripts `main` function without any arguments. For example, run `KnickKnack examples/hilo.k` to run the `hilo.k` example. KnickKnack relies on several standard libraries. It will search for these libraries in the current working directory (i.e. `.`) and in a folder called `lib` in the current working directory (i.e. `./lib`). The DS build is currently setup to embed all scripts and libraries within the .nds binary. It is also hardcoded to always call the `main` function of `nds/source/main.k`.

In most cases, you'll want to embed KnickKnack within another engine or application. C/C++ code can call KnickKnack functions via `knickknack_call`. KnickKnack code can call any C functions added to the map `g_knickknack_builtins`. See `src/api.h` and `src/std.cpp` for additional details.
