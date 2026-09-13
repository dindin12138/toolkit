/**
 * @file algo.h
 * @brief Convenience header for the Toolkit Algorithm Library.
 *
 * @details
 * This file is the primary include for the tk_algo module. It includes all
 * available algorithm headers (sequence, numeric, and future sorting, etc.)
 * for ease of use.
 *
 * A user can simply #include <tk/algo/algo.h> to get all generic
 * algorithms provided by the toolkit.
 *
 * @section algo_index Algorithm index (stage 1)
 * Every algorithm below is a `static inline` function operating on the
 * polymorphic tk_iterator_t interface. The header comments in each module are
 * the single source of truth for the full contract (category requirement,
 * return value, preconditions, complexity); this index is only a map.
 *
 * Sequence algorithms (<tk/algo/sequence.h>):
 *  - tk_algo_find_if  : first element matching a predicate (returns iterator)
 *  - tk_algo_for_each : call f on every element, in place (returns void)
 *  - tk_algo_count_if : count elements matching a predicate (returns size_t)
 *  - tk_algo_count    : count elements equal to a value via eq (returns size_t)
 *  - tk_algo_copy     : byte-copy the range into an output range
 *                       (returns the output iterator past the last write)
 *  - tk_algo_transform: write op(in) into an output range
 *                       (returns the output iterator past the last write)
 *
 * Numeric algorithms (<tk/algo/numeric.h>):
 *  - tk_algo_accumulate : fold the range into a caller-owned accumulator
 *                         (returns void; the result stays in the caller's *acc)
 */
#ifndef TOOLKIT_ALGO_ALGO_H
#define TOOLKIT_ALGO_ALGO_H

// Include all algorithm modules
#include <tk/algo/numeric.h>
#include <tk/algo/sequence.h>

#endif // TOOLKIT_ALGO_ALGO_H
