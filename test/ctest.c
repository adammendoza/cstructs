// ctest.c
//
// A minimalistic test framework.
//

#include "ctest.h"

#include <execinfo.h>
#include <libgen.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


////////////////////////////////////////////////////////
// Constants and static variables.

#define LOG_SIZE 65536
static char *program_name;
static char *test_name;  // A program may run multiple tests.
static char log[LOG_SIZE];
static char *log_cursor;
static char *log_end;

////////////////////////////////////////////////////////
// Static (internal) function definitions.

static void print_trace() {
  void *array[10];
  size_t size = backtrace(array, 10);
  char **strings = backtrace_symbols(array, size);
  for (size_t i = 0; i < size; ++i) test_printf("%s\n", strings[i]);
  free(strings);
}

static void handle_seg_fault(int sig) {
  print_trace();
  test_failed();
}

////////////////////////////////////////////////////////
// Public function definitions.

void start_all_tests(char *name) {
  program_name = basename(name);
  printf("%s - running ", program_name);
  fflush(stdout);
  signal(SIGSEGV, handle_seg_fault);
  log_end = log + LOG_SIZE;
}

int end_all_tests() {
  printf("\r%s - passed \n", program_name);
  return 0;
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

void test_that_(int cond, char *cond_str) {
  if (cond) return;
  test_printf("The following condition failed: %s\n", cond_str);
  test_failed();
}

void test_failed() {
  printf("\r%s - failed \n", program_name);
  printf("Failed in test '%s'; log follows:\n---\n", test_name);
  printf("%s\n---\n", log);
  printf("%s failed while running test %s.\n", program_name, test_name);
  exit(1);
}
