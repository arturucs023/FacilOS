/* nslibc/assert.h: sin NDEBUG aborta (ns_abort); con NDEBUG, nada. */
#ifndef F32NS_ASSERT_H
#define F32NS_ASSERT_H

void ns_abort(void);

#ifdef NDEBUG
#define assert(x) ((void)0)
#else
#define assert(x) ((x) ? (void)0 : ns_abort())
#endif

#endif
