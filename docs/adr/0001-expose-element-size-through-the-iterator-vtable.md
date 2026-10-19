# Expose element size through the iterator vtable

Algorithms operate on type-erased iterators, so any algorithm that moves elements — copying, swapping, sorting — needs the byte size of one element. Passing that size as an explicit argument, the way the copy algorithm originally did, turns every call site into a place to get it wrong, and a wrong size corrupts memory instead of failing. We therefore made element size a mandatory part of the iterator vtable: the range reports it, and the vtable validator rejects any container that does not.

## Considered Options

- **Caller-supplied size** — the previous copy signature. No protocol change, but every call site carries a second source of truth and a silent corruption path. Rejected.
- **A swap slot instead of a size accessor** — hides byte movement behind each container, but insertion sort must shift a run of elements rather than exchange pairs, so a swap-only protocol cannot express it without a second move primitive. Rejected.
- **A field on the iterator struct** — easiest to read, but every iterator grows, and the struct's size is a fixed, asserted contract. Rejected.

## Consequences

The accessor is mandatory for every iterator category, so a new container cannot register a vtable without implementing it. Algorithms no longer take an element size, which makes the copy signature incompatible with its previous form.
