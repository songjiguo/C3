#include <cos_component.h>
#include <cos_synchronization.h>
#include <print.h>

extern int lock_component_take(spdid_t spd, unsigned long lock_id, unsigned short int thd_id);
extern int lock_component_release(spdid_t spd, unsigned long lock_id);
extern int lock_component_pretake(spdid_t spd, unsigned long lock_id, unsigned short int thd);

int __sg_lock_component_pretake(spdid_t spd, unsigned long lock_id, unsigned short int thd)
{
	return lock_component_pretake(spd, lock_id, thd);
}

int __sg_lock_component_take(spdid_t spd, unsigned long lock_id, unsigned short int thd)
{
	return lock_component_take(spd, lock_id, thd);
}

int __sg_lock_component_release(spdid_t spd, unsigned long lock_id)
{
	return lock_component_release(spd, lock_id);
}
