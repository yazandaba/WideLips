# WideLips

A high‑performance, SIMD‑accelerated Lisp parser and parsing framework designed to run at multi‑GB/s.

## What's This About?

Most Lisp parsers advance one character at a time through a state machine. WideLips flips that model: 
it ingests N^2 (currently 32 only) characters per step and classifies them in parallel using underlying CPU SIMD instructions. 
The classification stage produces compact bit‑vectors where each bit corresponds to the character at position N and encodes its class. 
The parser’s state machine then operates over these masks using bit‑twiddling to navigate states and identify tokens 
with far fewer branches and cache misses.

This engine is paired with:

Arena allocators for predictable, allocation‑free hot paths

Zero‑copy tokens that reference the original source

Lazy parsing so only the S‑expressions you touch are materialized

Full trivia and source location tracking, yielding a concrete syntax tree rather than discarding whitespace/comments
Net effect: it scales to huge codebases without bogging down.

**Status**: Vectorized lexing is production‑ready. Dialect coverage and cross‑platform SIMD backends are in active development.
## Why This Exists

Traditional Lisp parsers work fine,Lisp’s highly regular, parenthesized syntax lends itself to data-parallel processing 
on modern CPUs with wide SIMD units and deep cache hierarchies. WideLips explores what a parser looks like when it’s 
designed for that hardware from first principles:

- **Data-Parallel Processing**: SIMD instructions process multiple characters per clock cycle
- **Structured Memory Management**: Arena allocators eliminate per-token allocation overhead
- **Minimal Data Movement**: Zero-copy design for most structures, the major number of memory writes in the 
hot path happens when creating Tokenization blocks and S-expression indices
- **Lazy Materialization**: Lazy parsing only processes the S-expressions you actually need

This architecture targets maximum throughput on modern CPUs while staying practical for real-world use.

## Core Optimizations

### 1. SIMD Vectorized Tokenization

WideLips processes text in N^2 chunks (32-bytes currently) using CPU underlying vector unit (X86 AVX2 currently):

- **Parallel Classification**: Identify character classes (parentheses, identifiers, numbers, whitespace) across 32 characters simultaneously
- **Bitmask Generation**: SIMD comparisons produce compact 32-bit masks for fast decision-making
- **Table-Driven Lookup**: Uses SIMD shuffle instructions (e.g. `pshufb`) for character classification without branches

The vectorized approach eliminates branch mispredictions and processes multiple characters per instruction also
instructions are scheduled carefully and expression trees are balanced to run in parallel (ILP).

### 2. Arenas Backed Containers

Custom `BumpVector<T>` and `MonoBumpVector<T>` containers backed by custom arena allocators:

- **Batch Allocation**: Pre-allocate large memory pools; allocating tokens never call `malloc` or any other dynamic allocation facilities
- **Cache-Friendly Layout**: Contiguous token storage improves prefetching and reduces TLB pressure
- **Fast Reset**: Parser reuse or deallocate via arena reset without individual `free()` calls
- **Predictable Sizing**: Arena capacity estimated from file size to minimize reallocation

This eliminates dynamic allocation/deallocation overhead that plagues traditional parsers allocating thousands of small token objects.

### 3. Zero-Copy Token Design

Tokens store `string_view` references to the original source text instead of copying strings:

```cpp
struct Token {
    TokenType type;
    std::string_view text;  // Points into source buffer
    SourceLocation location;
};
```

**Benefits**:
- Eliminates string allocation and copying overhead
- Reduces memory bandwidth usage
- Tokens become lightweight handles rather than heavyweight objects

### 4. Lazy Two-Phase Parsing

WideLips separates structural analysis from detailed tokenization:

**Phase 1 (Blue Pass)**: Fast structure extraction
- SIMD-classify entire file into `TokenizationBlock[]` array
- Stack-based S-expression matching to build `SExprIndex[]` (parenthesis pairs)
- Diagnostic generation for malformed structure
- Builds a map of where every S-expression lives

**Phase 2 (Green Pass)**: On-demand tokenization
- `TokenizeFirstSExpr()` / `TokenizeNext()`: Materialize tokens only for active S-expressions
- Keyword recognition via SWAR (SIMD-Within-A-Register) for identifiers ≤8 characters
- AST construction happens lazily per S-expression
- Skip tokenizing code sections you never traverse

**Why This Matters**: If you're analyzing one function in a 50,000-line codebase, you only pay for what you use. 
The rest stays as compressed structural metadata.

## Architecture Overview

### Processing Pipeline

```
Source Text
    ↓
┌─────────────────────────────────────┐
│  Phase 1: Blue Pass (Eager)         │
│  - SIMD character classification    │
│  - Validation                       │
│  - Build S-expression indices       │
└─────────────────────────────────────┘
    ↓
SExprIndex[] (Compressed structure map)
    ↓
┌─────────────────────────────────────┐
│  Phase 2: Green Pass (On-Demand)    │
│  - Tokenize specific S-expressions  │
│  - SWAR keyword recognition         │
│  - AST materialization              │
└─────────────────────────────────────┘
    ↓
Tokens & AST (Only for accessed code)
```

### SIMD Character Classification

The heart of WideLips is its vectorized classifier. For each 32-byte chunk:

1. **Load** 32 characters into an AVX2 register
2. **Classify** using parallel comparisons and shuffle operations:
   - Parentheses: `()[]` detection
   - Operators: `+ - * / % ....` detection
   - Whitespace: Space, tab, newline comparison masks
   - Numeric: `0-9` and `.` for number starts
   - Identifier: Valid symbol characters via lookup table
3. **Extract** bitmasks representing character classes
4. **Process** the masks to identify token boundaries

This processes 32 characters in the time traditional parsers handle them one by one, with no branches.

### Memory Layout

```
┌──────────────────────────────────────┐
│  Source Buffer                       │  ← Zero-copy source
└──────────────────────────────────────┘
         ↑ string_view references
┌──────────────────────────────────────┐
│  Token Arena                         │  ← Bump allocator
│  [Token][Token][Token]...            │
└──────────────────────────────────────┘
┌──────────────────────────────────────┐
│  SExprIndex Arena                    │  ← Structure metadata
│  [Index][Index][Index]...            │
└──────────────────────────────────────┘
```

All allocations are linear and contiguous, maximizing cache efficiency.

## Getting Started

### Requirements

- **C++20 compiler**: a mature version of Clang or MSVC (More can be found under [Library Build](#library-build))
- **CMake**: 3.20 or newer
- **CPU**: AVX2 support (Intel Haswell 2013+, AMD Excavator 2015+)

### Build

```bash
# Standard release build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Quick Start

```bash

# Run the benchmark suite
./build/WideLipsBench

# Run tests
ctest --test-dir build --output-on-failure
```

### Build Options

```bash
# Debug build with sanitizers
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON

# Release with benchmarks
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON

# Full build with everything
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTS=ON \
      -DBUILD_BENCHMARKS=ON \
      -DBUILD_EXAMPLES=ON
```

## Project Structure

```
WideLips/
├── include/WideLips/
│   ├── Lexer.h                  # Main tokenizer interface
│   ├── Parser.h                 # S-expression parser
│   ├── Token.h                  # Token type definitions
│   └── ADT/
│       ├── BumpVector.h         # Safe and expnadbale arena based vector
│       └── MonoBumpVector.h     # Unsafe single/mono arena based vector 
│   ├── Diagnostics.h            # Error reporting
│   ├── LispParseTree.h          # Lazy parse tree and nodes implemention 
│   └── AVX.h                    # AVX2 Vector type and instrinsics wraps
├── src/
│   ├── Lexer.cpp
│   └── Parser.cpp
├── tests/                     # Google Test suite
├── benchmarks/                # Performance benchmarks
└── examples/                  # Usage examples
```

## Performance Characteristics

All of the benchmarks were done on Raptor Lake CPU (intel i9-13900k), WideLips achieves:

- **Lexing throughput**: 3.4–4GB/S (on average)
- **Startup time**: Near-instant for lazy parsing (Blue pass only)

The SIMD approach really shines on large files where branch prediction fails traditional parsers.

### What Makes It Fast

- **No malloc in hot path**: Arena allocators eliminate heap allocation overhead
- **No string copies**: Zero-copy tokens reference source directly
- **No branches**: SIMD operations avoid conditional jumps
- **No wasted work**: Lazy parsing only processes what you access
- **Cache-friendly**: Contiguous memory layout and predictable access patterns

### Library Build
- **Flavor**: libWideLips can be built both as a static or shared library, by default, it's built as a static library to build 
libWideLips as a shared library, use `-DBUILD_SHARED_LIBS=ON` option.
- **OS**: WideLips was built on Windows mainly but Linux is fully supported as well.
- **Compiler**: WideLips was compiled using LLVM Clang 21 and MSVC 2022 19.44, but it should work with any C++20 compiler.

### Building And Running Tests
Unit tests projects are built from the source tree and do not link against the core library (libWideLips). 
The unit test project also is built by default, but there are a couple of options and remarks to mention:
- **Sanitizer**: Running unit tests with sanitizers is enabled by using the `ENABLE_SANITIZERS` option. on Windows, 
you need to install the [LLVM toolchain](https://github.com/llvm/llvm-project/releases) and set the `LLVM_DIR` environment variable.
also, you should build the source code with MSVC/clang-cl when you are running the tests with sanitizers on Windows
- **Coverage**: Running unit tests with coverage is enabled by using the `ENABLE_COVERAGE` option. when coverage
is enabled the binaries are instrumented with `-fprofile-instr-generate` and `-fcoverage-mapping`. it's recommended
to use `llvm-cov` and `llvm-profdata` tools to analyze the results, not to mention that the binaries are built with `-O0`
for precise mapping, and to avoid any optimization like DCE (dead code elimination) or inlining, coverage can be enabled
with builds using LLVM clang and MSVC.

## Testing

```bash
# Build and run all tests
cmake --build build --target WideLipsTests

# Run with verbose output
ctest --test-dir build --output-on-failure

# Run specific test suite
./build/tests/WideLipsTests --gtest_filter=LexerTest.*

# Run with sanitizers
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build
./build/tests/WideLipsTests
```

## Current Shortcomings

- **SIMD Platform**: AVX2 only (ARM NEON and AVX-512 planned)
- **OS**: Needs more testing on Linux, X86 macOS will be discarded from any support, but M-based (AArch64)
macOS is planned for future support.

## Contributing

Pull requests are welcome. If you're adding features, please:

- Keep the code clean and well-commented
- Add tests for new functionality
- Maintain the zero-copy, SIMD-first philosophy
- Write commit messages that explain *why*, not just *what*

Found a bug? Open an issue with a minimal reproduction case.

## Development Notes

### Adding SIMD Backends

In the current revision this is still not considered, but the next version will address this gap.

### Parser Extension

The two-phase design keeps the Blue pass (structural analysis) separate from the Green pass (tokenization). This means:

- Syntax extensions only touch the Green pass
- New dialects can reuse the same Blue pass
- Custom AST representations are easy to plug in

## License

MIT License - see LICENSE file for details.

---

*Built for people who think parsers should be fast and aren't afraid to use the hardware we have.*
