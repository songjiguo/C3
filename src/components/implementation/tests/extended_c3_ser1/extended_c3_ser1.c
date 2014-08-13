#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>

#include <evt.h>

#include <periodic_wake.h>
#include <timed_blk.h>

static int low = 13;
static int mid = 12;
static int hig = 11;

#define EXAMINE_LOCK
//#define EXAMINE_EVT

#ifdef EXAMINE_LOCK

#include <cos_synchronization.h>
cos_lock_t t_lock;
#define LOCK_TAKE()    lock_take(&t_lock)
#define LOCK_RELEASE() lock_release(&t_lock)
#define LOCK_INIT()    lock_static_init(&t_lock);

volatile int spin = 1;

static void try_hp(void)
{
	printc("thread h : %d is doing something\n", cos_get_thd_id());

	spin = 0;
	printc("thread h : %d try to take lock\n", cos_get_thd_id());
	LOCK_TAKE();
	printc("thread h : %d has the lock\n", cos_get_thd_id());
	LOCK_RELEASE();
	printc("thread h : %d released lock\n", cos_get_thd_id());

	return;
}

static void try_mp(void)
{
	printc("thread m : %d try to take lock\n", cos_get_thd_id());
	LOCK_TAKE();
	printc("thread m : %d has the lock\n", cos_get_thd_id());
	LOCK_RELEASE();
	printc("thread m : %d released lock\n", cos_get_thd_id());

	return;
}


static void try_lp(void)
{
	printc("<<< thread l : %d is doing something \n", cos_get_thd_id());
	printc("thread l : %d try to take lock\n", cos_get_thd_id());
	LOCK_TAKE();
	printc("thread l : %d has the lock\n", cos_get_thd_id());
	printc("thread l : %d spinning\n", cos_get_thd_id());
	while (spin);
	printc("thread l : %d is doing something\n", cos_get_thd_id());
	printc("thread l : %d try to release lock\n", cos_get_thd_id());
	LOCK_RELEASE();

	return;
}

vaddr_t ec3_ser1_test(void)
{
	if (cos_get_thd_id() == hig) try_hp();
	if (cos_get_thd_id() == mid) try_mp();
	if (cos_get_thd_id() == low) try_lp();

	return 0;
}


void
cos_init(void)
{
	printc("thd %d is trying to init lock\n", cos_get_thd_id());
	LOCK_INIT();
	printc("after init LOCK\n");
}

#endif


#ifdef EXAMINE_EVT

static void try_hp(void)
{
	printc("thread h : %d is doing something\n", cos_get_thd_id());
	long evt1, evt2;
	
	evt1 = evt_create(cos_spd_id());
	evt2 = evt_create(cos_spd_id());
	assert(evt1 > 0 && evt2 > 0);	
	return;
}

static void try_mp(void)
{
	printc("thread m : %d is doing nothing\n", cos_get_thd_id());

	return;
}

vaddr_t ec3_ser1_test(void)
{
	if (cos_get_thd_id() == hig) try_hp();
	if (cos_get_thd_id() == mid) try_mp();
	return 0;
}

#endif


