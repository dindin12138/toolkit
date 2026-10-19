# Comparators are three-way

Algorithms that reorder a range have to recognise equal elements: the sort's partition step groups them, and the heap must not disturb elements that compare equal. A two-way "is a less than b" predicate can express ordering but not equality, so every equality check would need two calls and would still only infer the answer from what did not happen. We therefore take a comparator that reports less than, equal to, or greater than in one call.

## Considered Options

- **A two-way less-than predicate**, matching the shape of the predicates the sequence algorithms already take. Familiar to anyone coming from the STL, and uniform with the rest of the library, but it cannot report equality — which the partition step needs to handle duplicate-heavy ranges efficiently. Rejected.

## Consequences

Comparators follow the C standard library's `qsort` convention rather than the STL's. The sequence algorithms' predicates stay two-way, so the library carries two conventions for user-supplied functions: a predicate answers a question about one element, a comparator orders two. The glossary distinguishes them for that reason.
