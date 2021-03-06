/**
 * Copyright 2009 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */


#include <cos_asm_server_stub_simple_stack.h>

.text	

cos_asm_server_stub_spdid(mman_get_page)
cos_asm_server_stub(mman_release_page)
cos_asm_server_stub_spdid(mman_revoke_page)
cos_asm_server_stub_spdid(mman_alias_page)
cos_asm_server_stub(mman_print_stats)

//cos_asm_server_fn_stub_spdid(mman_get_page, __sg_mman_get_page)
//cos_asm_server_fn_stub_spdid(mman_revoke_page, __sg_mman_revoke_page)

cos_asm_server_fn_stub_spdid(mman_reflect, __sg_mman_reflect)
