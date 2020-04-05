/*
 * Bytecode interpreter implemented in C.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#define NDEBUG

#include <stdint.h>
#include <stdlib.h>

#include "lj_bc.h"
#include "lj_bcins.h"
#include "lj_frame.h"
#include "lj_obj.h"
#include "uj_lib.h"

#define vm_assert(x) ((x)? (void)0 : abort())

/* FIXME: Align with x2 API from lj_bc.h */

#define vm_raw_ra(ins) (((ins) >> 8) & 0xff)
#define vm_raw_rb(ins) ((ins) >> 24)
#define vm_raw_rc(ins) (((ins) >> 16) & 0xff)
#define vm_raw_rd(ins) (((ins) >> 16))

#define vm_ra(ins) (vm_raw_ra(ins) * (sizeof(TValue) / 2))
#define vm_rb(ins) (vm_raw_rb(ins) * (sizeof(TValue) / 2))
#define vm_rc(ins) (vm_raw_rc(ins) * (sizeof(TValue) / 2))
#define vm_rd(ins) (vm_raw_rd(ins) * (sizeof(TValue) / 2))

/* FIXME: Apply base2func everywhere */
#define base2func(base) ((((base) - 1)->fr.func)->fn)
#define pc2proto(pc) ((const GCproto *)(pc) - 1)

struct vm_frame {
	lua_State *L;
	int64_t nargs;
	int64_t nres1;
	void *kbase;
};

typedef void *(* bc_handler)(BCIns, BCIns *, TValue *, struct vm_frame *);

/* NB! For development purposes only: */
#define HANDLER_SIGNATURE \
	BCIns ins, BCIns *pc, TValue *base, struct vm_frame *vmf

#define HANDLER_ARGUMENTS ins, pc, base, vmf

static void *uj_BC_NYI(HANDLER_SIGNATURE);
static void *uj_BC_MOV(HANDLER_SIGNATURE);
static void *uj_BC_ADD(HANDLER_SIGNATURE);
static void *uj_BC_KSHORT(HANDLER_SIGNATURE);
static void *uj_BC_KNUM(HANDLER_SIGNATURE);
static void *uj_BC_GGET(HANDLER_SIGNATURE);
static void *uj_BC_CALL(HANDLER_SIGNATURE);
static void *uj_BC_RET0(HANDLER_SIGNATURE);
static void *uj_BC_IFUNCF(HANDLER_SIGNATURE);
static void *uj_BC_HOTCNT(HANDLER_SIGNATURE);
static void *uj_BC_FORI(HANDLER_SIGNATURE);

/*
 * FORL
 * RET0
 */

static const bc_handler dispatch[] = {
	uj_BC_NYI, /* 0x00 ISLT */
	uj_BC_NYI, /* 0x01 ISGE */
	uj_BC_NYI, /* 0x02 ISLE */
	uj_BC_NYI, /* 0x03 ISGT */
	uj_BC_NYI, /* 0x04 ISEQV */
	uj_BC_NYI, /* 0x05 ISNEV */
	uj_BC_NYI, /* 0x06 ISEQS */
	uj_BC_NYI, /* 0x07 ISNES */
	uj_BC_NYI, /* 0x08 ISEQN */
	uj_BC_NYI, /* 0x09 ISNEN */
	uj_BC_NYI, /* 0x0a ISEQP */
	uj_BC_NYI, /* 0x0b ISNEP */
	uj_BC_NYI, /* 0x0c ISTC */
	uj_BC_NYI, /* 0x0d ISFC */
	uj_BC_NYI, /* 0x0e IST */
	uj_BC_NYI, /* 0x0f ISF */
	uj_BC_MOV, /* 0x10 MOV */
	uj_BC_NYI, /* 0x11 NOT */
	uj_BC_NYI, /* 0x12 UNM */
	uj_BC_NYI, /* 0x13 LEN */
	uj_BC_ADD, /* 0x14 ADD */
	uj_BC_NYI, /* 0x15 SUB */
	uj_BC_NYI, /* 0x16 MUL */
	uj_BC_NYI, /* 0x17 DIV */
	uj_BC_NYI, /* 0x18 MOD */
	uj_BC_NYI, /* 0x19 POW */
	uj_BC_NYI, /* 0x1a CAT */
	uj_BC_NYI, /* 0x1b KSTR */
	uj_BC_NYI, /* 0x1c KCDATA */
	uj_BC_KSHORT, /* 0x1d KSHORT */
	uj_BC_KNUM, /* 0x1e KNUM */
	uj_BC_NYI, /* 0x1f KPRI */
	uj_BC_NYI, /* 0x20 KNIL */
	uj_BC_NYI, /* 0x21 UGET */
	uj_BC_NYI, /* 0x22 USETV */
	uj_BC_NYI, /* 0x23 USETS */
	uj_BC_NYI, /* 0x24 USETN */
	uj_BC_NYI, /* 0x25 USETP */
	uj_BC_NYI, /* 0x26 UCLO */
	uj_BC_NYI, /* 0x27 FNEW */
	uj_BC_NYI, /* 0x28 TNEW */
	uj_BC_NYI, /* 0x29 TDUP */
	uj_BC_GGET, /* 0x2a GGET */
	uj_BC_NYI, /* 0x2b GSET */
	uj_BC_NYI, /* 0x2c TGETV */
	uj_BC_NYI, /* 0x2d TGETS */
	uj_BC_NYI, /* 0x2e TGETB */
	uj_BC_NYI, /* 0x2f TSETV */
	uj_BC_NYI, /* 0x30 TSETS */
	uj_BC_NYI, /* 0x31 TSETB */
	uj_BC_NYI, /* 0x32 TSETM */
	uj_BC_NYI, /* 0x33 CALLM */
	uj_BC_CALL, /* 0x34 CALL */
	uj_BC_NYI, /* 0x35 CALLMT */
	uj_BC_NYI, /* 0x36 CALLT */
	uj_BC_NYI, /* 0x37 ITERC */
	uj_BC_NYI, /* 0x38 ITERN */
	uj_BC_NYI, /* 0x39 VARG */
	uj_BC_NYI, /* 0x3a ISNEXT */
	uj_BC_NYI, /* 0x3b RETM */
	uj_BC_NYI, /* 0x3c RET */
	uj_BC_RET0, /* 0x3d RET0 */
	uj_BC_NYI, /* 0x3e RET1 */
	uj_BC_HOTCNT, /* 0x3f HOTCNT */
	uj_BC_NYI, /* 0x40 COVERG */
	uj_BC_FORI, /* 0x41 FORI */
	uj_BC_NYI, /* 0x42 JFORI */
	uj_BC_NYI, /* 0x43 FORL */
	uj_BC_NYI, /* 0x44 IFORL */
	uj_BC_NYI, /* 0x45 JFORL */
	uj_BC_NYI, /* 0x46 ITERL */
	uj_BC_NYI, /* 0x47 IITERL */
	uj_BC_NYI, /* 0x48 JITERL */
	uj_BC_NYI, /* 0x49 ITRNL */
	uj_BC_NYI, /* 0x4a IITRNL */
	uj_BC_NYI, /* 0x4b JITRNL */
	uj_BC_NYI, /* 0x4c LOOP */
	uj_BC_NYI, /* 0x4d ILOOP */
	uj_BC_NYI, /* 0x4e JLOOP */
	uj_BC_NYI, /* 0x4f JMP */
	uj_BC_IFUNCF, /* 0x50 FUNCF */ /* FIXME */
	uj_BC_IFUNCF, /* 0x51 IFUNCF */
	uj_BC_NYI, /* 0x52 JFUNCF */
	uj_BC_NYI, /* 0x53 FUNCV */
	uj_BC_NYI, /* 0x54 IFUNCV */
	uj_BC_NYI, /* 0x55 JFUNCV */
	uj_BC_NYI, /* 0x56 FUNCC */
	uj_BC_NYI, /* 0x57 FUNCCW */
};

#define vm_next(pc, base, vmf) \
	(dispatch[bc_op(*(pc))])(*(pc), (pc) + 1, (base), (vmf))

/* NB! For development purposes only: */
#define DISPATCH() \
	return vm_next(pc, base, vmf)

void uj_vm_call(lua_State *L, int nargs, int nres)
{
	const GCfunc *fn;
	const GCproto *pt;
	struct vm_frame vmf = {0};

	fn = uj_lib_checkfunc(L, 1);

	vm_assert(isluafunc(fn));
	pt = funcproto(fn);

	vmf.L = L;
	vmf.nargs = nargs;
	vmf.nres1 = nres + 1;
	/* NB! kbase will be set up in the prologue */

	L->base->fr.tp.ftsz = (int64_t)(FRAME_C | sizeof(TValue));

	vm_next(proto_bc(pt), L->base + 1, &vmf);
}

static void *vm_return(HANDLER_SIGNATURE)
{
	int8_t ra = vm_ra(ins);
	int8_t rd = vm_rd(ins);
	lua_State *L = vmf->L;

	for (ptrdiff_t i = -1; i < (ptrdiff_t)rd - 2; i++)
		*(base + i) = *(base + i + ra + 1);

	if (vmf->nres1 == 0 || vmf->nres1 > rd) { /* 0 == LUA_MULTRET + 1 */
		/* NYI: More results wanted. Check stack size and fill up results with nil. */
		vm_assert(0);
	}

	/* Set stack top: */
	L->top = base + vmf->nres1 - 2;

	/* Set stack base: */
	L->base = (TValue *)((char *)base - ((uint64_t)(uintptr_t)pc & (uint64_t)(-8)));

	/* NYI: Restore L->cframe */
	/* NYI: Set exit code */
	/* NYI: set_vmstate INTERP */
	/* NYI: restore_vmstate */

	return NULL;
}

static void *uj_BC_NYI(HANDLER_SIGNATURE)
{
	UNUSED(ins);
	UNUSED(pc);
	UNUSED(base);
	UNUSED(vmf);

	vm_assert(0);
	return NULL;
}

static void *uj_BC_MOV(HANDLER_SIGNATURE)
{
	TValue *dst = base + (ptrdiff_t)vm_ra(ins);
	TValue *src = base + (ptrdiff_t)vm_rd(ins);

	*dst = *src;

	DISPATCH();
}

static void *uj_BC_ADD(HANDLER_SIGNATURE)
{
	TValue *dst = base + (ptrdiff_t)(vm_raw_ra(ins) * sizeof(TValue) / 2);
	TValue *op1 = base + (ptrdiff_t)(vm_raw_rb(ins) * sizeof(TValue) / 2);
	TValue *op2 = base + (ptrdiff_t)(vm_raw_rc(ins) * sizeof(TValue) / 2);

	if (LJ_UNLIKELY(!tvisnum(op1) || !tvisnum(op2))) {
		/* FIXME: Implement metacall */
	}

	setnumV(dst, numV(op1) + numV(op2));
	DISPATCH();
}

static void *uj_BC_KSHORT(HANDLER_SIGNATURE)
{
	TValue *dst = base + (ptrdiff_t)vm_ra(ins);
	int16_t i16 = (int16_t)vm_raw_rd(ins);

	setnumV(dst, (double)i16);

	DISPATCH();
}

static void *uj_BC_KNUM(HANDLER_SIGNATURE)
{
	TValue *dst = base + (ptrdiff_t)vm_ra(ins);
	TValue *src = (TValue *)vmf->kbase + (ptrdiff_t)vm_rd(ins); /* FIXME */

	*dst = *src;

	DISPATCH();
}

static void *uj_BC_GGET(HANDLER_SIGNATURE)
{
	GCtab *tab = base2func(base).l.env;
	GCstr *str = (GCstr *)vmf->kbase + (ptrdiff_t)~vm_raw_rd(ins); /* FIXME */
	Node *n = &(tab->node[tab->hmask & str->hash]);

	do {
		TValue *dst;

		if (tvisstr(&n->key) && strV(&n->key) == str) {
			if (tvisnil(&n->val))
				goto key_not_found;

			dst = base + (ptrdiff_t)vm_ra(ins);
			*dst = n->val;
			DISPATCH();
		}
	} while ((n = n->next));

key_not_found:
	vm_assert(0);
	DISPATCH();
	/* NYI: Implement nil handling, metamethod lookup, etc. */
}

static void *uj_BC_CALL(HANDLER_SIGNATURE)
{
	TValue *fn = base + (ptrdiff_t)vm_ra(ins);

	/* NYI: set_vmstate INTERP */

	if (LJ_UNLIKELY(!tvisfunc(fn)))
		vm_assert(0); /* NYI: metacall */

	base = fn + 1;
	fn->fr.tp.pcr = pc;
	pc = ((GCfuncL *)fn->gcr)->pc;

	DISPATCH();
}

static void *uj_BC_HOTCNT(HANDLER_SIGNATURE)
{
	/* NYI: Payload */
	DISPATCH();
}

static void *uj_BC_FORI(HANDLER_SIGNATURE)
{
UJ_PEDANTIC_OFF

	static void *comparator[] = {
		&&positive_step,
		&&non_positive_step
	};
	TValue *idx;
	lua_Number i, stop;

	/* NYI: checktimeout */

	idx = base + (ptrdiff_t)vm_ra(ins);

	if (LJ_UNLIKELY(!tvisnum(idx)))
		vm_assert(0); /* NYI: vmeta_for */

	if (LJ_UNLIKELY(!tvisnum(idx + 1)))
		vm_assert(0); /* NYI: vmeta_for */

	if (LJ_UNLIKELY(!tvisnum(idx + 2)))
		vm_assert(0); /* NYI: vmeta_for */

	i = numV(idx);
	setnumV(idx + 3, i);
	stop = numV(idx + 1);

	goto *comparator[rawV(idx + 2) >> 63];

positive_step:
	if (stop < i)
		pc += vm_raw_rd(ins) - BCBIAS_J;
	DISPATCH();

non_positive_step:
	if (stop >= i)
		pc += vm_raw_rd(ins) - BCBIAS_J;
	DISPATCH();

UJ_PEDANTIC_OFF
}

static void *uj_BC_IFUNCF(HANDLER_SIGNATURE)
{
	TValue *top = base + vm_ra(ins);

	/* Setup kbase: */
	vmf->kbase = pc2proto(((base - 1)->fr.func)->fn.l.pc)->k;

	/* NYI: set_vmstate_lfunc */

	if (top > vmf->L->maxstack)
		; /* NYI: grow stack */

	/* NYI: checktimeout */

	/* Clear missing parameters */
	for (ptrdiff_t i = vmf->nargs; i < pc2proto(pc - 1)->numparams; i++)
		setnilV(base + i);

	DISPATCH();
}

/* RET0 rbase lit */
static void *uj_BC_RET0(HANDLER_SIGNATURE)
{
	/* NYI: set_vmstate INTERP */

	pc = (base - 1)->fr.tp.pcr; /* Restore caller's pc */

	if ((((uintptr_t)pc) & FRAME_TYPE)) {
		/* Not returning to a fixarg Lua func? */
		if ((((uintptr_t)pc - FRAME_VARG) & FRAME_TYPEP))
			return vm_return(HANDLER_ARGUMENTS);
		else
			vm_assert(0); /* NYI: Return from vararg function: relocate BASE down and RA up. */
	}

	/* Clear missing return values. */
	for (ptrdiff_t i = -1; i <= (ptrdiff_t)vm_raw_rb(*(pc - 1)) - 2; i++)
		setnilV(base + i);

	/* Restore base: */
	base -= ((ptrdiff_t)vm_ra(*(pc - 1)) + 1);

	/* Setup kbase: */
	vmf->kbase = pc2proto(((base - 1)->fr.func)->fn.l.pc)->k;

	/* NYI: set_vmstate_lfunc */
	/* NYI: checktimeout */

	DISPATCH();
}
