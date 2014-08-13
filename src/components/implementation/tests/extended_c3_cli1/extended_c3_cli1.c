#include <cos_component.h>
#include <print.h>
#include <res_spec.h>
#include <sched.h>
#include <mem_mgr.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <ec3_ser1.h>

int high, low, med;
int warm;

#define ITER 5

#define EXAMINE_LOCK
//#define EXAMINE_EVT

#define US_PER_TICK 10000

void 
cos_init(void)
{
	static int first = 0;
	union sched_param sp;
	int i, j, k;
	
	if(first == 0){
		first = 1;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		warm = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 15;
		med = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 20;
		low = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

	} else {
#ifdef EXAMINE_LOCK
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			timed_event_block(cos_spd_id(), 5);
			ec3_ser1_test();
		}

		if (cos_get_thd_id() == med) {
			printc("<<<med thd %d>>>\n", cos_get_thd_id());
			timed_event_block(cos_spd_id(), 2);
			ec3_ser1_test();
		}

		if (cos_get_thd_id() == low) {
			printc("<<<low thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test();
		}
#endif

#ifdef EXAMINE_EVT
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test();
		}

		if (cos_get_thd_id() == med) {
			printc("<<<med thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test();
		}
#endif
		/* if (cos_get_thd_id() == warm) { */
		/* 	printc("<<<warm thd %d>>>\n", cos_get_thd_id()); */
		/* 	i = 0; */
		/* 	while(i++ < ITER) ec3_ser1_test(); */
		/* } */
		
	}

	return;
}
