# Toolkit

A personal C toolkit of generic data structures and algorithms that interoperate through a runtime-polymorphic iterator protocol.

## Language

**Random access iterator**:
An iterator whose position can be moved by any offset, and whose distance to another position can be measured, both in constant time.
_Avoid_: Indexed iterator, array iterator

**Element size**:
The number of bytes occupied by one element of a range.
_Avoid_: Element width, stride, item size

**Comparator**:
A caller-supplied three-way ordering over two elements, reporting less than, equal to, or greater than.
_Avoid_: Less function, predicate, ordering callback

**Heap property**:
The arrangement in which no element is ordered above its parent, so the greatest element sits at the root.
_Avoid_: Heap order, heap invariant

**Sift up**:
Restoring the heap property by moving one element toward the root until its parent is no longer above it.
_Avoid_: Bubble up, percolate up, swim

**Sift down**:
Restoring the heap property by moving one element toward the leaves, following the greater child, until neither child is above it.
_Avoid_: Bubble down, percolate down, sink

**Introsort**:
A sorting strategy that begins as quicksort, switches to heapsort once the recursion grows too deep, and finishes small ranges with insertion sort.
_Avoid_: Hybrid sort, adaptive quicksort
