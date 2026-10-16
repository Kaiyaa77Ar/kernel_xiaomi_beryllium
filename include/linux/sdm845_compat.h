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

/* MGLRU =y workstream Batch Y1: 4.9 has mmap_sem, not mmap_lock. */
#define mmap_read_trylock(mm) down_read_trylock(&(mm)->mmap_sem)
#define mmap_read_unlock(mm) up_read(&(mm)->mmap_sem)

/* 4.9 has get/put_online_cpus, no cpus_read_lock. */
#define cpus_read_lock() get_online_cpus()
#define cpus_read_unlock() put_online_cpus()
#define static_branch_enable_cpuslocked(x) static_branch_enable(x)
#define static_branch_disable_cpuslocked(x) static_branch_disable(x)

/* 4.9 has memalloc_noio, no memalloc_noreclaim. */
#define memalloc_noreclaim_save() memalloc_noio_save()
#define memalloc_noreclaim_restore(x) memalloc_noio_restore(x)

/* 4.9 arm64 is 3-level: fold p4d onto pgd. */
#ifndef p4d_t
#define p4d_t pgd_t
#define PTRS_PER_P4D 1
#define P4D_SHIFT PGDIR_SHIFT
#define P4D_SIZE PGDIR_SIZE
#define P4D_MASK PGDIR_MASK
#define p4d_offset(pgd, addr) ((p4d_t *)(pgd))
#endif

/* 4.9 has pmd/pud_table, not *_leaf. */
#ifndef pmd_leaf
#define pmd_leaf(pmd) (pmd_present(pmd) && !pmd_table(pmd))
#endif
#ifndef pud_leaf
#define pud_leaf(pud) (pud_val(pud) && !pud_table(pud))
#endif
#ifndef p4d_leaf
#define p4d_leaf(p4d) 0
#endif

/* pud_index only exists when PGTABLE_LEVELS > 3; folded otherwise. */
#ifndef pud_index
#define pud_index(addr) (((addr) >> PUD_SHIFT) & (PTRS_PER_PUD - 1))
#endif

#endif /* _SDM845_COMPAT_H */
