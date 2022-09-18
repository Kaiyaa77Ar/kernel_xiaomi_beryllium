/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SDM845_COMPAT_H
#define _SDM845_COMPAT_H

/* Compatibility shims for backported core code. Preprocessor-only:
 * macros, literals and forward declarations. Never include heavy
 * subsystem headers (sched.h, mm.h, fs.h) here; this header is
 * force-included into every translation unit, including the early
 * ones that generate <generated/bounds.h>.
 */

#ifndef PF_MEMALLOC_NOFS
#define PF_MEMALLOC_NOFS 0x00080000 /* == PF_MEMALLOC_NOIO on 4.9; split in 4.10 */
#endif

/* MGLRU groundwork: 4.9 names for post-4.9 mm helpers. */
#define page_is_file_lru page_is_file_cache
#define thp_nr_pages(page) hpage_nr_pages(page)

#endif /* _SDM845_COMPAT_H */
