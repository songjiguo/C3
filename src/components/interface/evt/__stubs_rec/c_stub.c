/* Event recovery client stub interface */
// This interface code should work for both group and non-group events

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <objtype.h>

#include <evt.h>
#include <cstub.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

#define EVT_PRINT 0

#if EVT_PRINT == 1
#define print_evt(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define print_evt(fmt,...)
#endif

/* global fault counter, only increase, never decrease */
static unsigned long fcounter;

/* recovery data structure evt service */
struct blocked_thd {
	int thd;
	struct blocked_thd *next, *prev;
};

struct rec_data_evt {
	spdid_t spdid;
	unsigned long p_evtid;  // for tsplit only

	unsigned long c_evtid;
	unsigned long s_evtid;

	unsigned long fcnt;
	struct blocked_thd blkthd;
};

/**********************************************/
/* slab allocaevt and cvect for tracking evts */
/**********************************************/

CVECT_CREATE_STATIC(rec_evt_vect);
CSLAB_CREATE(rdevt, sizeof(struct rec_data_evt));

void print_rdevt_info(struct rec_data_evt *rd);

static struct rec_data_evt *
rdevt_lookup(int id)
{ 
	return cvect_lookup(&rec_evt_vect, id); 
}

static struct rec_data_evt *
rdevt_alloc(int evtid)
{
	struct rec_data_evt *rd;

	rd = cslab_alloc_rdevt();
	assert(rd);
	cvect_add(&rec_evt_vect, rd, evtid);
	return rd;
}

static void
rdevt_dealloc(struct rec_data_evt *rd)
{
	assert(rd);
	cslab_free_rdevt(rd);
}

static void
rdevt_addblk(struct rec_data_evt *rd, struct blocked_thd *ptr_blkthd)
{
	assert(rd && ptr_blkthd);

	ptr_blkthd->thd = cos_get_thd_id();
	INIT_LIST(ptr_blkthd, next, prev);
	ADD_LIST(&rd->blkthd, ptr_blkthd, next, prev);
       
	return;
}


static int
get_unique(void)
{
	unsigned int i;
	cvect_t *v;

	v = &rec_evt_vect;
	for(i = 1 ; i <= CVECT_MAX_ID ; i++) {
		if (!cvect_lookup(v, i)) return i;
	}

	return -1;
}

static void 
rd_cons(struct rec_data_evt *rd, spdid_t spdid, unsigned long cli_evtid, unsigned long ser_evtid, unsigned long par_evtid)
{
	assert(rd);

	rd->spdid	 = spdid;
	rd->p_evtid      = par_evtid;
	rd->c_evtid	 = cli_evtid;
	rd->s_evtid	 = ser_evtid;

	rd->fcnt	 = fcounter;

	return;
}

#ifdef REFLECTION
static int
cap_to_dest(int cap)
{
	int dest = 0;
	assert(cap > MAX_NUM_SPDS);
	if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, cap)) <= 0) assert(0);
	return dest;
}
#endif

extern int sched_reflect(spdid_t spdid, int par);

static struct rec_data_evt *
update_rd(int evtid, int par_evtid, int cap)
{
	unsigned long s_tid = 0;
        struct rec_data_evt *rd = NULL;
	struct blocked_thd *blk_thd;

        rd = rdevt_lookup(evtid);
	if (unlikely(!rd)) goto done;
	if (likely(rd->fcnt == fcounter)) goto done;

	rd->s_evtid = evt_create(cos_spd_id());
	rd->fcnt = fcounter;

#ifdef REFLECTION
	int evt_spd = cap_to_dest(cap);
	sched_reflect(cos_spd_id(), evt_spd);
#else
	for (blk_thd = FIRST_LIST(&rd->blkthd, next, prev);
	     blk_thd != &rd->blkthd;
	     blk_thd = FIRST_LIST(blk_thd, next, prev)){
		sched_reflect(cos_spd_id(), blk_thd->thd);
	}
#endif
	
done:	
	return rd;
}

/************************************/
/******  client stub functions ******/
/************************************/

CSTUB_FN_ARGS_3(long, evt_split, spdid_t, spdid, long, parent_evt, int, grp)

        struct rec_data_evt *rd = NULL;
redo:

CSTUB_ASM_3(evt_split, spdid, parent_evt, grp)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
	       
       	       fcounter++;
       	       goto redo;
       }

CSTUB_POST

CSTUB_FN_ARGS_1(long, evt_create, spdid_t, spdid)

        struct rec_data_evt *rd = NULL;
        unsigned long ser_eid, cli_eid;

redo:

CSTUB_ASM_1(evt_create, spdid)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
	       
       	       fcounter++;
       	       goto redo;
       }

	if ((ser_eid = ret) > 0) {
		// if does exist, we need an unique id. Otherwise, create it
		if (unlikely(rdevt_lookup(ser_eid))) {
			cli_eid = get_unique();
			assert(cli_eid > 0 && cli_eid != ser_eid);
		} else {
			cli_eid = ser_eid;
		}
		rd = rdevt_alloc(cli_eid);
		assert(rd);
		rd_cons(rd, cos_spd_id(), cli_eid, ser_eid, 0);
		INIT_LIST(&rd->blkthd, next, prev);
		ret = cli_eid;
	}

CSTUB_POST

CSTUB_FN_ARGS_2(int, evt_free, spdid_t, spdid, long, extern_evt)

        struct rec_data_evt *rd = NULL;
redo:
        rd = update_rd(extern_evt, 0, uc->cap_no);
	if (!rd) {
		printc("try to free a non-existing evt\n");
		return -1;
	}

CSTUB_ASM_2(evt_free, spdid, rd->s_evtid)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
	       
       	       fcounter++;
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_2(long, evt_wait, spdid_t, spdid, long, extern_evt)

	struct blocked_thd blk_thd;
        struct rec_data_evt *rd = NULL;
redo:
        rd = update_rd(extern_evt, 0, uc->cap_no);
	if (!rd) {
		printc("try to wait for a non-existing evt\n");
		return -1;
	}

        rdevt_addblk(rd, &blk_thd);

CSTUB_ASM_2(evt_wait, spdid, rd->s_evtid)

        if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
	       
       	       fcounter++;
       	       goto redo;
        }

        REM_LIST(&blk_thd, next, prev);

CSTUB_POST


CSTUB_FN_ARGS_2(int, evt_trigger, spdid_t, spdid, long, extern_evt)

        struct rec_data_evt *rd = NULL;
redo:
        rd = update_rd(extern_evt, 0, uc->cap_no);
	if (!rd) {
		printc("try to trigger a non-existing tor\n");
		return -1;
	}

CSTUB_ASM_2(evt_trigger, spdid, rd->s_evtid)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
	       
       	       fcounter++;
       	       goto redo;
       }

CSTUB_POST
