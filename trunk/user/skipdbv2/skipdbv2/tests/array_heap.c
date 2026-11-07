#include "../mgr/array-heap.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int freed_count;

static void count_free(void *element)
{
  if (element != NULL)
    ++freed_count;
}

static int check(int condition, const char *message)
{
  if (condition)
    return 1;
  fprintf(stderr, "array-heap regression failed: %s\n", message);
  return 0;
}

int main(void)
{
  array values;
  array saturated;
  int first = 1, second = 2, third = 3;
  void *sentinel_items[1] = { &first };

  if (!check(array_init(NULL, 1) == -1, "null init must fail"))
    return EXIT_FAILURE;
  if (!check(array_init(&values, 0) == -1 &&
      values.data == NULL && values.index == 0 && values.length == 0,
      "zero capacity must leave an empty array"))
    return EXIT_FAILURE;
  if (!check(array_init(&values, -1) == -1 &&
      values.data == NULL && values.index == 0 && values.length == 0,
      "negative capacity must leave an empty array"))
    return EXIT_FAILURE;
  if (!check(array_init(&values, 1) == 0,
      "positive-capacity initialization failed"))
    return EXIT_FAILURE;

  if (!check(array_push(&values, &first) == 0 &&
      array_push(&values, NULL) == 1 &&
      array_push(&values, &third) == 2,
      "push must grow before writing at full capacity"))
    goto fail;
  if (!check(values.index == 3 && values.length >= 3 &&
      ((void **)values.data)[0] == &first &&
      ((void **)values.data)[1] == NULL &&
      ((void **)values.data)[2] == &third,
      "push changed pointer values or index accounting"))
    goto fail;

  if (!check(array_grow(&values, values.length) == -1 &&
      array_grow(&values, values.length - 1) == -1,
      "grow must reject equal or smaller capacities"))
    goto fail;
  if (!check(array_grow(&values, values.length + 1) == 0 &&
      values.index == 3 &&
      ((void **)values.data)[0] == &first &&
      ((void **)values.data)[2] == &third,
      "successful grow must preserve existing elements"))
    goto fail;

  freed_count = 0;
  array_free(&values, count_free);
  if (!check(values.data == NULL && values.index == 0 && values.length == 0 &&
      freed_count == 2,
      "free must release non-null elements and reset the array"))
    return EXIT_FAILURE;
  array_free(&values, count_free);
  array_free(NULL, count_free);

  saturated.data = sentinel_items;
  saturated.index = INT_MAX;
  saturated.length = INT_MAX;
  if (!check(array_push(&saturated, &second) == -1 &&
      saturated.index == INT_MAX,
      "capacity overflow must fail before writing or incrementing index"))
    return EXIT_FAILURE;
  if (!check(array_push(NULL, &first) == -1 &&
      array_grow(NULL, 8) == -1,
      "null push/grow must fail safely"))
    return EXIT_FAILURE;

  puts("array-heap regression test passed");
  return EXIT_SUCCESS;

fail:
  array_free(&values, NULL);
  return EXIT_FAILURE;
}
