// Counting malloc wrappers for the embedded gnuplot; see gp_memory.h.
#include "gp_memory.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#include "memory_budget.hpp"

#if defined(_WIN32)
#include <malloc.h>
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

namespace {

std::atomic<bool> g_failed{false};
char g_message[256];

std::size_t block_size(void *p) {
#if defined(_WIN32)
  return _msize(p);
#elif defined(__APPLE__)
  return malloc_size(p);
#else
  return malloc_usable_size(p);
#endif
}

// Count `n` bytes that are already allocated. release() subtracts, so a wrapped-around negative
// amount adds without a budget check (bare gnuplot mallocs must not fail).
void add_unchecked(std::size_t n) { membudget::release(std::size_t(0) - n); }

// Count a block gnuplot allocated; false (nothing counted) when it does not fit the budget.
bool charge_block(void *p) {
  try {
    membudget::charge(block_size(p));
    return true;
  } catch (const std::exception &e) {
    lmplot_mem_note_failure(e.what());
    return false;
  }
}

void *counted(void *p) {
  if (p && !charge_block(p)) add_unchecked(block_size(p));
  return p;
}

void uncount(void *p) {
  if (p) membudget::release(block_size(p));
}

}  // namespace

extern "C" {

void lmplot_mem_note_failure(const char *message) {
  if (g_failed.load()) return;
  std::snprintf(g_message, sizeof g_message, "%s", message ? message : "out of memory");
  g_failed.store(true);
}

const char *lmplot_mem_failure(void) { return g_failed.load() ? g_message : nullptr; }

void *lmplot_mem_malloc(size_t n) { return counted(std::malloc(n)); }

void *lmplot_mem_calloc(size_t count, size_t size) { return counted(std::calloc(count, size)); }

char *lmplot_mem_strdup(const char *s) {
  std::size_t n = std::strlen(s) + 1;
  char *d = static_cast<char *>(lmplot_mem_malloc(n));
  if (d) std::memcpy(d, s, n);
  return d;
}

void *lmplot_mem_realloc(void *p, size_t n) {
  std::size_t old = p ? block_size(p) : 0;
  void *q = std::realloc(p, n);
  if (!q) return nullptr;  // p is untouched
  membudget::release(old);
  return counted(q);
}

void lmplot_mem_free(void *p) {
  uncount(p);
  std::free(p);
}

void *lmplot_mem_malloc_checked(size_t n) {
  try {
    membudget::require(n);
  } catch (const std::exception &e) {
    lmplot_mem_note_failure(e.what());
    return nullptr;
  }
  void *p = std::malloc(n ? n : 1);
  if (p && !charge_block(p)) {
    std::free(p);
    return nullptr;
  }
  return p;
}

void *lmplot_mem_realloc_checked(void *p, size_t n) {
  if (!p) return lmplot_mem_malloc_checked(n);
  std::size_t old = block_size(p);
  try {
    if (n > old) membudget::require(n - old);
  } catch (const std::exception &e) {
    lmplot_mem_note_failure(e.what());
    return nullptr;  // gnuplot keeps p and reports "out of memory"
  }
  void *q = std::realloc(p, n);
  if (!q) return nullptr;
  membudget::release(old);
  return counted(q);
}

}  // extern "C"
