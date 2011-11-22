/*
 * Copyright 2011 Shinpei Kato
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __GDEV_NVIDIA_H__
#define __GDEV_NVIDIA_H__

#ifdef __KERNEL__
#include "gdev_drv.h"
#else
#include "gdev_lib.h"
#endif
#include "gdev_list.h"
#include "gdev_time.h"
#include "gdev_nvidia_def.h"

//#define GDEV_DMA_PCOPY

#define GDEV_SUBCH_COMPUTE 1
#define GDEV_SUBCH_M2MF 2
#define GDEV_SUBCH_PCOPY0 3
#define GDEV_SUBCH_PCOPY1 4

#define GDEV_FENCE_COUNT 4 /* the number of fence types. */
#define GDEV_FENCE_COMPUTE 0
#define GDEV_FENCE_M2MF 1
#define GDEV_FENCE_PCOPY0 2
#define GDEV_FENCE_PCOPY1 3
#ifdef GDEV_DMA_PCOPY
#define GDEV_FENCE_DMA GDEV_FENCE_PCOPY0
#else
#define GDEV_FENCE_DMA GDEV_FENCE_M2MF
#endif

#define GDEV_FENCE_LIMIT 0x80000000

/**
 * virutal address space available for user buffers.
 */
#define GDEV_VAS_USER_START 0x20000000
#define GDEV_VAS_USER_END (1ull << 40)
#define GDEV_VAS_SIZE GDEV_VAS_USER_END

/**
 * memory types.
 */
#define GDEV_MEM_DEVICE 0
#define GDEV_MEM_DMA 1

/**
 * virtual address space (VAS) object struct:
 *
 * NVIDIA GPUs support virtual memory (VM) with 40 bits addressing.
 * VAS hence ranges in [0:1<<40]. In particular, the pscnv bo function will
 * allocate [0x20000000:1<<40] to any buffers in so called global memory,
 * local memory, and constant memory. the rest of VAS is used for different 
 * purposes, e.g., for shared memory.
 * CUDA programs access these memory spaces as follows:
 * g[$reg] redirects to one of g[$reg], l[$reg-$lbase], and s[$reg-$sbase],
 * depending on how local memory and shared memory are set up.
 * in other words, g[$reg] may reference global memory, local memory, and
 * shared memory.
 * $lbase and $sbase are respectively local memory and shared memory base
 * addresses, which are configured when GPU kernels are launched.
 * l[0] and g[$lbase] reference the same address, so do s[0] and g[$sbase].
 * constant memory, c[], is another type of memory space that is often used
 * to store GPU kernels' parameters (arguments).
 * global memory, local memory, and constant memory are usually mapped on
 * device memory, a.k.a., video RAM (VRAM), though they could also be mapped
 * on host memory, a.k.a., system RAM (SysRAM), while shared memory is always
 * mapped on SRAM present in each MP.
 */
struct gdev_vas {
	void *pvas; /* driver private object. */
	gdev_device_t *gdev; /* vas is associated with a specific device. */
	struct gdev_list mem_list; /* list of device memory spaces. */
	struct gdev_list dma_mem_list; /* list of host dma memory spaces. */
};

/**
 * GPU context object struct:
 */
struct gdev_ctx {
	void *pctx; /* driver private object. */
	gdev_vas_t *vas; /* chan is associated with a specific vas object. */
	struct gdev_fifo {
		volatile uint32_t *regs; /* channel control registers. */
		void *ib_bo; /* driver private object. */
		uint32_t *ib_map;
		uint32_t ib_order;
		uint64_t ib_base;
		uint32_t ib_mask;
		uint32_t ib_put;
		uint32_t ib_get;
		void *pb_bo; /* driver private object. */
		uint32_t *pb_map;
		uint32_t pb_order;
		uint64_t pb_base;
		uint32_t pb_mask;
		uint32_t pb_size;
		uint32_t pb_pos;
		uint32_t pb_put;
		uint32_t pb_get;
	} fifo; /* command FIFO queue struct. */
	struct gdev_fence { /* fence objects (for compute and dma). */
		void *bo; /* driver private object. */
		uint32_t *map;
		uint64_t addr;
		uint32_t sequence[GDEV_FENCE_COUNT];
	} fence;
};

/**
 * device/host memory object struct:
 */
struct gdev_mem {
	void *bo; /* driver private object. */
	gdev_vas_t *vas; /* mem is associated with a specific vas object. */
	struct gdev_list list_entry; /* entry to the memory list. */
	uint64_t addr; /* virtual memory address. */
	void *map; /* memory-mapped buffer (for host only). */
};

/* private compute functions. */
struct gdev_compute {
	void (*launch)(gdev_ctx_t*, struct gdev_kernel*);
	void (*fence_write)(gdev_ctx_t*, int, uint32_t);
	void (*fence_read)(gdev_ctx_t*, int, uint32_t*);
	void (*memcpy)(gdev_ctx_t*, uint64_t, uint64_t, uint32_t);
	void (*membar)(gdev_ctx_t*);
	void (*init)(gdev_ctx_t*);
};

/**
 * utility macros
 */
#define GDEV_MEM_ADDR(mem) (mem)->addr
#define GDEV_MEM_BUF(mem) (mem)->map

/**
 * runtime/driver-dependent resource management functions.
 */
int gdev_compute_init(gdev_device_t*);
int gdev_query(gdev_device_t*, uint32_t, uint32_t*);
gdev_device_t *gdev_dev_open(int);
void gdev_dev_close(gdev_device_t*);
gdev_vas_t *gdev_vas_new(gdev_device_t*, uint64_t);
void gdev_vas_free(gdev_vas_t*);
gdev_ctx_t *gdev_ctx_new(gdev_device_t*, gdev_vas_t*);
void gdev_ctx_free(gdev_ctx_t*);
gdev_mem_t *gdev_malloc(gdev_vas_t*, uint64_t, int);
void gdev_free(gdev_mem_t*);

/**
 * architecture-dependent setup functions.
 */
void nvc0_compute_setup(gdev_device_t *gdev);

/**
 * runtime/driver/architecture-independent compute functions.
 */
uint32_t gdev_memcpy(gdev_ctx_t*, uint64_t, uint64_t, uint32_t);
uint32_t gdev_launch(gdev_ctx_t*, struct gdev_kernel*);
void gdev_mb(gdev_ctx_t*);
int gdev_poll(gdev_ctx_t*, int, uint32_t, gdev_time_t*);

/**
 * runtime/driver/architecture-independent heap operations.
 */
void gdev_heap_init(gdev_vas_t*);
void gdev_heap_add(gdev_mem_t*, int);
void gdev_heap_del(gdev_mem_t*);
gdev_mem_t *gdev_heap_lookup(gdev_vas_t*, uint64_t, int);
void gdev_garbage_collect(gdev_vas_t*);

/**
 * runtime/driver/architecture-independent inline FIFO functions.
 */
static inline void __gdev_relax_fifo(void)
{
	SCHED_YIELD();
}

static inline void __gdev_push_fifo
(gdev_ctx_t *ctx, uint64_t base, uint32_t len, int flags)
{
	uint64_t w = base | (uint64_t)len << 40 | (uint64_t)flags << 40;
	while (((ctx->fifo.ib_put + 1) & ctx->fifo.ib_mask) == ctx->fifo.ib_get) {
		uint32_t old = ctx->fifo.ib_get;
		ctx->fifo.ib_get = ctx->fifo.regs[0x88/4];
		if (old == ctx->fifo.ib_get) {
			__gdev_relax_fifo();
		}
	}
	ctx->fifo.ib_map[ctx->fifo.ib_put * 2] = w;
	ctx->fifo.ib_map[ctx->fifo.ib_put * 2 + 1] = w >> 32;
	ctx->fifo.ib_put++;
	ctx->fifo.ib_put &= ctx->fifo.ib_mask;
	MB(); /* is this needed? */
	ctx->fifo.ib_map[0] = ctx->fifo.ib_map[0]; /* flush writes? */
	ctx->fifo.regs[0x8c/4] = ctx->fifo.ib_put;
}

static inline void __gdev_update_get(gdev_ctx_t *ctx)
{
	uint32_t lo = ctx->fifo.regs[0x58/4];
	uint32_t hi = ctx->fifo.regs[0x5c/4];
	if (hi & 0x80000000) {
		uint64_t mg = ((uint64_t)hi << 32 | lo) & 0xffffffffffull;
		ctx->fifo.pb_get = mg - ctx->fifo.pb_base;
	} else {
		ctx->fifo.pb_get = 0;
	}
}

static inline void __gdev_fire_ring(gdev_ctx_t *ctx)
{
	if (ctx->fifo.pb_pos != ctx->fifo.pb_put) {
		if (ctx->fifo.pb_pos > ctx->fifo.pb_put) {
			uint64_t base = ctx->fifo.pb_base + ctx->fifo.pb_put;
			uint32_t len = ctx->fifo.pb_pos - ctx->fifo.pb_put;
			__gdev_push_fifo(ctx, base, len, 0);
		}
		else {
			uint64_t base = ctx->fifo.pb_base + ctx->fifo.pb_put;
			uint32_t len = ctx->fifo.pb_size - ctx->fifo.pb_put;
			__gdev_push_fifo(ctx, base, len, 0);
			/* why need this? */
			if (ctx->fifo.pb_pos) {
				__gdev_push_fifo(ctx, ctx->fifo.pb_base, ctx->fifo.pb_pos, 0);
			}
		}
		ctx->fifo.pb_put = ctx->fifo.pb_pos;
	}
}

static inline void __gdev_out_ring(gdev_ctx_t *ctx, uint32_t word)
{
	while (((ctx->fifo.pb_pos + 4) & ctx->fifo.pb_mask) == ctx->fifo.pb_get) {
		uint32_t old = ctx->fifo.pb_get;
		__gdev_fire_ring(ctx);
		__gdev_update_get(ctx);
		if (old == ctx->fifo.pb_get) {
			__gdev_relax_fifo();
		}
	}
	ctx->fifo.pb_map[ctx->fifo.pb_pos/4] = word;
	ctx->fifo.pb_pos += 4;
	ctx->fifo.pb_pos &= ctx->fifo.pb_mask;
}

static inline void __gdev_begin_ring_nv50
(gdev_ctx_t *ctx, int subc, int mthd, int len)
{
	__gdev_out_ring(ctx, mthd | (subc<<13) | (len<<18));
}

static inline void __gdev_begin_ring_nv50_const
(gdev_ctx_t *ctx, int subc, int mthd, int len)
{
	__gdev_out_ring(ctx, mthd | (subc<<13) | (len<<18) | (0x4<<28));
}

static inline void __gdev_begin_ring_nvc0
(gdev_ctx_t *ctx, int subc, int mthd, int len)
{
	__gdev_out_ring(ctx, (0x2<<28) | (len<<16) | (subc<<13) | (mthd>>2));
}

static inline void __gdev_begin_ring_nvc0_const
(gdev_ctx_t *ctx, int subc, int mthd, int len)
{
	__gdev_out_ring(ctx, (0x6<<28) | (len<<16) | (subc<<13) | (mthd>>2));
}

#endif
