# Personal C Toolkit

## Motivation

As I learn C programming, I've realized that many common tools and data structures that are standard in other languages are not part of the C standard library. It's a common practice for C developers to build their own personal, reusable utility libraries to be more productive.

This project is my journey to create such a toolkit. My goal is to build a set of data structures and algorithms that I am familiar with, which will help me in my future C development endeavors. It serves as both a practical tool and a great learning exercise.

## Core Idea

This library provides a set of generic, easy-to-use data structures and algorithms.

The main design feature is a **polymorphic iterator system**, inspired by the C++ STL. This allows me to write generic algorithms (like `tk_algo_find_if` and `tk_algo_sort`) that can operate on any data structure in the toolkit, without needing to know the container's internal details.

To get started quickly and build upon battle-tested code, the initial data structures are implemented as wrappers around a well-known, high-quality, single-header library: [stb](https://github.com/nothings/stb) `stb_ds.h`. In the future, other structures might be added by wrapping other libraries or by implementing them from scratch.

## Current Features

- A generic, dynamic vector (`tk_vec_t`), with random-access iterators.
- A doubly linked list (`tk_list_t`), with bidirectional iterators.
- A polymorphic iterator system with capability categories (`FORWARD`,
  `BIDIRECTIONAL`, `RANDOM_ACCESS`) and a validated vtable protocol.
- Element size is part of the protocol: every iterator reports the byte size of
  one element, at every position (including `end()`), so algorithms can move
  elements without the caller supplying a size.
- Random-access protocol support: `tk_iter_advance_by` (O(1) offset movement)
  and `tk_iter_distance` (O(1) element distance), advertised only by
  random-access iterators.
- A three-way comparator type (`tk_comparator_t`), used by every algorithm that
  reorders a range: it follows `qsort`'s convention rather than the STL's
  two-way predicate, so equality is reportable in one call.
- Generic sequence algorithms (`<tk/algo/sequence.h>`), all operating on the
  polymorphic iterator interface:
  - `tk_algo_find_if` — first element matching a predicate;
  - `tk_algo_for_each` — call a function on every element (in place);
  - `tk_algo_count` / `tk_algo_count_if` — count matching elements;
  - `tk_algo_copy` / `tk_algo_transform` — write a range into an output range.
    `tk_algo_copy` is a three-argument copy like STL's: it reads the element
    size from the source iterator instead of taking one;
  - `tk_algo_swap` — exchange the elements at two iterator positions, byte-wise,
    for any element size, without allocating.
- Generic numeric algorithms (`<tk/algo/numeric.h>`):
  - `tk_algo_accumulate` — fold a range into a caller-owned accumulator.
- The heap family (`<tk/algo/heap.h>`), operating in place on any range with
  random access, without allocating and without copying the data anywhere:
  - `tk_algo_make_heap` — rearrange an arbitrary range into a max-heap in
    linear time;
  - `tk_algo_push_heap` — sift a newly appended element up into the heap;
  - `tk_algo_pop_heap` — move the maximum to the last position and restore the
    property over the shortened range, leaving removal to the caller;
  - `tk_algo_sort_heap` — drain a heap into ascending order (heapsort);
  - `tk_algo_is_heap` — check the heap property; `sort_heap` asserts it in
    debug builds, because draining a non-heap yields silent garbage.
- The sort family (`<tk/algo/sort.h>`), operating in place on any range with
  random access, without allocating and without copying the data anywhere:
  - `tk_algo_sort` — order a range into the order the comparator dictates. It
    is an introsort: quicksort with a median-of-three pivot and a three-way
    partition, heapsort once the recursion budget is spent, insertion sort to
    finish. Worst case O(n log n) for every input, logarithmic recursion depth,
    and linear on a range whose elements all compare equal.
  - `tk_algo_sort` is **not stable**: equal elements may end up in any order.
    Stability would need O(n) scratch space, and this sort allocates nothing
    (ADR-0002).
  - A range without random access is a debug assertion; in a release build the
    call returns and leaves the range untouched, silently. The header carries
    that as a warning section.
- The umbrella header `<tk/algo/algo.h>` includes every algorithm module.
- Algorithms are container-agnostic: the same call works across containers
  (e.g. copying a `tk_list_t` into a `tk_vec_t`).
- A standardized error-handling system using the `tk_error_t` enum.

## How to Build and Test

The project is built using CMake and, in this repository, is developed inside a
[Nix](https://nixos.org/) + [devenv](https://devenv.sh/) shell that provides the
compiler (`clang`), `cmake`, `pkg-config` and the Criterion test framework. If
you have `direnv` configured, the environment loads automatically; otherwise run
commands through `devenv shell`:

```bash
devenv shell --quiet -- bash -c 'cmake -S . -B build -G Ninja && cmake --build build'
```

### Build the Library

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

### Build and Run Tests

The tests use the Criterion framework.

```bash
cmake -S . -B build -G Ninja -DTOOLKIT_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Optional: Sanitizers

To configure a build with AddressSanitizer and UndefinedBehaviorSanitizer, pass
`-DTOOLKIT_ENABLE_SANITIZERS=ON`:

```bash
cmake -S . -B build-asan -G Ninja -DTOOLKIT_BUILD_TESTS=ON -DTOOLKIT_ENABLE_SANITIZERS=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

## Future Goals

As I learn more and my needs for future projects grow, I plan to:

- Add more data structures, such as a hash map.
- Expand the algorithm library further (more `<numeric>` algorithms such as
  `reduce` / `partial_sum`); copying, transforming, the heap family and sorting
  are already available.
- Continuously refine the API to make it as clean and useful as possible for my own use.
