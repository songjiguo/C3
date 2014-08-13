#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!l.o,a1;!te.o,a3;!e.o,a4;!ec3cli1.o, ;!ec3ser1.o, ;!pfr.o, ;!va.o,a2:\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o|[faulthndlr_]llboot.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
\
va.o-fprr.o|print.o|mm.o|l.o|boot.o;\
l.o-fprr.o|mm.o|print.o|pfr.o;\
pfr.o-fprr.o|mm.o|print.o|boot.o;\
e.o-fprr.o|print.o|mm.o|l.o|va.o;\
te.o-print.o|fprr.o|mm.o|va.o;\
\
ec3cli1.o-print.o|va.o|fprr.o|l.o|mm.o|ec3ser1.o|te.o;\
ec3ser1.o-print.o|l.o|va.o|fprr.o|mm.o|te.o|e.o\
" ./gen_client_stub
