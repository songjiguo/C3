#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <evt.h>
#include <torrent.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <ramfs_test2.h>

/* Military Slang: SNAFU, SUSFU, FUBAR, TARFU, BOCHIA */
/* based on the code from unit test for ramfs */

#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif

char buffer[1024];

int high, low;

void test0(void)
{
	td_t t1, t2;
	long evt1, evt2;
	char *params1 = "bar";
	char *params2 = "foo/";
	char *params3 = "foo/bar";
	char *data1 = "1234567890", *data2 = "asdf;lkj", *data3 = "asdf;lkj1234567890";
	unsigned int ret1, ret2;

	evt1 = evt_create(cos_spd_id());
	evt2 = evt_create(cos_spd_id());
	assert(evt1 > 0 && evt2 > 0);


	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("UNIT TEST FAILED: split failed %d\n", t1);
		return;
	}
	trelease(cos_spd_id(), t1);

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("UNIT TEST FAILED: split2 failed %d\n", t1); return;
	}
	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	if (t2 < 1) {
		printc("UNIT TEST FAILED: split3 failed %d\n", t2); return;
	}

	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1));
	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2));
	printv("write %d & %d, ret %d & %d\n", strlen(data1), strlen(data2), ret1, ret2);

	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	if (t1 < 1 || t2 < 1) {
		printc("UNIT TEST FAILED: later splits failed\n");
		return;
	}
	
	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	printv("read %d (%d): %s (%s)\n", ret1, strlen(data1), buffer, data1);
	assert(!strcmp(buffer, data1));
	assert(ret1 == strlen(data1));
	buffer[0] = '\0';

	ret1 = tread_pack(cos_spd_id(), t2, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	assert(!strcmp(buffer, data2));
	assert(ret1 == strlen(data2));
	printv("read %d: %s\n", ret1, buffer);
	buffer[0] = '\0';

	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);

	return;
}


void test1(void)
{
	td_t t1, t2, t0;
	long evt1, evt2, evt0;
	char *params0, *params1, *params2, *params3;
	char *data0, *data1, *data2, *data3;
	int ret1, ret2, ret0;
	char *strl, *strh;

	params0 = "who";
	params1 = "bar1234567/";
	params2 = "foo/";
	params3 = "foo/bar1234567/who";
	data0 = "lkevinandy";
	data1 = "l0987654321";
	data2 = "laabbccddeeff";
	data3 = "laabbccddeeffl0987654321";
	strl = "testmore_l";

	char *merge = "delete";

	/* printc("\n<<< TEST 1 START (thread %d)>>>\n", cos_get_thd_id()); */

	evt1 = evt_create(cos_spd_id());
	evt2 = evt_create(cos_spd_id());
	evt0 = evt_create(cos_spd_id());
	assert(evt1 > 0 && evt2 > 0 && evt0 > 0);

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("  split2 failed %d\n", t1); return;
	}

	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	if (t2 < 1) {
		printc("  split1 failed %d\n", t2); return;
	}

	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0);
	if (t0 < 1) {
		printc("  split0 failed %d\n", t0); return;
	}


	printc("\nthread %d writes str %s in %s\n", cos_get_thd_id(), data1, params2);
	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1));

	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2));
	printc("thread %d writes str %s in %s%s\n", cos_get_thd_id(), data2, params2, params1);
	
	ret2 = twrite_pack(cos_spd_id(), t2, strl, strlen(strl));
	printc("thread %d writes str %s in %s%s\n", cos_get_thd_id(), strl, params2, params1);

	ret0 = twrite_pack(cos_spd_id(), t0, data0, strlen(data0));
	printc("thread %d writes str %s in %s%s%s\n", cos_get_thd_id(), data0, params2, params1, params0);

	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t0);

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0);

	if (t1 < 1 || t2 < 1 || t0 < 1) {
		printc("later splits failed\n");
		return;
	}

	if (tmerge(cos_spd_id(), t1, td_null, merge , strlen(merge))) printc("NO! can not delete %s!!\n", params2);
	if (tmerge(cos_spd_id(), t2, td_null, merge , strlen(merge))) printc("NO! can not delete %s!!\n", params1);
	if (tmerge(cos_spd_id(), t0, td_null, merge , strlen(merge))) printc("NO! can not delete %s!!\n", params0);

	/* t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1); */
	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
	if (ret1 > 0 && ret1 <= 1023) buffer[ret1] = '\0';
	printv("thread %d read %d (%d): %s (%s)\n", cos_get_thd_id(),  ret1, strlen(data1), buffer, data1);
	buffer[0] = '\0';
	
	/* t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2); */
	ret2 = tread_pack(cos_spd_id(), t2, buffer, 1023);
	if (ret2 > 0 && ret2 <= 1023) buffer[ret2] = '\0';
	printv("thread %d read %d: %s\n", cos_get_thd_id(), ret2, buffer);
	buffer[0] = '\0';

	/* t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0); */
	ret0 = tread_pack(cos_spd_id(), t0, buffer, 1023);
	if (ret0 < 0){printc("No such file\n");}
	if (ret0 > 0 && ret0 <= 1023) {
		buffer[ret0] = '\0';
		printv("thread %d read %d: %s\n", cos_get_thd_id(), ret0, buffer);
		buffer[0] = '\0';
	}
		
	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t0);

	/* printc("<<< TEST 1 PASSED (thread %d)>>>\n\n", cos_get_thd_id()); */

	return;
}


void test2(void)
{
	td_t t1, t2, t0;
	long evt1, evt2, evt0;
	char *params0, *params1, *params2, *params3;
	char *data0, *data1, *data2, *data3;
	int ret1, ret2, ret0;
	char *strl, *strh;

	params0 = "who";
	params1 = "barasd/";
	params2 = "foo/";
	params3 = "foo/barasd/who";
	data0 = "kevinandy";
	data1 = "0987654321";
	data2 = "aabbccddeeff";
	data3 = "aabbccddeeff0987654321";
	strl = "testmore";

	evt1 = evt_create(cos_spd_id());
	evt2 = evt_create(cos_spd_id());
	evt0 = evt_create(cos_spd_id());
	assert(evt1 > 0 && evt2 > 0 && evt0 > 0);

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("  split2 failed %d\n", t1); return;
	}

	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	if (t2 < 1) {
		printc("  split1 failed %d\n", t2); return;
	}

	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0);
	if (t0 < 1) {
		printc("  split0 failed %d\n", t0); return;
	}

	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1));
	printc("thread %d writes str %s in %s\n", cos_get_thd_id(), data1, params2);

	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2));
	printc("thread %d writes str %s in %s%s\n", cos_get_thd_id(), data2, params2, params1);
	
	ret2 = twrite_pack(cos_spd_id(), t2, strl, strlen(strl));
	printc("thread %d writes str %s in %s%s\n", cos_get_thd_id(), strl, params2, params1);

	ret0 = twrite_pack(cos_spd_id(), t0, data0, strlen(data0));
	printc("thread %d writes str %s in %s%s%s\n", cos_get_thd_id(), data0, params2, params1, params0);

	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t0);

	/* let low wake up high here */
	if (cos_get_thd_id() == low) {
		printc("thread %d is going to wakeup thread %d\n", low, high);
		sched_wakeup(cos_spd_id(), high);
	}

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0);
	if (t1 < 1 || t2 < 1 || t0 < 1) {
		printc("later splits failed\n");
		return;
	}

	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	printv("thread %d read %d (%d): %s (%s)\n", cos_get_thd_id(),  ret1, strlen(data1), buffer, data1);
	buffer[0] = '\0';
	
	ret2 = tread_pack(cos_spd_id(), t2, buffer, 1023);
	if (ret2 > 0) buffer[ret2] = '\0';
	printv("thread %d read %d: %s\n", cos_get_thd_id(), ret2, buffer);
	buffer[0] = '\0';

	ret0 = tread_pack(cos_spd_id(), t0, buffer, 1023);
	if (ret0 > 0) buffer[ret0] = '\0';
	printv("thread %d read %d: %s\n", cos_get_thd_id(), ret0, buffer);
	buffer[0] = '\0';
		
	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t0);

	return;
}


void test3(void)
{
	td_t t1, t2, t0;
	long evt1, evt2, evt0;
	char *params0, *params1, *params2, *params3;
	char *data0, *data1, *data2, *data3;
	int ret1, ret2, ret0;
	char *strl, *strh;

	if (cos_get_thd_id() == high) {
		params0 = "who";
		params1 = "bbrabc/";
		params2 = "foo/";
		params3 = "foo/bbrabc/who";
		data0 = "handykevin";
		data1 = "h1234567890";
		data2 = "hasdf;lkj";
		data3 = "hasdf;lkjh1234567890";
		strh = "testmore_h";
	}


	if (cos_get_thd_id() == low) {
		params0 = "who";
		params1 = "bar/";
		params2 = "foo/";
		params3 = "foo/bar/who";
		data0 = "lkevinandy";
		data1 = "l0987654321";
		data2 = "laabbccddeeff";
		data3 = "laabbccddeeffl0987654321";
		strl = "testmore_l";
	}

	evt1 = evt_create(cos_spd_id());
	evt2 = evt_create(cos_spd_id());
	evt0 = evt_create(cos_spd_id());
	assert(evt1 > 0 && evt2 > 0 && evt0 > 0);

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("  split2 failed %d\n", t1); return;
	}

	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	if (t2 < 1) {
		printc("  split1 failed %d\n", t2); return;
	}

	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0);
	if (t0 < 1) {
		printc("  split0 failed %d\n", t0); return;
	}

	printc("\nthread %d writes str %s in %s\n", cos_get_thd_id(), data1, params2);
	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1));

	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2));
	printc("thread %d writes str %s in %s%s\n", cos_get_thd_id(), data2, params2, params1);
	
	if (cos_get_thd_id() == low) {
		ret2 = twrite_pack(cos_spd_id(), t2, strl, strlen(strl));
		printc("thread %d writes str %s in %s%s\n", cos_get_thd_id(), strl, params2, params1);
	}
	if (cos_get_thd_id() == high) {
		ret2 = twrite_pack(cos_spd_id(), t2, strh, strlen(strh));
		printc("thread %d writes str %s in %s%s\n", cos_get_thd_id(), strh, params2, params1);
	}

	ret0 = twrite_pack(cos_spd_id(), t0, data0, strlen(data0));
	printc("thread %d writes str %s in %s%s%s\n", cos_get_thd_id(), data0, params2, params1, params0);

	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t0);

	/* let low wake up high here */
	if (cos_get_thd_id() == low) {
		printc("thread %d is going to wakeup thread %d\n", low, high);
		sched_wakeup(cos_spd_id(), high);
	}

#ifdef TEST_4
	ramfs_test2();  	/* all another component to mimic the situation */
#endif

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0);
	if (t1 < 1 || t2 < 1 || t0 < 1) {
		printc("later splits failed\n");
		return;
	}

	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	printv("thread %d read %d (%d): %s (%s)\n", cos_get_thd_id(),  ret1, strlen(data1), buffer, data1);
	buffer[0] = '\0';
	
	ret2 = tread_pack(cos_spd_id(), t2, buffer, 1023);
	if (ret2 > 0) buffer[ret2] = '\0';
	printv("thread %d read %d: %s\n", cos_get_thd_id(), ret2, buffer);
	buffer[0] = '\0';

	ret0 = tread_pack(cos_spd_id(), t0, buffer, 1023);
	if (ret0 > 0) buffer[ret0] = '\0';
	printv("thread %d read %d: %s\n", cos_get_thd_id(), ret0, buffer);
	buffer[0] = '\0';
		
	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t0);

	return;
}


void cos_init(void)
{
	static int first = 0;
	union sched_param sp;
	
	if(first == 0){
		first = 1;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 12;
		low = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
	} else {

#ifdef TEST_0
	if (cos_get_thd_id() == high) {
		timed_event_block(cos_spd_id(), 1);
		/* for fault injection, this is too slow, instead, we can just loop */
		/* periodic_wake_create(cos_spd_id(), 1); */
		while(1) {
			test0(); /* unit test */
			/* periodic_wake_wait(cos_spd_id()); */
		}
	}
#endif

#ifdef TEST_1
	if (cos_get_thd_id() == high) {
		timed_event_block(cos_spd_id(), 1);
		periodic_wake_create(cos_spd_id(), 2);
		while(1) {
			test1();
			periodic_wake_wait(cos_spd_id());
		}
	}
#endif

#ifdef TEST_2
		if (cos_get_thd_id() == high) sched_block(cos_spd_id(), 0);
		test2();
#endif

#ifdef TEST_3
		if (cos_get_thd_id() == high) sched_block(cos_spd_id(), 0);
		test3();
#endif

	}
	
	return;
}