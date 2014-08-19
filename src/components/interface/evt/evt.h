#ifndef   	EVT_H
#define   	EVT_H

typedef enum {
	EVT_NIL   = 0,
	EVT_READ  = 0x1, 
	EVT_WRITE = 0x2, 
	EVT_SPLIT = 0x4, 
	EVT_MERGE = 0x8, 
	EVT_ALL   = (EVT_READ|EVT_WRITE|EVT_SPLIT|EVT_MERGE)
} evt_flags_t;

// The following APIs have fault tolerance support
long evt_split(spdid_t spdid, long parent_evt, int grp);
long evt_create(spdid_t spdid);
void evt_free(spdid_t spdid, long extern_evt);
long evt_wait(spdid_t spdid, long extern_evt);
int evt_trigger(spdid_t spdid, long extern_evt);

// The following APIs have no fault tolerance support
long evt_create(spdid_t spdid);

long __evt_create(spdid_t spdid); // get a new server side id  :Jiguo
int evt_update_status(spdid_t spdid, long extern_evt);  // after reflection, Jiguo

long evt_grp_wait(spdid_t spdid);
int evt_grp_mult_wait(spdid_t spdid, struct cos_array *data);
int evt_set_prio(spdid_t spdid, long extern_evt, int prio);
unsigned long *evt_stats(spdid_t spdid, unsigned long *stats);
int evt_stats_len(spdid_t spdid);

#endif 	    /* !EVT_H */
