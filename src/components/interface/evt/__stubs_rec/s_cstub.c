#include <cos_component.h>
#include <evt.h>

long __sg_evt_split(spdid_t spdid, long parent_evt, int grp)
{
	return evt_split(spdid, parent_evt, grp);
}

long __sg_evt_create(spdid_t spdid)
{
	return evt_create(spdid);
}

int __sg_evt_free(spdid_t spdid, long extern_evt)
{
	evt_free(spdid, extern_evt);
	return 0;
}

long __sg_evt_wait(spdid_t spdid, long extern_evt)
{
	return evt_wait(spdid, extern_evt);
}

int __sg_evt_trigger(spdid_t spdid, long extern_evt)
{
	return evt_trigger(spdid, extern_evt);
}
