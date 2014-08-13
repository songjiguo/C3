/* 
   Jiguo: add the reflection for the case that the client calls malloc
   and crashes. The mapping DS maintained in mm is thought valid,
   which should be revoked
*/

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <cos_alloc.h>

#include <mem_mgr_large.h>
#include <objtype.h>

#include "../../implementation/sched/cos_sched_sync.h"


/* extern void *alloc_page(void); */
/* extern void free_page(void *ptr); */

#define CSLAB_ALLOC(sz)   malloc(PAGE_SIZE)
#define CSLAB_FREE(x, sz) free(x)
#include <cslab.h>

#define CVECT_ALLOC() malloc(PAGE_SIZE)
#define CVECT_FREE(x) free(x)
#include <cvect.h>

struct rec_data_page {
	vaddr_t addr;
	struct rec_data_page *next, *prev;
};

struct rec_data_spd {
	spdid_t       spdid;
	struct rec_data_page pages;
};

// slab allocator for different spds
CVECT_CREATE_STATIC(rec_spd_vect);
CSLAB_CREATE(rdspd, sizeof(struct rec_data_spd));

static struct rec_data_spd *
rdspd_lookup(int id)
{ 
	return cvect_lookup(&rec_spd_vect, id); 
}

static struct rec_data_spd *
rdspd_alloc(int spdid)
{
	struct rec_data_spd *rd;

	rd = cslab_alloc_rdspd();
	assert(rd);
	cvect_add(&rec_spd_vect, rd, spdid);
	return rd;
}

static void
rdspd_dealloc(struct rec_data_spd *rd)
{
	assert(rd);
	cslab_free_rdspd(rd);
}

// slab allocator for pages in a spd
/* CVECT_CREATE_STATIC(rec_page_vect); */
CSLAB_CREATE(rdpage, sizeof(struct rec_data_page));

/* static struct rec_data_page * */
/* rdpage_lookup(vaddr_t addr) */
/* {  */
/* 	return cvect_lookup(&rec_page_vect, addr >> PAGE_SHIFT);  */
/* } */

static struct rec_data_page *
rdpage_alloc(struct rec_data_spd *rd, vaddr_t addr)
{
	struct rec_data_page *rd_page = NULL;

	assert(rd);

	rd_page = cslab_alloc_rdpage();
	if (!rd_page) goto done;
	rd_page->addr = addr;
	INIT_LIST(rd_page, next, prev);
	ADD_END_LIST(&rd->pages, rd_page, next, prev);
	
	/* cvect_add(&rec_page_vect, rd_page, addr >> PAGE_SHIFT); */
done:
	return rd_page;
}

static void
rdpage_dealloc(struct rec_data_page *rd_page)
{
	assert(rd_page);
	cslab_free_rdpage(rd_page);
	
	return;
}

int __sg_mman_revoke_page(spdid_t spd, vaddr_t addr, int flags)
{
	int ret = 0;
	
	assert(!(ret = mman_revoke_page(spd, addr, flags)));

#ifdef REFLECTION
	cos_sched_lock_take();

	struct rec_data_spd *rd;
	rd = rdspd_lookup(spd);
	assert(rd);

	struct rec_data_page *rd_page;
	/* rd_page = rdpage_lookup(addr); */
	/* assert(rd_page); */

	// remove from list and from cvect
	/* REM_LIST(rd_page, next, prev); */
	/* rdpage_dealloc(rd_page); */

	rd_page = FIRST_LIST(&rd->pages, next, prev);
	assert(rd_page && rd_page != &rd->pages);
	REM_LIST(rd_page, next, prev);
	cos_sched_lock_release();
#endif
	return ret;
}

vaddr_t __sg_mman_get_page(spdid_t spdid, vaddr_t addr, int flags)
{
	vaddr_t ret = 0;

	struct rec_data_spd *rd = NULL;

#ifdef REFLECTION
	ret = mman_get_page(spdid, addr, flags);
	assert(ret > 0);

	// track the allocated page
	cos_sched_lock_take();
	rd = rdspd_lookup(spdid);
	if (unlikely(!rd)) {
		rd = rdspd_alloc(spdid);
		assert(rd);
		INIT_LIST(&rd->pages, next, prev);
		rd->spdid = spdid;
	} 
	assert(rd && rd->spdid == spdid);
	assert(rdpage_alloc(rd, addr));
	cos_sched_lock_release();
#else
	ret = mman_get_page(spdid, addr, flags);
	assert(ret > 0);
#endif

	return ret;
}

/* This is memory manger server interface function for client to
   reflect the allocated pages  */
vaddr_t __sg_mman_reflect(spdid_t spd, int src_spd)
{
        struct rec_data_spd *rd = NULL;
        struct rec_data_page *rd_page = NULL;
	int ret = 0;

	assert(src_spd);
	cos_sched_lock_take();
	/* printc("mm server side stub (thd %d)\n", cos_get_thd_id()); */
	/* printc("passed reflection: spd %d src_spd %d\n", spd, src_spd); */

	rd = rdspd_lookup(src_spd);
	assert(rd);
	rd_page = FIRST_LIST(&rd->pages, next, prev);
	if (rd_page == &rd->pages) goto done; // no more pages on the list
	assert(rd_page);
	ret = rd_page->addr;
done:
	cos_sched_lock_release();
	return ret;
}
