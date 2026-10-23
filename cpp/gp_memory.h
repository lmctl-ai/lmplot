/* Memory budget for the embedded gnuplot (see cpp/memory_budget.hpp and
 * durable-memory/09-resource-limits.md).
 *
 * gnuplot is C: its heap memory comes from malloc, outside the counting operator new. This header
 * is force-included (-include) into every gnuplot C source and routes malloc/calloc/realloc/
 * strdup/free through counting wrappers (cpp/gp_memory.cpp), so gnuplot's heap is part of the
 * host's total budget. Only gp_alloc/gp_realloc (src/alloc.c, compiled with LMPLOT_MEM_CHECKED)
 * may fail: over budget they get NULL and report "out of memory" through gnuplot's own error path
 * (int_error/longjmp, no C++ exception in C frames). Bare malloc/strdup calls in gnuplot rarely
 * check for NULL, so they are counted but never refused; the host reports a budget failure noted
 * during a command after the command returns. */
#ifndef LMPLOT_GP_MEMORY_H
#define LMPLOT_GP_MEMORY_H
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#ifndef __APPLE__
#include <malloc.h> /* before the macros below: it declares malloc/free again */
#endif

#ifdef __cplusplus
extern "C" {
#endif
void *lmplot_mem_malloc(size_t n);
void *lmplot_mem_calloc(size_t count, size_t size);
void *lmplot_mem_realloc(void *p, size_t n);
char *lmplot_mem_strdup(const char *s);
void lmplot_mem_free(void *p);
/* NULL instead of going over the budget (gp_alloc / gp_realloc only). */
void *lmplot_mem_malloc_checked(size_t n);
void *lmplot_mem_realloc_checked(void *p, size_t n);
/* First failure of a budget check (or an exception caught at a C boundary) since start, or NULL. */
const char *lmplot_mem_failure(void);
void lmplot_mem_note_failure(const char *message);
#ifdef __cplusplus
}
#endif

#ifndef __cplusplus
#undef malloc
#undef calloc
#undef realloc
#undef strdup
#undef free
#ifdef LMPLOT_MEM_CHECKED
#define malloc(n) lmplot_mem_malloc_checked(n)
#define realloc(p, n) lmplot_mem_realloc_checked(p, n)
#else
#define malloc(n) lmplot_mem_malloc(n)
#define realloc(p, n) lmplot_mem_realloc(p, n)
#endif
#define calloc(c, n) lmplot_mem_calloc(c, n)
#define strdup(s) lmplot_mem_strdup(s)
#define free(p) lmplot_mem_free(p)
#endif
#endif
