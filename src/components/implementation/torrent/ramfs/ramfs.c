/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <torrent.h>
#include <torlib.h>

#include <cbuf.h>
#include <print.h>
#include <cos_synchronization.h>
#include <evt.h>
#include <cos_alloc.h>
#include <cos_map.h>
#include <fs.h>

#include <pgfault.h>

static cos_lock_t fs_lock;
struct fsobj root;
#define LOCK() if (lock_take(&fs_lock)) BUG();
#define UNLOCK() if (lock_release(&fs_lock)) BUG();

#define MIN_DATA_SZ 256

#define TSPLIT_FAULT
//#define TWRITE_FAULT
//#define TREAD_FAULT
//#define TRELEASE_FAULT

static int aaa = 0;

td_t __tsplit(spdid_t spdid, td_t tid, char *param, int len, 
	      tor_flags_t tflags, long evtid, td_t desired_tid)
{
	td_t ret = 0;
	struct torrent *tor;

	if (desired_tid == -10) aaa = 1;	/* control the fault, test purpose only */

	/* if (desired_tid) { */
	/* 	printc("desire tid %d\n", desired_tid); */
	/* 	tor = tor_lookup(desired_tid); */
	/* 	if (tor) return -1; */
	/* } */
	/* printc("call tsplit from td %d\n", tid); */
	ret = tsplit(spdid, tid, param, len, tflags, evtid);
done:
	return ret;
}

td_t 
tsplit(spdid_t spdid, td_t td, char *param, 
       int len, tor_flags_t tflags, long evtid) 
{
	td_t ret = -1;
	struct torrent *t, *nt;
	struct fsobj *fso, *fsc, *parent; /* obj, child, and parent */
	char *subpath;

#ifdef TSPLIT_FAULT
	if (aaa == 1){
		printc("<< tsplit: Before Assert >> \n");
		assert(0);
	}
#endif
	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	fso = t->data;

	fsc = fsobj_path2obj(param, len, fso, &parent, &subpath);
	if (!fsc) {
		assert(parent);
		if (!(parent->flags & TOR_SPLIT)) ERR_THROW(-EACCES, done);
		fsc = fsobj_alloc(subpath, parent);
		if (!fsc) ERR_THROW(-EINVAL, done);
		fsc->flags = tflags;
	} else {
		/* File has less permissions than asked for? */
		if ((~fsc->flags) & tflags) ERR_THROW(-EACCES, done);
	}

	fsobj_take(fsc);
	nt = tor_alloc(fsc, tflags);
	if (!nt) ERR_THROW(-ENOMEM, free);
	ret = nt->td;
done:
	UNLOCK();
	return ret;
free:  
	fsobj_release(fsc);
	goto done;
}

int 
twmeta(spdid_t spdid, td_t td, cbuf_t cb, int sz, int offset, int flag)
{
	int ret = -1, left;
	struct torrent *t;
	struct fsobj *fso;
	char *buf, *test;

	int buf_sz;
	u32_t id;
	cbuf_unpack(cb, &id, (u32_t*)&buf_sz);

	if (tor_isnull(td)) return -EINVAL;

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	assert(t->data);
	fso = t->data;
	assert(fso->size <= fso->allocated);
	assert(t->offset <= fso->size);

	if (flag == 1) { 		/* recovery in the order*/
		/* printc("look up cbuf of id %d\n", id); */
		buf = cbuf_c_introspect(cos_spd_id(), id, CBUF_INTRO_PAGE);
		if (!buf) ERR_THROW(-EINVAL, done);

		left = fso->allocated - t->offset;
		if (left >= sz) {
			ret = sz;
			if (fso->size < (t->offset + sz)) fso->size = t->offset + sz;
		} else {
			char *new;
			int new_sz;

			new_sz = fso->allocated == 0 ? MIN_DATA_SZ : fso->allocated * 2;
			new    = malloc(new_sz);
			if (!new) ERR_THROW(-ENOMEM, done);
			if (fso->data) {
				memcpy(new, fso->data, fso->size);
				free(fso->data);
			}

			fso->data      = new;
			fso->allocated = new_sz;
			left           = new_sz - t->offset;
			ret            = left > sz ? sz : left;
			fso->size      = t->offset + ret;
		}
		memcpy(fso->data + t->offset, buf, ret);
		t->offset += ret;
	}

done:	
	UNLOCK();
	return ret;
}

int 
tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len)
{
	struct torrent *t;
	int ret = 0;

	if (!tor_is_usrdef(td)) return -1;

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	/* currently only allow deletion */
	if (td_into != td_null) ERR_THROW(-EINVAL, done);

	tor_free(t);
done:   
	UNLOCK();
	return ret;
}


static int ddd = 0;

void
trelease(spdid_t spdid, td_t td)
{
	struct torrent *t;

	if (!tor_is_usrdef(td)) return;

#ifdef TRELEASE_FAULT
	if (ddd++ == 2){
		printc("<< trelease: Before Assert >> \n");
		assert(0);
	}
#endif

	LOCK();
	t = tor_lookup(td);
	if (!t) goto done;
	fsobj_release((struct fsobj *)t->data);
	tor_free(t);
done:
	UNLOCK();
	return;
}

static int bbb = 0;

int 
tread(spdid_t spdid, td_t td, int cbid, int sz)
{
	int ret = -1, left;
	struct torrent *t;
	struct fsobj *fso;
	char *buf;

	if (tor_isnull(td)) return -EINVAL;

#ifdef TREAD_FAULT
	if (bbb++ == 2){
		printc("<< tread: Before Assert >> \n");
		assert(0);
	}
#endif
	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	assert(!tor_is_usrdef(td) || t->data);
	if (!(t->flags & TOR_READ)) ERR_THROW(-EACCES, done);

	fso = t->data;
	assert(fso->size <= fso->allocated);
	assert(t->offset <= fso->size);
	if (!fso->size) ERR_THROW(0, done);

	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);

	left = fso->size - t->offset;
	ret  = left > sz ? sz : left;

	assert(fso->data);
	memcpy(buf, fso->data + t->offset, ret);
	t->offset += ret;
done:	
	UNLOCK();
	return ret;
}

static int ccc = 0;

int 
twrite(spdid_t spdid, td_t td, int cbid, int sz)
{
	int ret = -1, left;
	struct torrent *t;
	struct fsobj *fso;
	char *buf;

	int buf_sz;
	u32_t id;
	cbuf_unpack(cbid, &id, (u32_t*)&buf_sz);

	if (tor_isnull(td)) return -EINVAL;

#ifdef TWRITE_FAULT
	if (ccc++ == 2){
		printc("<< twrite: Before Assert >> \n");
		assert(0);
	}
#endif
	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	assert(t->data);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, done);

	fso = t->data;
	assert(fso->size <= fso->allocated);
	assert(t->offset <= fso->size);

	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);
	
	left = fso->allocated - t->offset;
	if (left >= sz) {
		ret = sz;
		if (fso->size < (t->offset + sz)) fso->size = t->offset + sz;
	} else {
		char *new;
		int new_sz;

		new_sz = fso->allocated == 0 ? MIN_DATA_SZ : fso->allocated * 2;
		new    = malloc(new_sz);
		if (!new) ERR_THROW(-ENOMEM, done);
		if (fso->data) {
			memcpy(new, fso->data, fso->size);
			free(fso->data);
		}

		fso->data      = new;
		fso->allocated = new_sz;
		left           = new_sz - t->offset;
		ret            = left > sz ? sz : left;
		fso->size      = t->offset + ret;
	}
	memcpy(fso->data + t->offset, buf, ret);
	t->offset += ret;

	/* printc("found a buf @ %p\n", buf); */
	/* printc("[[[[[ twrite : "); */
	/* int i; */
	/* for (i = 0; i < 10; i++){ */
	/* 	printc("%c", *(buf++)); */
	/* } */
	/* printc("]]]]]\n\n"); */

	/* update the cbuf owner to ramfs */
	/* should only the owner can free/revoke the cbuf */
	if (cbuf_c_claim(cos_spd_id(), id)) BUG();
done:	
	UNLOCK();
	return ret;
}

int cos_init(void)
{
	lock_static_init(&fs_lock);
	torlib_init();

	fs_init_root(&root);
	root_torrent.data = &root;
	root.flags = TOR_READ | TOR_SPLIT;

	return 0;
}
