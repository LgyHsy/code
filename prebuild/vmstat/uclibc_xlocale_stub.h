/* Stub for uClibc builds without xlocale (__UCLIBC_HAS_XLOCALE__).
 * procps uses newlocale/uselocale only to force C numeric format when
 * parsing /proc; on this toolchain C locale is already the default.
 */
#ifndef ANJ_UCLIBC_XLOCALE_STUB_H
#define ANJ_UCLIBC_XLOCALE_STUB_H

#ifndef LC_NUMERIC_MASK
#define LC_NUMERIC_MASK 0
#endif
#ifndef LC_GLOBAL_LOCALE
#define LC_GLOBAL_LOCALE ((locale_t)0)
#endif

typedef void *locale_t;

static inline locale_t newlocale(int category_mask, const char *locale, locale_t base)
{
	(void)category_mask;
	(void)locale;
	(void)base;
	return (locale_t)0;
}

static inline locale_t uselocale(locale_t dataset)
{
	(void)dataset;
	return (locale_t)0;
}

static inline void freelocale(locale_t dataset)
{
	(void)dataset;
}

#endif /* ANJ_UCLIBC_XLOCALE_STUB_H */
