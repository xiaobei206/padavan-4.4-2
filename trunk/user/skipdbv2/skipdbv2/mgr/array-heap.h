#ifndef __ARRAY_HEAP_H__
#define __ARRAY_HEAP_H__

typedef struct {
  void* data;
  int index;
  int length;
} array;

/* Initialize an uninitialized array; failure leaves it empty. */
int array_init(array* arr, int size);
/* Return the inserted zero-based index, or -1 without changing contents. */
int array_push(array* arr, void* data);
/* Increase capacity; failed reallocations preserve the existing allocation. */
int array_grow(array* arr, int size);
/* Detach/reset before callbacks, then release the old backing allocation. */
void array_free(array* arr, void (*free_element)(void*));

#endif
