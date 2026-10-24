#ifndef _SCHED_PELT_H
#define _SCHED_PELT_H

#ifdef CONFIG_SMP
#include <linux/sched.h>

/*
 * LOAD_AVG_PERIOD/LOAD_AVG_MAX values match the stock 4.9 fair.c
 * (LOAD_AVG_PERIOD=32, LOAD_AVG_MAX=47742) and k419-firefly's default
 * _32 half-life; duplicated here since pelt.h is a standalone header
 * included from multiple TUs (fair.c, core.c) where fair.c's local
 * #defines are not visible.
 */
#define LOAD_AVG_PERIOD 32
#define LOAD_AVG_MAX 47742

#ifndef cap_scale
#define cap_scale(v, s) ((v)*(s) >> SCHED_CAPACITY_SHIFT)
#endif

#define PELT_MIN_DIVIDER	(LOAD_AVG_MAX - 1024)

static inline u32 get_pelt_divider(struct sched_avg *avg)
{
	return PELT_MIN_DIVIDER + avg->period_contrib;
}

int __update_load_avg_blocked_se(u64 now, struct sched_entity *se);
int __update_load_avg_se(u64 now, struct cfs_rq *cfs_rq, struct sched_entity *se);
int __update_load_avg_cfs_rq(u64 now, struct cfs_rq *cfs_rq);
int update_rt_rq_load_avg(u64 now, struct rq *rq, int running);
int update_dl_rq_load_avg(u64 now, struct rq *rq, int running);

#ifdef CONFIG_SCHED_THERMAL_PRESSURE
int update_thermal_load_avg(u64 now, struct rq *rq, u64 capacity);

static inline u64 thermal_load_avg(struct rq *rq)
{
	return READ_ONCE(rq->avg_thermal.load_avg);
}
#else
static inline int
update_thermal_load_avg(u64 now, struct rq *rq, u64 capacity)
{
	return 0;
}

static inline u64 thermal_load_avg(struct rq *rq)
{
	return 0;
}
#endif

#ifdef CONFIG_HAVE_SCHED_AVG_IRQ
int update_irq_load_avg(struct rq *rq, u64 running);
#else
static inline int
update_irq_load_avg(struct rq *rq, u64 running)
{
	return 0;
}
#endif

/* 4.9 has no util_est/UTIL_EST machinery; this is a plain no-op. */
static inline void cfs_se_util_change(struct sched_avg *avg)
{
}

/* 4.9 has no clock_update lockdep bookkeeping on rq; no-op. */
static inline void assert_clock_updated(struct rq *rq)
{
}

/* Minimal 64-bit (arm64) u64_u32_store: no 32-bit lockless readers exist in
 * this tree for these PELT clock fields, so a single WRITE_ONCE suffices. */
static inline void u64_u32_store(u64 *v, u64 x)
{
	WRITE_ONCE(*v, x);
}

static inline u64 rq_clock_pelt(struct rq *rq)
{
	lockdep_assert_held(&rq->lock);
	assert_clock_updated(rq);

	return rq->clock_pelt - rq->lost_idle_time;
}

/* The rq is idle, we can sync to clock_task */
static inline void _update_idle_rq_clock_pelt(struct rq *rq)
{
	rq->clock_pelt  = rq_clock_task(rq);

	u64_u32_store(&rq->clock_idle, rq_clock(rq));
	smp_wmb();
	u64_u32_store(&rq->clock_pelt_idle, rq_clock_pelt(rq));
}

static inline void update_rq_clock_pelt(struct rq *rq, s64 delta)
{
	if (unlikely(is_idle_task(rq->curr))) {
		_update_idle_rq_clock_pelt(rq);
		return;
	}

	/*
	 * Scale the elapsed time to reflect the real amount of computation.
	 * 4.9's arch_scale_cpu_capacity()/arch_scale_freq_capacity() take
	 * (struct sched_domain *, int cpu) - pass NULL sd, matching how
	 * 4.9's own fair.c __update_load_avg() calls them.
	 */
	delta = cap_scale(delta, arch_scale_cpu_capacity(NULL, cpu_of(rq)));
	delta = cap_scale(delta, arch_scale_freq_capacity(NULL, cpu_of(rq)));

	rq->clock_pelt += delta;
}

static inline void update_idle_rq_clock_pelt(struct rq *rq)
{
	u32 divider = ((LOAD_AVG_MAX - 1024) << SCHED_CAPACITY_SHIFT) - LOAD_AVG_MAX;
	u32 util_sum = rq->cfs.avg.util_sum;
	util_sum += rq->avg_rt.util_sum;
	util_sum += rq->avg_dl.util_sum;

	if (util_sum >= divider)
		rq->lost_idle_time += rq_clock_task(rq) - rq->clock_pelt;

	_update_idle_rq_clock_pelt(rq);
}

/* cfs_rq->rq is the direct 4.9 field (struct cfs_rq { struct rq *rq; ... }) */
static inline struct rq *pelt_rq_of(struct cfs_rq *cfs_rq)
{
	return cfs_rq->rq;
}

#ifdef CONFIG_CFS_BANDWIDTH
static inline void update_idle_cfs_rq_clock_pelt(struct cfs_rq *cfs_rq)
{
	u64 throttled;

	if (unlikely(cfs_rq->throttle_count))
		throttled = U64_MAX;
	else
		throttled = cfs_rq->throttled_clock_pelt_time;

	u64_u32_store(&cfs_rq->throttled_pelt_idle, throttled);
}

/* rq->task_clock normalized against any time this cfs_rq has spent throttled */
static inline u64 cfs_rq_clock_pelt(struct cfs_rq *cfs_rq)
{
	if (unlikely(cfs_rq->throttle_count))
		return cfs_rq->throttled_clock_pelt - cfs_rq->throttled_clock_pelt_time;

	return rq_clock_pelt(pelt_rq_of(cfs_rq)) - cfs_rq->throttled_clock_pelt_time;
}
#else
static inline void update_idle_cfs_rq_clock_pelt(struct cfs_rq *cfs_rq) { }
static inline u64 cfs_rq_clock_pelt(struct cfs_rq *cfs_rq)
{
	return rq_clock_pelt(pelt_rq_of(cfs_rq));
}
#endif

/*
 * se_weight/se_runnable helpers, adapted from k419-firefly (4.19)
 * MINUS the 4.19-only `se->sched_delayed` guard (DELAYED_DEQUEUE, absent in 4.9).
 */
#ifdef CONFIG_FAIR_GROUP_SCHED
#define entity_is_task_pelt(se) (!se->my_q)
static inline struct cfs_rq *cfs_rq_of_pelt(struct sched_entity *se)
{
	return se->cfs_rq;
}
static inline void se_update_runnable(struct sched_entity *se)
{
	se->runnable_weight = cfs_rq_of_pelt(se)->h_nr_runnable;
}
static inline unsigned long se_runnable(struct sched_entity *se)
{
	return entity_is_task_pelt(se) ? !!se->on_rq : se->runnable_weight;
}
static inline unsigned long se_weight(struct sched_entity *se)
{
	return scale_load_down(se->load.weight);
}
#else
#define entity_is_task_pelt(se) (1)
static inline void se_update_runnable(struct sched_entity *se)
{
}
static inline unsigned long se_runnable(struct sched_entity *se)
{
	return !!se->on_rq;
}
static inline unsigned long se_weight(struct sched_entity *se)
{
	return scale_load_down(se->load.weight);
}
#endif

#endif /* CONFIG_SMP */
#endif /* _SCHED_PELT_H */
