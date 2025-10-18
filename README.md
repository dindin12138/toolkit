# Personal C Toolkit

## Motivation

As I learn C programming, I've realized that many common tools and data structures that are standard in other languages are not part of the C standard library. It's a common practice for C developers to build their own personal, reusable utility libraries to be more productive.

This project is my journey to create such a toolkit. My goal is to build a set of data structures and algorithms that I am familiar with, which will help me in my future C development endeavors. It serves as both a practical tool and a great learning exercise.

## Core Idea

This library provides a set of generic, easy-to-use data structures and algorithms.

The main design feature is a **polymorphic iterator system**, inspired by the C++ STL. This allows me to write generic algorithms (like `find`, `sort`, etc.) that can operate on any data structure in the toolkit, without needing to know the container's internal details.

To get started quickly and build upon battle-tested code, the initial data structures are implemented as wrappers around a well-known, high-quality, single-header library: [stb](https://github.com/nothings/stb) `stb_ds.h`. In the future, other structures might be added by wrapping other libraries or by implementing them from scratch.

## Current Features

*   A generic, dynamic vector (`tk_vec_t`).
*   A polymorphic iterator system.
*   A simple `tk_algo_find_if` algorithm to demonstrate the iterator concept.
*   A standardized error-handling system using the `tk_error_t` enum.

## How to Build and Test

The project is built using CMake.

### Build the Library

```bash
cmake -S . -B build
cmake --build build
```

### Build and Run Tests

The tests use the Criterion framework.

```bash
# From the build directory
cmake -DTOOLKIT_BUILD_TESTS=ON ..
make
ctest
```

## Future Goals

As I learn more and my needs for future projects grow, I plan to:

*   Add more data structures, such as a hash map and a linked list.
*   Expand the algorithm library with functions for sorting, copying, and transforming elements.
*   Continuously refine the API to make it as clean and useful as possible for my own use.
