/* scheduler reflection server stub interface : Jiguo 

 * Track all threads that blocks from a client

 * In order to reflect on scheduler, the threads that are blocked from
 * different components are tracked here. There are 2 ways to reflect
 * thread for recovering objects: 1) per object reflection 2) per
 * component reflection (including all threads blocked from that
 * component). 

 * For example, if fault component is faulty, we can either reflect
 * all blocked threads for a specific lock (from lock's client
 * interface), or we can reflect all blocked threads from lock
 * component (from scheduler's server interface, which is here)

*/

#include <cos_component.h>
#include <sched.h>
#include <print.h>

#include "../../implementation/sched/cos_sched_sync.h"

#include <objtype.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

struct blocked_thd {
	int id;
	int dep_thd;
	struct blocked_thd *next, *prev;
};

struct rec_data_blk {
	spdid_t       spdid;
	struct blocked_thd blkthd;
};

CVECT_CREATE_STATIC(rec_blk_vect);
CSLAB_CREATE(rdblk, sizeof(struct rec_data_blk));

static struct rec_data_blk *
rdblk_lookup(int id)
{ 
	return cvect_lookup(&rec_blk_vect, id); 
}

static struct rec_data_blk *
rdblk_alloc(int spdid)
{
	struct rec_data_blk *rd;

	rd = cslab_alloc_rdblk();
	assert(rd);
	cvect_add(&rec_blk_vect, rd, spdid);
	return rd;
}

static void
rdblk_dealloc(struct rec_data_blk *rd)
{
	assert(rd);
	cslab_free_rdblk(rd);
}

static void
rdblk_addblk(struct rec_data_blk *rd, struct blocked_thd *ptr_blkthd, int dep_thd)
{
	assert(rd && ptr_blkthd);

	ptr_blkthd->id = cos_get_thd_id();
	ptr_blkthd->dep_thd = dep_thd;

	INIT_LIST(ptr_blkthd, next, prev);
	ADD_LIST(&rd->blkthd, ptr_blkthd, next, prev);
       
	return;
}

// track blocked threads here for all clients (on each thread stack)
int __sg_sched_block(spdid_t spdid, int dependency_thd)
{
	struct blocked_thd blk_thd;
	struct rec_data_blk *rd = NULL;

#ifdef REFLECTION_ALL
	// add to list
	cos_sched_lock_take();
	if (unlikely(!rdblk_lookup(spdid))) {
		rd = rdblk_alloc(spdid);
		assert(rd);
		INIT_LIST(&rd->blkthd, next, prev);
		rd->spdid = spdid;
	} else {
		rd = rdblk_lookup(spdid);
		assert(rd);
	}
	assert(rd->spdid == spdid);
	rdblk_addblk(rd, &blk_thd, dependency_thd);
	cos_sched_lock_release();

	sched_block(spdid, dependency_thd);

	// remove from list
	cos_sched_lock_take();
	REM_LIST(&blk_thd, next, prev);
	cos_sched_lock_release();
#else
	sched_block(spdid, dependency_thd);
#endif
	return 0;
}


/* This is scheduler server interface function for client reflection
   1) When reflect thread state for single object (say, a lock), par:
   blocked thread id 2) When reflect thread state for component (say,
   lock component), par: component id */
int __sg_sched_reflect(spdid_t spd, int par)
{
        struct rec_data_blk *rd = NULL;
	struct blocked_thd *blk_thd;

	assert(par);

	printc("scheduler server side stub (thd %d)\n", cos_get_thd_id());
	printc("passed reflection: spd %d par %d\n", spd, par);

#ifdef REFLECTION_ALL	
	rd = rdblk_lookup(par);
	assert(rd);
	for (blk_thd = FIRST_LIST(&rd->blkthd, next, prev);
	     blk_thd != &rd->blkthd;
	     blk_thd = FIRST_LIST(blk_thd, next, prev)){
		sched_wakeup(cos_spd_id(), blk_thd->id);
	}
#else
	sched_wakeup(spd, par);
#endif
	return 0;
}

