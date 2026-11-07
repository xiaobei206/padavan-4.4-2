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


static array *reentrant_free_target;
static size_t reentrant_free_calls;
static int reentrant_free_saw_empty;
static int reentrant_free_init_result;
static int reentrant_free_push_result;
static void *reentrant_free_replacement;

static void reinitialize_during_array_free(void *element)
{
  if (NULL == element)
    return;

  ++reentrant_free_calls;
  if (reentrant_free_calls == 1) {
    reentrant_free_saw_empty =
        reentrant_free_target->data == NULL &&
        reentrant_free_target->index == 0 &&
        reentrant_free_target->length == 0;

    /* A nested free must see the detached empty array. */
    array_free(reentrant_free_target, reinitialize_during_array_free);
    reentrant_free_init_result = array_init(reentrant_free_target, 1);
    if (reentrant_free_init_result == 0)
      reentrant_free_push_result =
          array_push(reentrant_free_target, reentrant_free_replacement);
  }
}

static int test_free_callback_reentrancy(void)
{
  array values = { NULL, 0, 0 };
  int first = 11;
  int second = 22;
  int replacement = 33;
  int okay = 0;

  reentrant_free_target = &values;
  reentrant_free_calls = 0;
  reentrant_free_saw_empty = 0;
  reentrant_free_init_result = -1;
  reentrant_free_push_result = -1;
  reentrant_free_replacement = &replacement;

  if (array_init(&values, 2) != 0 ||
      array_push(&values, &first) != 0 ||
      array_push(&values, &second) != 1) {
    fprintf(stderr, "array-heap reentrant-free fixture initialization failed\n");
    goto done;
  }

  array_free(&values, reinitialize_during_array_free);
  if (!check(reentrant_free_calls == 2,
      "array_free did not invoke each detached element callback exactly once") ||
      !check(reentrant_free_saw_empty,
      "array_free callback observed the original array before detachment") ||
      !check(reentrant_free_init_result == 0 && reentrant_free_push_result == 0,
      "array_free callback could not reinitialize and reuse the array") ||
      !check(values.data != NULL && values.index == 1 && values.length == 1 &&
          ((void **)values.data)[0] == &replacement,
      "outer array_free destroyed storage created by its callback"))
    goto done;

  okay = 1;

done:
  reentrant_free_target = NULL;
  array_free(&values, NULL);
  reentrant_free_replacement = NULL;
  return okay;
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

  if (!test_free_callback_reentrancy())
    return EXIT_FAILURE;

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
