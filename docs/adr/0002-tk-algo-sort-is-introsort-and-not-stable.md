# Sort is introsort and is not stable

Sorting must be in-place, allocation-free and worst-case O(n log n): a sort that can fail on allocation, or degrade to quadratic time on adversarial input, is not safe to call from generic code. Introsort meets all three, while a stable sort would need scratch space the library does not have. We chose introsort and document explicitly that the order of equal elements is unspecified.

## Considered Options

- **Heapsort alone** — satisfies the guarantees and reuses the heap family directly, but is consistently slower in practice and gives up quicksort's advantages. Rejected.
- **Merge sort** — the only option that offers stability without extra comparisons, but it needs O(n) scratch space, which makes allocation a caller-visible concern. Rejected for now; a stable sort remains a possible separate addition.

## Consequences

Callers who need equal elements to keep their relative order cannot use this sort. Any future stable sort will have to take its scratch space from the caller.
