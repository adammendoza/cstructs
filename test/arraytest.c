#include "CArray.h"

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

// used for test framework only (which may be true for above includes as well!)
#include <libgen.h>
#include <stdarg.h>
#include <string.h>

// TODO
// * Make sure I have full code coverage.
// * Include all major use cases (not many).
// * Look for edge cases.
// * Try to keep the test framework general (within CStructs).
// * Clean up comments within this file.

/* Some practice code to help me know how to do a few things:
 * (1) Print out a stack trace on a signal like a seg fault.
 * (2) How to work with an array of arrays, if needed.
 */

#define array_size(x) (sizeof(x) / sizeof(x[0]))


////////////////////////////////////////////////////////////////////////////////
// Test framework. (I plan to move this into it's own file.)

enum {
  test_success = 0,
  test_failure = 1
};

#define LOG_SIZE 65536
static char *program_name;
static char *test_name;  // A program may run multiple tests.
static char log[LOG_SIZE];
static char *log_cursor;
static char *log_end;

typedef int (*TestFunction)();

int test_printf_(const char *format, ...) {
  static char buffer[LOG_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, LOG_SIZE, format, args);
  va_end(args);

  // The -1 makes room for the null terminator.
  size_t size_left = log_end - log_cursor - 1;
  char *new_cursor = log + strlcat(log_cursor, buffer, size_left);
  int chars_added = new_cursor - log_cursor;
  log_cursor = new_cursor;

  return chars_added;
}

#define run_test(x) run_test_(x, #x)
#define run_tests(...) run_tests_(#__VA_ARGS__, __VA_ARGS__)

#define test_printf(...) test_printf_(__VA_ARGS__)

void print_trace() {
  void *array[10];
  size_t size;
  char **strings;
  size_t i;

  size = backtrace (array, 10);
  strings = backtrace_symbols (array, size);
 
  test_printf ("Obtained %zd stack frames.\n", size);

  for (i = 0; i < size; i++) test_printf ("%s\n", strings[i]);

  free (strings);
}

void test_failed() {
  printf("\r%s - failed \n", program_name);
  printf("Failed in test '%s'; log follows:\n---\n", test_name);
  printf("%s\n---\n", log);
  printf("%s failed while running test %s.\n", program_name, test_name);
  exit(1);
}

void handle_seg_fault(int sig) {
  print_trace();
  test_failed();
}

void start_all_tests(char *name) {
  program_name = basename(name);
  printf("%s - running ", program_name);
  fflush(stdout);
  signal(SIGSEGV, handle_seg_fault);
  log_end = log + LOG_SIZE;
}

void run_test_(TestFunction test_fn, char *new_test_name) {
  log[0] = '\0';
  log_cursor = log;

  test_name = new_test_name;

  test_fn();
}

void run_tests_(char *test_names, ...) {
  va_list args;
  va_start(args, test_names);

  char *names = strdup(test_names);
  char *token;
  while ((token = strsep(&names, " \t\n,")) != NULL) {
    if (*token == '\0') continue;  // Avoid empty tokens.
    run_test_(va_arg(args, TestFunction), token);
  }

  va_end(args);
  free(names);
}

int end_all_tests() {
  printf("\r%s - passed \n", program_name);
  return 0;
}

void test_that_(int cond, char *cond_str) {
  if (cond) return;
  test_printf("The following condition failed: %s\n", cond_str);
  test_failed();
}

#define test_that(cond) test_that_(cond, #cond)


// End of test framework.
////////////////////////////////////////////////////////////////////////////////


void print_int_array(CArray int_array) {
  CArrayFor(int *, i, int_array) {
    test_printf("%d ", *i);
  }
  test_printf("\n");
}

void print_double_array(CArray double_array) {
  CArrayFor(double *, d, double_array) {
    test_printf("%f ", *d);
  }
  test_printf("\n");
}

// Test out an array of direct ints.
int test_int_array() {
  CArray array = CArrayNew(0, sizeof(int));
  test_that(array->count == 0);

  int i = 1;
  CArrayAddElement(array, i);  // array is now [1].
  test_that(CArrayElementOfType(array, 0, int) == 1);
  test_that(array->count == 1);

  CArrayAddZeroedElements(array, 2);  // array is now [1, 0, 0].
  test_that(CArrayElementOfType(array, 2, int) == 0);
  test_that(array->count == 3);

  // Test out appending another array.
  CArray other_array = CArrayNew(4, sizeof(int));
  int other_ints[] = {2, 3, 4, 5};
  for (i = 0; i < 4; ++i) CArrayAddElement(other_array, other_ints[i]);
  test_printf("other_array has contents:\n");
  print_int_array(other_array);
  CArrayAppendContents(array, other_array);
  test_printf("Just after append, array is:\n");
  print_int_array(array);
  CArrayDelete(other_array);
  test_that(array->count == 7);
  test_that(CArrayElementOfType(array, 5, int) == 4);

  // Test out a for loop over the elements.
  int values[] = {1, 0, 0, 2, 3, 4, 5};
  i = 0;
  CArrayFor(int *, int_ptr, array) {
    test_that(*int_ptr == values[i++]);
  }

  CArrayDelete(array);

  return test_success;
}

// This tests a pattern using an array of arrays of ints.
int test_subarrays() {
  CArray array = CArrayNew(16, sizeof(CArrayStruct));
  array->releaser = CArrayRelease;

  for (int i = 0; i < 10; ++i) {
    CArray subarray = CArrayInit(CArrayNewElement(array), 16, sizeof(int));
    test_printf("subarray = %p\n", subarray);
    for (int j = 0; j < 5; ++j) {
      int newInt = j + i * 5;
      CArrayAddElement(subarray, newInt);
    }
    print_int_array(subarray);
  }

  test_printf("Starting to print out subarrays.\n");
  int i = 0;
  CArrayFor(CArray, subarray, array) {
    int elt = CArrayElementOfType(subarray, 2, int);
    test_that(elt == i * 5 + 2);
    ++i;
    print_int_array(subarray);
  }

  CArrayDelete(array);

  return test_success;
}

// Test that the releaser is properly called.
static int num_releaser_calls = 0;
void counting_releaser(void *element) {
  num_releaser_calls++;
}

int test_releaser() {
  const int num_elts = 32;
  CArray array = CArrayNew(64, sizeof(int));
  array->releaser = counting_releaser;

  for (int i = 0; i < num_elts; ++i) {
    CArrayAddElementByPointer(array, &i);
  }

  CArrayDelete(array);

  test_printf("num_releaser_calls=%d.\n", num_releaser_calls);
  test_that(num_releaser_calls == num_elts);

  return test_success;
}

int test_clear() {
  CArray array = CArrayNew(0, sizeof(double));
  double values[] = {1.0, 2.0, 3.14};
  for (int i = 0; i < array_size(values); ++i) CArrayAddElement(array, values[i]);
  test_that(array->count == 3);
  test_that(CArrayElementOfType(array, 2, double) == 3.14);
  CArrayClear(array);
  test_that(array->count == 0);
  CArrayDelete(array);
  return test_success;
}

// Test that sorting works.
int compare_doubles(void *context, const void *elt1, const void *elt2) {
  double d1 = *(double *)elt1;
  double d2 = *(double *)elt2;
  return d1 - d2;
}

int test_sort() {
  CArray array = CArrayNew(0, sizeof(double));
  double values[] = {-1.2, 2.4, 3.1, 0.0, -2.2};
  for (int i = 0; i < array_size(values); ++i) CArrayAddElement(array, values[i]);
  test_that(array->count == 5);
  test_that(CArrayElementOfType(array, 2, double) == 3.1);
  test_printf("Before sorting, array is:\n");
  print_double_array(array);
  CArraySort(array, compare_doubles, NULL);
  test_printf("After sorting, array is:\n");
  print_double_array(array);
  test_that(CArrayElementOfType(array, 0, double) == -2.2);
  test_that(CArrayElementOfType(array, 1, double) == -1.2);
  CArrayDelete(array);
  return test_success;
}

int test_remove() {
  CArray array = CArrayNew(0, sizeof(double));
  double values[] = {2.0, 3.0, 5.0, 7.0};
  for (int i = 0; i < array_size(values); ++i) CArrayAddElement(array, values[i]);
  test_that(array->count == 4);

  double *element = CArrayElement(array, 2);
  test_that(*element == 5.0);

  CArrayRemoveElement(array, element);
  test_that(array->count == 3);
  test_that(CArrayElementOfType(array, 2, double) == 7.0);
  CArrayDelete(array);
  return test_success;
}

// Test the find functionality; we need to sort elements in memcmp order for find to work.
int mem_compare(void *context, const void *elt1, const void *elt2) {
  return memcmp(elt1, elt2, *(size_t *)context);
}

int test_find() {
  size_t element_size = sizeof(double);

  CArray array = CArrayNew(0, element_size);
  double values[] = {2.0, 3.0, 5.0, 7.0};
  for (int i = 0; i < array_size(values); ++i) CArrayAddElement(array, values[i]);
  test_that(array->count == 4);

  CArraySort(array, mem_compare, &element_size);

  void *element = CArrayFind(array, &values[1]);
  test_that(*(double *)element == values[1]);

  CArrayDelete(array);

  return test_success;
}

int main(int argc, char **argv) {
  start_all_tests(argv[0]);
  run_tests(
    test_subarrays, test_int_array, test_releaser,
    test_clear, test_sort, test_remove, test_find
  );
  return end_all_tests();
}
