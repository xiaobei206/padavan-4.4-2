#include <limits.h>
#include <stdlib.h>

#include "array-heap.h"

static int array_has_valid_state_(const array *arr)
{
  return arr != NULL && arr->data != NULL && arr->index >= 0 &&
      arr->length > 0 && arr->index <= arr->length;
}

int array_init(array* arr, int size)
{
  void **data;

  if (NULL == arr)
    return -1;

  arr->data = NULL;
  arr->index = 0;
  arr->length = 0;
  if (size <= 0 || (size_t)size > (size_t)-1 / sizeof(void *))
    return -1;

  data = (void **)calloc((size_t)size, sizeof(void *));
  if (NULL == data)
    return -1;

  arr->data = data;
  arr->length = size;
  return 0;
}

int array_push(array* arr, void* data)
{
  int index;

  if (!array_has_valid_state_(arr))
    return -1;

  if (arr->index == arr->length) {
    if (arr->length > INT_MAX / 2 ||
        array_grow(arr, arr->length * 2) != 0)
      return -1;
  }

  index = arr->index;
  ((void **)arr->data)[index] = data;
  arr->index = index + 1;
  return index;
}

int array_grow(array* arr, int size)
{
  void **new_data;

  if (!array_has_valid_state_(arr) || size <= arr->length ||
      (size_t)size > (size_t)-1 / sizeof(void *))
    return -1;

  new_data = (void **)realloc(arr->data, sizeof(void *) * (size_t)size);
  if (NULL == new_data)
    return -1;

  arr->data = new_data;
  arr->length = size;
  return 0;
}

void array_free(array* arr, void (*free_element)(void*))
{
  void **items;
  int i;

  if (NULL == arr)
    return;

  items = (void **)arr->data;
  if (items != NULL && free_element != NULL &&
      arr->index > 0 && arr->index <= arr->length) {
    for (i = 0; i < arr->index; ++i) {
      if (items[i] != NULL)
        free_element(items[i]);
    }
  }

  free(arr->data);
  arr->data = NULL;
  arr->index = 0;
  arr->length = 0;
}
