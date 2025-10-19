/**
 * @file list.c
 * @brief Implements the public interface for the toolkit's generic
 * doubly-linked list.
 *
 * @details
 * This implementation provides a non-intrusive, generic doubly-linked list
 * that stores copies of user data, similar in semantics to tk_vec_t.
 * It manages node allocation internally and uses malloc/memcpy for element
 * data. It provides bidirectional iterators.
 */

#include <stdlib.h>           // For malloc, free
#include <string.h>           // For memcpy
#include <tk/core/error.h>    // Error codes
#include <tk/core/iterator.h> // Iterator definitions
#include <tk/core/macros.h>   // TK_ASSERT
#include <tk/core/types.h>    // tk_bool, size_t
#include <tk/ds/list.h>       // Our public header

/**
 * @brief Internal node structure for the doubly-linked list.
 */
typedef struct tk_list_node_t {
  struct tk_list_node_t *prev;
  struct tk_list_node_t *next;
  void *data; // Pointer to the allocated copy of the element's data
} tk_list_node_t;

/**
 * @struct tk_list_t
 * @brief The opaque struct for the doubly-linked list container.
 */
struct tk_list_t {
  tk_list_node_t *head; // Pointer to the first node, or NULL if empty
  tk_list_node_t *tail; // Pointer to the last node, or NULL if empty
  size_t size;          // Number of elements in the list
  size_t element_size;  // Size of each element in bytes
};

// --- Iterator Implementation ---

/**
 * @brief Private state for a tk_list_t iterator.
 * Stores node pointer and list pointer. Fits within SBO.
 */
typedef struct {
  tk_list_node_t *node; // Pointer to the current node (or NULL for end)
  tk_list_t *list; // Pointer back to the list (needed for retreat from end)
} tk_list_iter_state_t;

// --- Helper Functions ---

/**
 * @brief Allocates and initializes a new list node, including copying element
 * data.
 * @param element Pointer to the user data to copy.
 * @param element_size Size of the element data.
 * @return Pointer to the new node, or NULL on allocation failure.
 */
static tk_list_node_t *tk_list_node_create(const void *element,
                                           size_t element_size) {
  tk_list_node_t *node = (tk_list_node_t *)malloc(sizeof(tk_list_node_t));
  if (!node) {
    return NULL; // Node allocation failed
  }

  node->data = malloc(element_size);
  if (!node->data) {
    free(node); // Data allocation failed, free the node
    return NULL;
  }

  // Store a copy
  memcpy(node->data, element, element_size);
  node->prev = NULL;
  node->next = NULL;
  return node;
}

/**
 * @brief Frees a list node and its associated data copy.
 * @param node The node to free.
 * @param destroyer Optional function to free element data resources. It
 * receives the pointer to the element data itself (node->data).
 */
static void tk_list_node_destroy(tk_list_node_t *node,
                                 tk_element_destroyer_t destroyer) {
  if (!node)
    return;

  if (destroyer) {
    // Pass the pointer to the copied data directly to the destroyer
    destroyer(node->data);
  }
  free(node->data); // Free the copied data
  free(node);       // Free the node itself
}

// --- Lifecycle Functions ---

tk_list_t *tk_list_create(size_t element_size) {
  TK_ASSERT(element_size > 0);
  if (element_size == 0) {
    return NULL; // Invalid argument
  }

  tk_list_t *list = (tk_list_t *)malloc(sizeof(tk_list_t));
  if (!list) {
    return NULL; // Allocation failed
  }

  list->head = NULL;
  list->tail = NULL;
  list->size = 0;
  list->element_size = element_size;
  return list;
}

void tk_list_clear(tk_list_t *list) {
  TK_ASSERT(list != NULL);
  if (!list)
    return;

  tk_list_node_t *current = list->head;
  tk_list_node_t *next;
  while (current != NULL) {
    next = current->next;
    // Use NULL destroyer for shallow clear
    tk_list_node_destroy(current, NULL);
    current = next;
  }
  list->head = NULL;
  list->tail = NULL;
  list->size = 0;
}

void tk_list_destroy(tk_list_t *list) {
  if (!list) {
    return;
  }
  tk_list_clear(list); // Free all nodes and their data copies
  free(list);          // Free the list structure itself
}

void tk_list_destroy_full(tk_list_t *list, tk_element_destroyer_t destroyer) {
  if (!list) {
    return;
  }

  tk_list_node_t *current = list->head;
  tk_list_node_t *next;
  while (current != NULL) {
    next = current->next;
    // Pass the user's destroyer function
    tk_list_node_destroy(current, destroyer);
    current = next;
  }
  // No need to reset head/tail/size as the list struct is freed immediately
  // after
  free(list);
}

// --- Size/Query Functions ---

size_t tk_list_size(const tk_list_t *list) {
  TK_ASSERT(list != NULL);
  return list ? list->size : 0;
}

tk_bool tk_list_is_empty(const tk_list_t *list) {
  TK_ASSERT(list != NULL);
  return list ? (list->size == 0) : true;
}

// --- Element Access Functions ---

const void *tk_list_front(const tk_list_t *list) {
  TK_ASSERT(list != NULL);
  return (list && list->head) ? list->head->data : NULL;
}

void *tk_list_front_mut(tk_list_t *list) {
  TK_ASSERT(list != NULL);
  return (list && list->head) ? list->head->data : NULL;
}

const void *tk_list_back(const tk_list_t *list) {
  TK_ASSERT(list != NULL);
  return (list && list->tail) ? list->tail->data : NULL;
}

void *tk_list_back_mut(tk_list_t *list) {
  TK_ASSERT(list != NULL);
  return (list && list->tail) ? list->tail->data : NULL;
}

// --- Modifiers ---

tk_error_t tk_list_push_back(tk_list_t *list, const void *element) {
  TK_ASSERT(list != NULL && element != NULL);
  if (!list || !element)
    return TK_E_INVALID_ARG;

  tk_list_node_t *new_node = tk_list_node_create(element, list->element_size);
  if (!new_node) {
    return TK_E_NOMEM;
  }

  if (list->tail) { // List is not empty
    list->tail->next = new_node;
    new_node->prev = list->tail;
    list->tail = new_node;
  } else { // List is empty
    list->head = new_node;
    list->tail = new_node;
  }
  list->size++;
  return TK_SUCCESS;
}

void tk_list_pop_back(tk_list_t *list) {
  TK_ASSERT(list != NULL);
  if (!list || !list->tail) {
    return; // Empty list or invalid list
  }

  tk_list_node_t *node_to_remove = list->tail;
  list->tail = node_to_remove->prev;

  if (list->tail) { // More than one element existed
    list->tail->next = NULL;
  } else { // Only one element existed
    list->head = NULL;
  }

  tk_list_node_destroy(node_to_remove, NULL);
  list->size--;
}

tk_error_t tk_list_push_front(tk_list_t *list, const void *element) {
  TK_ASSERT(list != NULL && element != NULL);
  if (!list || !element)
    return TK_E_INVALID_ARG;

  tk_list_node_t *new_node = tk_list_node_create(element, list->element_size);
  if (!new_node) {
    return TK_E_NOMEM;
  }

  if (list->head) { // List is not empty
    list->head->prev = new_node;
    new_node->next = list->head;
    list->head = new_node;
  } else { // List is empty
    list->head = new_node;
    list->tail = new_node;
  }
  list->size++;
  return TK_SUCCESS;
}

void tk_list_pop_front(tk_list_t *list) {
  TK_ASSERT(list != NULL);
  if (!list || !list->head) {
    return; // Empty list or invalid list
  }

  tk_list_node_t *node_to_remove = list->head;
  list->head = node_to_remove->next;

  if (list->head) { // More than one element existed
    list->head->prev = NULL;
  } else { // Only one element existed
    list->tail = NULL;
  }

  tk_list_node_destroy(node_to_remove, NULL);
  list->size--;
}

// Helper function to validate iterator and get node (can return NULL for end
// iter)
static tk_list_node_t *tk_list_get_node_from_iter(const tk_list_t *list,
                                                  tk_iterator_t iter) {
  if (!iter.vtable || iter.vtable->type_name == NULL ||
      strcmp(iter.vtable->type_name, "tk_list_iterator") != 0) {
    return (tk_list_node_t *)0xFFFFFFFF; // Use a distinct invalid pointer value
  }
  // Check if iterator's list pointer matches our list
  const tk_list_iter_state_t *state =
      (const tk_list_iter_state_t *)iter.state.data;
  if (state->list != list) {
    return (tk_list_node_t *)0xFFFFFFFF; // Iterator from a different list
  }
  return state->node; // Can be NULL if it's the end iterator
}

tk_error_t tk_list_insert_before(tk_list_t *list, tk_iterator_t before_iter,
                                 const void *element) {
  TK_ASSERT(list != NULL && element != NULL);
  if (!list || !element)
    return TK_E_INVALID_ARG;

  tk_list_node_t *before_node = tk_list_get_node_from_iter(list, before_iter);

  // Check for distinct invalid pointer
  if (before_node == (tk_list_node_t *)0xFFFFFFFF) {
    return TK_E_INVALID_ARG;
  }

  if (before_node == NULL) { // Insert at the end (before_iter is end())
    return tk_list_push_back(list, element);
  }

  if (before_node == list->head) { // Insert at the beginning (before head)
    return tk_list_push_front(list, element);
  }

  // Insert in the middle (before before_node)
  tk_list_node_t *new_node = tk_list_node_create(element, list->element_size);
  if (!new_node) {
    return TK_E_NOMEM;
  }

  // Link new_node between before_node->prev and before_node
  new_node->prev = before_node->prev;
  new_node->next = before_node;
  // before_node->prev cannot be NULL here because we handled the head case
  TK_ASSERT(before_node->prev != NULL);
  before_node->prev->next = new_node;
  before_node->prev = new_node;

  list->size++;
  return TK_SUCCESS;
}

tk_iterator_t tk_list_erase_at(tk_list_t *list, tk_iterator_t iter) {
  // Initialize to invalid iterator
  tk_iterator_t next_iter = {.vtable = NULL};
  TK_ASSERT(list != NULL);
  if (!list || tk_list_is_empty(list)) {
    return next_iter; // Return invalid iterator
  }

  tk_list_node_t *node_to_remove = tk_list_get_node_from_iter(list, iter);

  // Check for invalid iterator or trying to erase end()
  if (node_to_remove == (tk_list_node_t *)0xFFFFFFFF ||
      node_to_remove == NULL) {
    return next_iter; // Return invalid iterator
  }

  // Determine the iterator for the next node *before* unlinking
  tk_list_node_t *next_node = node_to_remove->next;
  if (next_node) {
    // Create a valid iterator pointing to the next node
    next_iter.vtable = iter.vtable; // Use the same vtable
    tk_list_iter_state_t *next_state =
        (tk_list_iter_state_t *)next_iter.state.data;
    next_state->list = list;
    next_state->node = next_node;
  } else {
    // If we removed the last element, the next iterator is the end iterator
    next_iter = tk_list_end(list);
  }

  // Unlink the node
  if (node_to_remove->prev) {
    node_to_remove->prev->next = node_to_remove->next;
  } else {
    // Removing the head
    TK_ASSERT(list->head == node_to_remove);
    list->head = node_to_remove->next;
  }

  if (node_to_remove->next) {
    node_to_remove->next->prev = node_to_remove->prev;
  } else {
    // Removing the tail
    TK_ASSERT(list->tail == node_to_remove);
    list->tail = node_to_remove->prev;
  }

  tk_list_node_destroy(node_to_remove, NULL);
  list->size--;

  return next_iter;
}

// --- vtable function implementations ---

/**
 * @brief (vtable) Advances the list iterator to the next node.
 */
static void tk_list_iter_advance(tk_iterator_t *self) {
  tk_list_iter_state_t *state = (tk_list_iter_state_t *)self->state.data;
  if (state->node) { // Check if not already at end
    state->node = state->node->next;
  }
  // If state->node was NULL (end), it remains NULL.
}

/**
 * @brief (vtable) Retreats the list iterator to the previous node.
 * Required for TK_ITER_BIDIRECTIONAL.
 */
static void tk_list_iter_retreat(tk_iterator_t *self) {
  tk_list_iter_state_t *state = (tk_list_iter_state_t *)self->state.data;
  if (state->node) { // Retreating from a valid node
    state->node = state->node->prev;
  } else { // Retreating from the end() iterator (node is NULL)
    TK_ASSERT(state->list != NULL); // Need the list pointer
    // Check if list is empty before accessing tail
    if (state->list->tail != NULL) {
      state->node = state->list->tail; // Go to the last element
    }
    // If list was empty, node remains NULL, which is correct for begin() ==
    // end()
  }
}

/**
 * @brief (vtable) Gets the data pointer from the list iterator.
 * Asserts if trying to dereference the end iterator.
 */
static void *tk_list_iter_get(const tk_iterator_t *self) {
  const tk_list_iter_state_t *state =
      (const tk_list_iter_state_t *)self->state.data;
  // Cannot dereference end iterator (node is NULL)
  TK_ASSERT(state->node != NULL && "Attempted to dereference end() iterator");
  return state->node ? state->node->data
                     : NULL; // Return NULL defensively if assert disabled
}

/**
 * @brief (vtable) Checks if two list iterators are equal.
 */
static tk_bool tk_list_iter_equal(const tk_iterator_t *iter1,
                                  const tk_iterator_t *iter2) {
  const tk_list_iter_state_t *state1 =
      (const tk_list_iter_state_t *)iter1->state.data;
  const tk_list_iter_state_t *state2 =
      (const tk_list_iter_state_t *)iter2->state.data;
  // Equal if they point to the same node (or both are NULL/end)
  // AND belong to the same list instance.
  return (state1->node == state2->node) && (state1->list == state2->list);
}

/**
 * @brief (vtable) Clones a list iterator.
 */
static void tk_list_iter_clone(tk_iterator_t *dest, const tk_iterator_t *src) {
  // Simple struct copy works because state fits in SBO
  *dest = *src;
}

/**
 * @brief The single, static vtable for all tk_list_t iterators.
 * Uses the updated TK_DEFINE_ITERATOR_VTABLE macro.
 */
static const tk_iterator_vtable_t g_list_vtable =
    TK_DEFINE_ITERATOR_VTABLE(tk_list_iter,          /* Function Prefix */
                              TK_ITER_BIDIRECTIONAL, /* Category */
                              "tk_list_iterator");   /* Type Name */

// --- Public iterator function implementations ---

tk_iterator_t tk_list_begin(tk_list_t *list) {
  TK_ASSERT(list != NULL);
  // Validate the vtable in debug builds (now checks retreat).
  tk_iterator_vtable_validate(&g_list_vtable);

  tk_iterator_t iter;
  iter.vtable = &g_list_vtable;

  // Get pointer to the internal state buffer
  tk_list_iter_state_t *state = (tk_list_iter_state_t *)iter.state.data;

  // Fill state: store list pointer and pointer to the first node
  state->list = list;
  state->node = list->head;

  return iter;
}

tk_iterator_t tk_list_end(tk_list_t *list) {
  TK_ASSERT(list != NULL);
  tk_iterator_t iter;
  iter.vtable = &g_list_vtable;

  // Get pointer to the internal state buffer
  tk_list_iter_state_t *state = (tk_list_iter_state_t *)iter.state.data;

  // Fill state: store list pointer and NULL for the node pointer
  state->list = list;
  state->node = NULL;

  return iter;
}
