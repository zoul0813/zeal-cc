# C Compiler for Zeal 8-bit OS

The scope of this project is to write a C Compiler for the Zeal 8-bit OS system, powered by a Z80 CPU.  This project should also compile with GCC/Clang on macOS and Linux, and you can use conditional compilation to achieve this.  We will run tests on macOS and Linux, to verify the compilers code initially.

The project should compile using SDCC 4.4.0, and Zeal 8-bit OS (ZOS) has custom API's for working with files
and user input.  Example projects that are built for ZOS and tested, are in the examples/ folder.

The C Compiler should be able to compile C code written in the C99 standard.

The compiler should produce appropriate Z80 Assembly files that can later be assembled and linked by Zealasm (source for Zealasm in examples/Zealasm).

A starter CMakeLists.txt is already in the project root - feel free to modify this as needed, but use it as a starting point as the ZOS Toolchain has a few special needs.