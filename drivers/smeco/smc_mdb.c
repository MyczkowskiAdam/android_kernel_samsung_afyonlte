/*
* Copyright (c) 2013, Renesas Mobile Corporation.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#if 0
/*
Change history:

Version:       10   15-Mar-2012     Heikki Siikaluoma
Status:        draft
Description :  Code cleanup, Linux Kernel compile warnings removed.

Version:       7    11-Feb-2012     Heikki Siikaluoma
Status:        draft
Description :  Removed MDB list, smc_channel pointer in use.

Version:       6    03-Jan-2012     Janne Mahosenaho
Status:        draft
Description :  Added TLSF statistics.
-------------------------------------------------------------------------------
Version:       5    28-Dec-2011     Janne Mahosenaho
Status:        draft
Description :  Fixes for Linux build.
-------------------------------------------------------------------------------
Version:       4    27-Dec-2011     Janne Mahosenaho
Status:        draft
Description :  Alternative TLSF implementation added.
-------------------------------------------------------------------------------
Version:       1    01-Dec-2011     Jussi Pellinen
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#include "smc_common_includes.h"
#include "smc.h"
#include "smc_trace.h"
#include "smc_mdb.h"


    /* The  debug functions  only can  be used  when _DEBUG_TLSF_  is set. */
#ifndef _DEBUG_TLSF_
#define _DEBUG_TLSF_  (0)
#endif

#if !defined(__GNUC__)
#ifndef __inline__
#define __inline__ inline
#endif
#endif


#define SMC_MDB_ALLOC_FROM_POOL( pool, size )  tlsf_malloc(pool, size)
#define SMC_MDB_FREE_FROM_POOL(pool, ptr)      tlsf_free(pool, ptr)





    /* Create/destroy a memory pool. */
size_t tlsf_create(size_t bytes, void *mem);
void   tlsf_destroy(void *pool);

static __inline__ void* tlsf_malloc(void *pool, size_t bytes);
static __inline__ void  tlsf_free(void *pool, void *ptr);

#define FORCE_FLS_GENERIC 0
#define TLSF_STATISTICS   0

#if defined (__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)) \
	&& defined (__GNUC_PATCHLEVEL__) && !FORCE_FLS_GENERIC

static __inline__ int tlsf_ffs(unsigned int word)
{
	return __builtin_ffs(word) - 1;
}

static __inline__ int tlsf_fls(unsigned int word)
{
	const int bit = word ? 32 - __builtin_clz(word) : 0;
	return bit - 1;
}

#elif defined (__ARMCC_VERSION) && !FORCE_FLS_GENERIC

static __inline__ int tlsf_ffs(unsigned int word)
{
	const unsigned int reverse = word & (~word + 1);
	const int bit = 32 - __clz(reverse);
	return bit - 1;
}

static __inline__ int tlsf_fls(unsigned int word)
{
	const int bit = word ? 32 - __clz(word) : 0;
	return bit - 1;
}

#else
/* Fall back to generic implementation. */
static __inline__ int tlsf_fls_generic(unsigned int word)
{
	int bit = 32;

	if (!word) bit -= 1;
	if (!(word & 0xffff0000)) { word <<= 16; bit -= 16; }
	if (!(word & 0xff000000)) { word <<= 8; bit -= 8; }
	if (!(word & 0xf0000000)) { word <<= 4; bit -= 4; }
	if (!(word & 0xc0000000)) { word <<= 2; bit -= 2; }
	if (!(word & 0x80000000)) { word <<= 1; bit -= 1; }

	return bit;
}/* Implement ffs in terms of fls. */

#define tlsf_ffs(word) \
	(tlsf_fls_generic(word & (~word + 1)) - 1)

#define tlsf_fls(word) \
	(tlsf_fls_generic(word) - 1)

#endif

/**
 * Constants.
 */

/* Public constants: may be modified. */
enum tlsf_public
{
	/* log2 of number of linear subdivisions of block sizes. */
	SL_INDEX_COUNT_LOG2 = 5,
};

/* Private constants: do not modify. */
enum tlsf_private
{
	/* All allocation sizes and addresses are aligned to 32 bytes. */
	ALIGN_SIZE_LOG2 = 5,

	ALIGN_SIZE = (1 << ALIGN_SIZE_LOG2),

	/*
	** We support allocations of sizes up to (1 << FL_INDEX_MAX) bits.
	** However, because we linearly subdivide the second-level lists, and
	** our minimum size granularity is 32 bytes, it doesn't make sense to
	** create first-level lists for sizes smaller than SL_INDEX_COUNT * 32,
	** or (1 << (SL_INDEX_COUNT_LOG2 + 2)) bytes, as there we will be
	** trying to split size ranges into more slots than we have available.
	** Instead, we calculate the minimum threshold size, and place all
	** blocks below that size into the 0th first-level list.
	*/

	FL_INDEX_MAX = 30,
	SL_INDEX_COUNT = (1 << SL_INDEX_COUNT_LOG2),
	FL_INDEX_SHIFT = (SL_INDEX_COUNT_LOG2 + ALIGN_SIZE_LOG2),
	FL_INDEX_COUNT = (FL_INDEX_MAX - FL_INDEX_SHIFT + 1),

	SMALL_BLOCK_SIZE = (1 << FL_INDEX_SHIFT),
};

#define TLSF_SIGNATURE  (0x1234AAAA)

/*
** Cast and min/max macros.
*/

#define TLSF_CAST(t, exp)	((t) (exp))
#define TLSF_MIN(a, b)		((a) < (b) ? (a) : (b))
#define TLSF_MAX(a, b)		((a) > (b) ? (a) : (b))

/*
** Set assert macro, if it has not been provided by the user.
*/
#if !defined (tlsf_assert)
#define tlsf_assert assert
#endif

/*
** Static assertion mechanism.
*/

#define _tlsf_glue2(x, y) x ## y
#define _tlsf_glue(x, y) _tlsf_glue2(x, y)
#define tlsf_static_assert(exp) \
	typedef char _tlsf_glue(static_assert, __LINE__) [(exp) ? 1 : -1]

/* This code has been tested on 32- and 64-bit (LP/LLP) architectures. */
tlsf_static_assert(sizeof(int) * CHAR_BIT == 32);
tlsf_static_assert(sizeof(size_t) * CHAR_BIT >= 32);
tlsf_static_assert(sizeof(size_t) * CHAR_BIT <= 64);

/* SL_INDEX_COUNT must be <= number of bits in sl_bitmap's storage type. */
tlsf_static_assert(sizeof(unsigned int) * CHAR_BIT >= SL_INDEX_COUNT);

/* Ensure we've properly tuned our sizes. */
tlsf_static_assert(ALIGN_SIZE == SMALL_BLOCK_SIZE / SL_INDEX_COUNT);

/*
** Data structures and associated constants.
*/

/*
** Block header structure.
**
** There are several implementation subtleties involved:
** - The prev_phys_block field is only valid if the previous block is free.
** - The next_free / prev_free fields are only valid if the block is free.
** - User data (external pointer) starts after padding
*/
typedef struct block_header_t
{
	/* Points to the previous physical block. */
	struct block_header_t* prev_phys_block;

    uint8_t is_free; 
    uint8_t is_prev_free; 

    /* Internal block size including header. */
	size_t size;

    /* Padding to alignment */
    uint8_t padding[22];
	
	/* Next and previous free blocks. User data starts here if the block is in use.*/
	struct block_header_t* next_free;
	struct block_header_t* prev_free;
    
} block_header_t;

static const size_t block_header_overhead = ALIGN_SIZE;

static const size_t block_size_min = 
	(sizeof(block_header_t) + (ALIGN_SIZE - 1)) & ~(ALIGN_SIZE - 1);

static const size_t block_size_max = TLSF_CAST(size_t, 1) << FL_INDEX_MAX;


/* The TLSF pool structure. */
typedef struct pool_t
{
	/* Empty lists point at this block to indicate they are free. */
	block_header_t block_null;
    uint32_t tlsf_free_marker;
    
	/* Bitmaps for free lists. */
	unsigned int fl_bitmap;
	unsigned int sl_bitmap[FL_INDEX_COUNT];

	/* Head of free lists. */
	block_header_t* blocks[FL_INDEX_COUNT][SL_INDEX_COUNT];

    /* 28 bytes to next 32 byte border. Use to store debug data. */
    uint32_t free_blocks;
    uint32_t used_blocks;
    uint32_t free_space;
    uint32_t free_space_min;
} pool_t;

/* A type used for casting when doing pointer arithmetic. */
typedef uint32_t tlsfptr_t;

/**
 * block_header_t member macros.
 */

#define block_size(block) \
	(block->size)

#define block_set_size(block, new_size) \
	(block->size = new_size)

#define block_is_last(block) \
	(0 == block->size ? 1 : 0)

#define block_is_free(block) \
	(block->is_free)

#define block_set_free(block) \
	(block->is_free = 1)

#define block_set_used(block) \
	(block->is_free = 0)

#define block_is_prev_free(block) \
	(block->is_prev_free)

#define block_set_prev_free(block) \
	(block->is_prev_free = 1)

#define block_set_prev_used(block) \
	(block->is_prev_free = 0)

#define BLOCK_FROM_PTR(ptr) \
	(TLSF_CAST(block_header_t*, \
		TLSF_CAST(unsigned char*, ptr) - block_header_overhead))

#define block_to_ptr(block) \
	(TLSF_CAST(void*, \
		TLSF_CAST(unsigned char*, block) + block_header_overhead))

/* Return location of next block after block of given size. */
#define offset_to_block(block, size) \
	(TLSF_CAST(block_header_t*, TLSF_CAST(size_t, block) + size))

/* Return location of previous block. */
#define block_prev(block) \
	(block->prev_phys_block)

/* Return location of next existing block. */
#define block_next(block) \
	(offset_to_block(block, block_size(block)))

/* Link a new block with its physical neighbor, return the neighbor. */
static __inline__ block_header_t* block_link_next(block_header_t* block)
{
	block_header_t* next = block_next(block);
	next->prev_phys_block = block;
	return next;
}

static __inline__ void block_mark_as_free(block_header_t* block)
{
	/* Link the block to the next block, first. */
    block_header_t* next = block_link_next(block);
    block_set_prev_free(next);
    block_set_free(block);
}

static __inline__ void block_mark_as_used(block_header_t* block)
{
	block_header_t* next = block_next(block);
	block_set_prev_used(next);
	block_set_used(block);
}

#define align_up(x, align) \
	((x + (align - 1)) & ~(align - 1))

#define align_down(x, align) \
	(x - (x & (align - 1)))

#define ALIGN_PTR(ptr, align) \
    (TLSF_CAST(void*, (TLSF_CAST(tlsfptr_t, ptr) + (align - 1)) & ~(align - 1)))

/*
** Adjust an allocation size to be aligned to word size, and no smaller
** than internal minimum.
*/
#define adjust_request_size(size, align) \
    (TLSF_MAX(align_up(size + block_header_overhead, align), block_size_min))

/*
** TLSF utility functions. In most cases, these are direct translations of
** the documentation found in the white paper.
*/

static __inline__ void mapping_insert(size_t size, int* fli, int* sli)
{
	if (size < SMALL_BLOCK_SIZE)
	{
		/* Store small blocks in first list. */
		*fli = 0;
		*sli = size / (SMALL_BLOCK_SIZE / SL_INDEX_COUNT);
	}
	else
	{
		*fli = tlsf_fls(size);
		*sli = size >> (*fli - SL_INDEX_COUNT_LOG2) ^ (1 << SL_INDEX_COUNT_LOG2);
		*fli -= (FL_INDEX_SHIFT - 1);
	}
}

/* This version rounds up to the next block size (for allocations) */
static __inline__ void mapping_search(size_t size, int* fli, int* sli)
{
	if (size >= (1 << SL_INDEX_COUNT_LOG2))
	{
		size += (1 << (tlsf_fls(size) - SL_INDEX_COUNT_LOG2)) - 1;
	}
	mapping_insert(size, fli, sli);
}

static __inline__ block_header_t* search_suitable_block(pool_t* pool, int* fli, int* sli)
{
	/*
	** First, search for a block in the list associated with the given
	** fl/sl index.
	*/
	unsigned int sl_map = pool->sl_bitmap[*fli] & (~0 << *sli);
	if (!sl_map)
	{
		/* No block exists. Search in the next largest first-level list. */
		const unsigned int fl_map = pool->fl_bitmap & (~0 << (*fli + 1));

		if (!fl_map)
		{
			    /* No free blocks available, memory has been exhausted. */
			return NULL;
		}

		*fli = tlsf_ffs(fl_map);
		sl_map = pool->sl_bitmap[*fli];
	}
	tlsf_assert(sl_map); /* internal error - second level bitmap is null */
	*sli = tlsf_ffs(sl_map);

	    /* Return the first block in the free list. */
	return pool->blocks[*fli][*sli];
}

/**
 * Removes a free block from the free list.
 */
static __inline__ void remove_free_block(pool_t* pool, block_header_t* block, int fl, int sl)
{
    block_header_t* prev = NULL;
    block_header_t* next = NULL;

    if( pool == NULL )
    {
        SMC_TRACE_PRINTF_ASSERT("MDB: remove_free_block: Target pool for block 0x%08X is NULL (fl: %d, sl: %d)", (uint32_t)block, fl, sl);
        tlsf_assert(0);
    }

    if( block == NULL )
    {
        SMC_TRACE_PRINTF_ASSERT("MDB: remove_free_block: Block to be freed is NULL (fl: %d, sl: %d)", fl, sl);
        tlsf_assert(0);
    }

	prev = block->prev_free;
	next = block->next_free;

	if( prev == NULL )
	{
	    SMC_TRACE_PRINTF_ASSERT("MDB: remove_free_block: Block 0x%08X->prev_free is NULL (fl: %d, sl: %d)", (uint32_t)block, fl, sl);
	    tlsf_assert(prev);  /* prev_free field can not be null */
	}

	tlsf_assert(next);  /* next_free field can not be null */

	next->prev_free = prev;
	prev->next_free = next;

	    /* If this block is the head of the free list, set new head. */
	if (pool->blocks[fl][sl] == block)
	{
		pool->blocks[fl][sl] = next;

		    /* If the new head is null, clear the bitmap. */
		if (next == &pool->block_null)
		{
			pool->sl_bitmap[fl] &= ~(1 << sl);

			/* If the second bitmap is now empty, clear the fl bitmap. */
			if (!pool->sl_bitmap[fl])
			{
				pool->fl_bitmap &= ~(1 << fl);
			}
		}
	}
#if TLSF_STATISTICS
    pool->free_blocks--;
#endif
}

/* Insert a free block into the free block list. */
static __inline__ void insert_free_block(pool_t* pool, block_header_t* block, int fl, int sl)
{
	block_header_t* current_block = pool->blocks[fl][sl];
	tlsf_assert(current_block); //free list cannot have a null entry
	tlsf_assert(block); //cannot insert a null entry into the free list
	block->next_free = current_block;
	block->prev_free = &pool->block_null;
	current_block->prev_free = block;

	tlsf_assert(block_to_ptr(block) == ALIGN_PTR(block_to_ptr(block), ALIGN_SIZE)); //block not aligned properly
	/*
	** Insert the new block at the head of the list, and mark the first-
	** and second-level bitmaps appropriately.
	*/
	pool->blocks[fl][sl] = block;
	pool->fl_bitmap |= (1 << fl);
	pool->sl_bitmap[fl] |= (1 << sl);
#if TLSF_STATISTICS
    pool->free_blocks++;
#endif
}

/* Remove a given block from the free list. */
static __inline__ void block_remove(pool_t* pool, block_header_t* block)
{
	int fl, sl;
	mapping_insert(block_size(block), &fl, &sl);
	remove_free_block(pool, block, fl, sl);
}

/* Insert a given block into the free list. */
static __inline__ void block_insert(pool_t* pool, block_header_t* block)
{
	int fl, sl;
	mapping_insert(block_size(block), &fl, &sl);
	insert_free_block(pool, block, fl, sl);
}

#define BLOCK_CAN_SPLIT(block, size) \
	(block_size(block) >= block_size_min + size)

/* Split a block into two, the second of which is free. */
static __inline__ block_header_t* block_split(block_header_t* block, size_t size)
{
	/* Calculate the amount of space left in the remaining block. */
	block_header_t* remaining =	offset_to_block(block, size);

	const size_t remain_size = block_size(block) - size;

	tlsf_assert(block_to_ptr(remaining) == ALIGN_PTR(block_to_ptr(remaining), ALIGN_SIZE)); /* remaining block not aligned properly */

	tlsf_assert(block_size(block) == remain_size + size);
	block_set_size(remaining, remain_size);
	tlsf_assert(block_size(remaining) >= block_size_min);  /* block split with invalid size */

	block_set_size(block, size);
	block_mark_as_free(remaining);

	return remaining;
}

/* Absorb a free block's storage into an adjacent previous free block. */
static __inline__ block_header_t* block_absorb(block_header_t* prev, block_header_t* block)
{
	tlsf_assert(!block_is_last(prev)); //previous block can't be last
	prev->size += block_size(block);
	block_link_next(prev);
	return prev;
}

/* Merge a just-freed block with an adjacent previous free block. */
static __inline__ block_header_t* block_merge_prev(pool_t* pool, block_header_t* block)
{
	if (block_is_prev_free(block))
	{
		block_header_t* prev = block_prev(block);
		tlsf_assert(prev); //prev physical block can't be null
		tlsf_assert(block_is_free(prev)); //prev block is not free though marked as such
		block_remove(pool, prev);
		block = block_absorb(prev, block);
	}

	return block;
}

/* Merge a just-freed block with an adjacent free block. */
static __inline__ block_header_t* block_merge_next(pool_t* pool, block_header_t* block)
{
	block_header_t* next = block_next(block);
	tlsf_assert(next); /* next physical block can't be null */

	if (block_is_free(next))
	{
		tlsf_assert(!block_is_last(block)); /* previous block can't be last */
		block_remove(pool, next);
		block = block_absorb(block, next);
	}

	return block;
}

/* Trim any trailing block space off the end of a block, return to pool. */
static __inline__ void block_trim_free(pool_t* pool, block_header_t* block, size_t size)
{
	tlsf_assert(block_is_free(block)); //block must be free
	if (BLOCK_CAN_SPLIT(block, size))
	{
		block_header_t* remaining_block = block_split(block, size);
		block_link_next(block);
		block_set_prev_free(remaining_block);
		block_insert(pool, remaining_block);
	}
}

static __inline__ block_header_t* block_locate_free(pool_t* pool, size_t size)
{
	int fl = 0, sl = 0;
	block_header_t* block = 0;

	if(size)
	{
		mapping_search(size, &fl, &sl);
		block = search_suitable_block(pool, &fl, &sl);
	}

	if(block != NULL)
	{
	    if( block_size(block) < size )
        {
            SMC_TRACE_PRINTF_ASSERT("MDB: block_locate_free: Actual suitable block size: %d < size %d requested", block_size(block), size);
            tlsf_assert(block_size(block) >= size);
        }

		remove_free_block(pool, block, fl, sl);
	}
	/*
	else
	{
	    SMC_TRACE_PRINTF_WARNING("block_locate_free: pool 0x%08X has no free blocks for %d bytes", (uint32_t)pool, size );
	}
	*/

	return block;
}

static __inline__ void* block_prepare_used(pool_t* pool, block_header_t* block, size_t size)
{
	void* p = 0;

	if(block)
	{
		block_trim_free(pool, block, size);
		block_mark_as_used(block);

#if TLSF_STATISTICS
        pool->free_space -= block_size(block);
        if (pool->free_space < pool->free_space_min)
            pool->free_space_min = pool->free_space;
        SMC_TRACE_PRINTF_MDB("ALLOCATED %d bytes, free_space %d, free_space_min %d", block_size(block), pool->free_space, pool->free_space_min);
#endif
		p = block_to_ptr(block);
	}

	return p;
}

/* Clear structure and point all empty lists at the null block. */
static __inline__ void pool_construct(pool_t* pool)
{
	int i, j;

	pool->block_null.next_free = &pool->block_null;
	pool->block_null.prev_free = &pool->block_null;

    pool->tlsf_free_marker = TLSF_SIGNATURE;

	pool->fl_bitmap = 0;
	for (i = 0; i < FL_INDEX_COUNT; ++i)
	{
		pool->sl_bitmap[i] = 0;
		for (j = 0; j < SL_INDEX_COUNT; ++j)
		{
			pool->blocks[i][j] = &pool->block_null;
		}
	}
    pool->free_blocks = 0;
    pool->used_blocks = 0;
}

/*
** Debugging utilities.
*/

typedef struct integrity_t
{
	int prev_status;
	int status;
} integrity_t;

#define tlsf_insist(x) { tlsf_assert(x); if (!(x)) { status--; } }

static void integrity_walker(void* ptr, size_t size, int used, void* user)
{
	block_header_t* block = BLOCK_FROM_PTR(ptr);
	integrity_t* integ = TLSF_CAST(integrity_t*, user);
	const int this_prev_status = block_is_prev_free(block) ? 1 : 0;
	const int this_status = block_is_free(block) ? 1 : 0;
	const size_t this_block_size = block_size(block);

	int status = 0;
	tlsf_insist(integ->prev_status == this_prev_status); /* prev status incorrect */
	tlsf_insist(size == this_block_size);                /* block size incorrect  */

	integ->prev_status = this_status;
	integ->status += status;
}

int tlsf_check_heap(tlsf_pool tlsf)
{
	int i, j;
	pool_t* pool = TLSF_CAST(pool_t*, tlsf);
	int status = 0;

	/* Check that the blocks are physically correct. */
	integrity_t integ = { 0, 0 };
	tlsf_walk_heap(tlsf, integrity_walker, &integ);
	status = integ.status;

	/* Check that the free lists and bitmaps are accurate. */
	for (i = 0; i < FL_INDEX_COUNT; ++i)
	{
		for (j = 0; j < SL_INDEX_COUNT; ++j)
		{
			const int             fl_map  = pool->fl_bitmap & (1 << i);
			const int             sl_list = pool->sl_bitmap[i];
			const int             sl_map  = sl_list & (1 << j);
			const block_header_t* block   = pool->blocks[i][j];

			/* Check that first- and second-level lists agree. */
			if (!fl_map)
			{
				tlsf_insist(!sl_map);                   /* second-level map must be null */
			}

			if (!sl_map)
			{
				tlsf_insist(block == &pool->block_null); /* block list must be null */
				continue;
			}

			/* Check that there is at least one free block. */
			tlsf_insist(sl_list);                       /* no free blocks in second-level map */
			tlsf_insist(block != &pool->block_null);    /* block should not be null           */

			while (block != &pool->block_null)
			{
				int fli, sli;
				tlsf_insist(block_is_free(block));                  // block should be free
				tlsf_insist(!block_is_prev_free(block));            // blocks should have coalesced
				tlsf_insist(!block_is_free(block_next(block)));     // blocks should have coalesced
				tlsf_insist(block_is_prev_free(block_next(block))); // block should be free
				tlsf_insist(block_size(block) >= block_size_min);   // block not minimum size

				mapping_insert(block_size(block), &fli, &sli);
				tlsf_insist(fli == i && sli == j);                  // block size indexed in wrong list
				block = block->next_free;
			}
		}
	}

	return status;
}

#undef tlsf_insist

static void default_walker(void* ptr, size_t size, int used, void* user)
{
	(void)user;
	SMC_TRACE_PRINTF_MDB("0x%08X %s size: 0x%08X (0x%08X)", (uint32_t)ptr, used ? "used" : "free", (uint32_t)size, (uint32_t)BLOCK_FROM_PTR(ptr));
}

void tlsf_walk_heap(tlsf_pool pool, tlsf_walker walker, void* user)
{
	tlsf_walker     heap_walker = walker ? walker : default_walker;
	block_header_t* block       = offset_to_block(pool, sizeof(pool_t));

	while (block && !block_is_last(block))
	{
		heap_walker(
			block_to_ptr(block),
			block_size(block),
			!block_is_free(block),
			user);
		block = block_next(block);
	}
}

size_t tlsf_block_size(void* ptr)
{
	size_t size = 0;

	if (ptr)
	{
		const block_header_t* block = BLOCK_FROM_PTR(ptr);
		size = block_size(block);
	}

	return size;
}

size_t tlsf_ptr_size(void * ptr)
{
    size_t size = 0;
    size = tlsf_block_size(ptr);
    if (size)
    {
        size -= block_header_overhead;
    }
    return size;
}

/*
** Overhead of the TLSF structures in a given memory block passed to
** tlsf_create, equal to the size of a pool_t aligned up to next 32 byte boundary.
*/
size_t tlsf_overhead()
{
	const size_t pool_overhead = align_up(sizeof(pool_t), ALIGN_SIZE);
	return pool_overhead;
}

/*
** TLSF main interface. Right out of the white paper.
*/

size_t tlsf_create(size_t bytes, void *mem)
{
	block_header_t* block = NULL;
	block_header_t* next  = NULL;
	pool_t*         pool  = NULL;

#if _DEBUG_TLSF_
	int rv = 0;
#endif

	const size_t pool_overhead = tlsf_overhead();
    /* Subtract the size of pool header overhead and size of sentinel to get
    ** the actual usable pool size */
	const size_t pool_bytes = align_down((bytes - pool_overhead - block_size_min), ALIGN_SIZE);

    SMC_TRACE_PRINTF_MDB("bytes %d, pool_bytes %d, pool_overhead %d, block_size_min %d", bytes, pool_bytes, pool_overhead, block_size_min);

	pool = TLSF_CAST(pool_t*, mem);

#if _DEBUG_TLSF_
	/* Verify ffs/fls work properly. */
	rv += (tlsf_ffs(0) == -1) ? 0 : 0x1;
	rv += (tlsf_fls(0) == -1) ? 0 : 0x2;
	rv += (tlsf_ffs(1) == 0) ? 0 : 0x4;
	rv += (tlsf_fls(1) == 0) ? 0 : 0x8;
	rv += (tlsf_ffs(0x80000000) == 31) ? 0 : 0x10;
	rv += (tlsf_ffs(0x80008000) == 15) ? 0 : 0x20;
	rv += (tlsf_fls(0x80000008) == 31) ? 0 : 0x40;
	rv += (tlsf_fls(0x7FFFFFFF) == 30) ? 0 : 0x80;
#endif

	if (pool_bytes < block_size_min || pool_bytes > block_size_max)
	{
		SMC_TRACE_PRINTF_MDB("tlsf_create: Pool size must be between %u and %u bytes.\n",
			(unsigned int)(block_size_min),
			(unsigned int)(block_size_max));
		return 0;
	}

	/* Construct a valid pool object. */
	pool_construct(pool);

	/* Create the main free block. */
    block = offset_to_block(TLSF_CAST(void*, pool), pool_overhead);
	block_set_size(block, pool_bytes);
	block_set_free(block);
	block_set_prev_used(block);
	block_insert(pool, block);

#if TLSF_STATISTICS
    pool->free_space = block_size(block) - block_header_overhead;
    pool->free_space_min = pool->free_space;
#else
    pool->free_space = 0;
#endif

	/* Split the block to create a zero-size pool sentinel block. */
	next = block_link_next(block);
	block_set_size(next, 0);
	block_set_used(next);
	block_set_prev_free(next);

	return pool_bytes;
}

void tlsf_destroy(void *pool)
{
    pool_t *tlsf = (pool_t*)pool;
	tlsf->tlsf_free_marker = 0;
}

static __inline__ void* tlsf_malloc(void *tlsf, size_t size)
{
	pool_t* pool = TLSF_CAST(pool_t*, tlsf);
	const size_t adjust = adjust_request_size(size, ALIGN_SIZE);
	block_header_t* block = block_locate_free(pool, adjust);
	return block_prepare_used(pool, block, adjust);
}

static __inline__ void tlsf_free(void *tlsf, void *ptr)
{
	if(ptr)
	{
		pool_t* pool = TLSF_CAST(pool_t*, tlsf);
		block_header_t* block = BLOCK_FROM_PTR(ptr);
#if TLSF_STATISTICS
        pool->free_space += block_size(block);
#endif
		block_mark_as_free(block);
		block = block_merge_prev(pool, block);
		block = block_merge_next(pool, block);
		block_insert(pool, block);
	}
}

/*******************
 ** MDB functions **
 *******************/


/*
 * Calculates the size of required share memory MDB with given MDB data area size.
 */
uint32_t smc_mdb_calculate_required_shared_mem( uint32_t mdb_data_area_size )
{
    uint32_t required_mem = 0;

        /* No additional shared memory needed except the data area */
    required_mem = mdb_data_area_size;

    SMC_TRACE_PRINTF_MDB("smc_mdb_calculate_required_shared_mem: MDB requires %d bytes of shared memory", required_mem);

    return required_mem;
}


smc_mdb_channel_info_t* smc_mdb_channel_info_create( void )
{
    smc_mdb_channel_info_t* channel_info = NULL;

    channel_info = (smc_mdb_channel_info_t*)SMC_MALLOC(sizeof(smc_mdb_channel_info_t));

    assert( channel_info != NULL );

    channel_info->pool_out       = NULL;
    channel_info->total_size_out = 0;
    channel_info->pool_in        = NULL;
    channel_info->total_size_in  = 0;

    SMC_TRACE_PRINTF_MDB("smc_mdb_channel_info_create: MDB channel info 0x%08X created", (uint32_t)channel_info);

    return channel_info;
}


/**
 * Create Pool out
 */
uint8_t smc_mdb_create_pool_out( void* pool_address, uint32_t pool_size )
{
    uint8_t ret_val   = SMC_OK;
    int32_t init_size = 0;
    int32_t i         = 0;

    assert(sizeof(uint32_t) == 4);
    assert(sizeof(uint8_t)  == 1);

    assert(pool_address != NULL);
    assert(pool_size != 0);

    SMC_TRACE_PRINTF_MDB("smc_mdb_create_pool_out: pool_address: 0x%08X, pool_size: %d", (uint32_t)pool_address, pool_size);

    init_size = tlsf_create(pool_size, pool_address);

    if (init_size <= 0)
    {
        SMC_TRACE_PRINTF_ASSERT("smc_mdb_create_pool_out: MDB OUT not created, invalid size %d", init_size);
        assert(init_size <= 0);
    }
    else
    {
        SMC_TRACE_PRINTF_MDB("smc_mdb_create_pool_out: MDB OUT successfully created, init size was %d", init_size);

        for (i = pool_size; i > 0; i--)
        {
            void* ptr = SMC_MDB_ALLOC_FROM_POOL(pool_address, i);

            if (ptr != NULL)
            {
                SMC_MDB_FREE_FROM_POOL(pool_address, ptr);
                break;
            }
        }
    }
#if TLSF_STATISTICS
    ((pool_t*)pool_address)->free_space_min = ((pool_t*)pool_address)->free_space;
#endif

    return ret_val;
}

void smc_mdb_info_destroy( smc_mdb_channel_info_t* smc_mdb_info )
{
    if( smc_mdb_info )
    {
        SMC_TRACE_PRINTF_MDB("smc_mdb_info_destroy: 0x%08X", (uint32_t)smc_mdb_info);

        tlsf_destroy(smc_mdb_info->pool_out);

        SMC_FREE( smc_mdb_info );
        smc_mdb_info = NULL;
    }
}

void smc_mdb_all_destroy( void )
{
    SMC_TRACE_PRINTF_MDB("smc_mdb_all_destroy: NOTHING TO DO !!");
}


/**
 * Allocates memory for specified channel
 */
void* smc_mdb_alloc( smc_channel_t* smc_channel, uint32_t length )
{
    void* ptr;
    void* pool_out = NULL;
 
    assert(smc_channel != NULL);
    assert(smc_channel->smc_mdb_info != NULL);
    
    pool_out = smc_channel->smc_mdb_info->pool_out;

    assert(length > 0);
    assert(pool_out != NULL);
    
    ptr = SMC_MDB_ALLOC_FROM_POOL( pool_out, length );
    
    if( ptr == NULL )
    {
#if 1
        /* Take out warning print because that might cause rcu_preempt stall... Testing that. */
        SMC_TRACE_PRINTF_DEBUG("smc_mdb_alloc: MDB OUT OF SHM MEMORY: channel id %d (0x%08X), tried to allocate %d bytes from out pool 0x%08X",
                    smc_channel->id, (uint32_t)smc_channel, length, (uint32_t)pool_out );
#else
        SMC_TRACE_PRINTF_WARNING("smc_mdb_alloc: MDB OUT OF SHM MEMORY: channel id %d (0x%08X), tried to allocate %d bytes from out pool 0x%08X",
                    smc_channel->id, (uint32_t)smc_channel, length, (uint32_t)pool_out );
#endif
    }
    else
    {
        SMC_TRACE_PRINTF_MDB_ALLOC("smc_mdb_alloc: channel: %d (0x%08X), allocated %d bytes to ptr 0x%08X, pool 0x%08X",
                    smc_channel->id, (uint32_t)smc_channel, length, (uint32_t)ptr, pool_out );
    }


    return ptr;
}

void smc_mdb_free( const smc_channel_t* smc_channel, void* ptr )
{
    void* pool_out = NULL;

    assert(ptr != NULL);
    assert(smc_channel != NULL);
    assert(smc_channel->smc_mdb_info != NULL);

    pool_out = smc_channel->smc_mdb_info->pool_out;

    SMC_TRACE_PRINTF_MDB_ALLOC("smc_mdb_free: channel: %d (0x%08X), ptr 0x%08X pool 0x%08X",
            smc_channel->id, (uint32_t)smc_channel, (uint32_t)ptr, (uint32_t)pool_out);

    assert(pool_out != NULL);

    SMC_MDB_FREE_FROM_POOL(pool_out, ptr);
}

void smc_mdb_copy( void* target, void* source, uint32_t length )
{
    SMC_TRACE_PRINTF_MDB("smc_mdb_copy(): target: 0x%08X, source: 0x%08X, length: %d", (uint32_t)target, (uint32_t)source, length);

    assert(target != NULL);
    assert(source != NULL);
    assert(length != 0);

    /* --- NOTE: The SMC.C module uses memcpy directly
     *           IF THIS FUNCTION IS CHANGED --> CHECK THE SMC.C
     */

    memcpy(target, source, length);
}

uint8_t smc_mdb_address_check( const smc_channel_t* smc_channel, void* ptr, uint8_t direction )
{
    uint8_t result    = SMC_OK;
    void*   begin_ptr = NULL;
    void*   end_ptr   = NULL;

    smc_mdb_channel_info_t* channel_info_ptr = smc_channel->smc_mdb_info;

    assert(channel_info_ptr != NULL);

    SMC_TRACE_PRINTF_MDB("smc_mdb_address_check: channel_id: ID %d (0x%08X), ptr: 0x%08X, direction: %d",
            smc_channel->id, (uint32_t)smc_channel, (uint32_t)ptr, direction);

    switch (direction)
    {
        case SMC_MDB_IN:
        {
            assert(channel_info_ptr->pool_in != NULL);
            begin_ptr = channel_info_ptr->pool_in;
            end_ptr = (uint8_t*)channel_info_ptr->pool_in + channel_info_ptr->total_size_in;
            break;
        }
        case SMC_MDB_OUT:
        {
            assert(channel_info_ptr->pool_out != NULL);
            begin_ptr = channel_info_ptr->pool_out;
            end_ptr = (uint8_t*)channel_info_ptr->pool_out + channel_info_ptr->total_size_out;
            break;
        }
        default:
        {
            assert((direction != SMC_MDB_IN) && (direction != SMC_MDB_OUT));
            break;
        }
    }

    if ((ptr >= begin_ptr) && (ptr < end_ptr))
    {
        SMC_TRACE_PRINTF_MDB("smc_mdb_address_check: ptr 0x%08X is in pool area 0x%08X - 0x%08X",
                        (uint32_t)ptr, (uint32_t)begin_ptr, (uint32_t)end_ptr);
        result = SMC_OK;
    }
    else
    {
        SMC_TRACE_PRINTF_MDB("smc_mdb_address_check: ptr 0x%08X is not in pool area 0x%08X - 0x%08X",
                (uint32_t)ptr, (uint32_t)begin_ptr, (uint32_t)end_ptr);
        result = SMC_ERROR;
    }

    return result;
}


uint32_t smc_mdb_channel_frag_get( smc_channel_t* smc_channel )
{ 
    pool_t *pool = NULL;

    assert( smc_channel != NULL );

    pool = smc_channel->smc_mdb_info->pool_out;

    assert( pool!=NULL );

    return pool->free_blocks;
}

uint32_t smc_mdb_channel_free_space_get( smc_channel_t* smc_channel )
{
    pool_t* pool = NULL;

    assert( smc_channel!=NULL );

    pool = smc_channel->smc_mdb_info->pool_out;

    assert( pool!=NULL );

    return  pool->free_space;
}

uint32_t smc_mdb_channel_free_space_min_get( smc_channel_t* smc_channel )
{
    pool_t* pool = NULL;

    assert( smc_channel!=NULL );

    pool = smc_channel->smc_mdb_info->pool_out;

    assert( pool!=NULL );

    return  pool->free_space_min;
}

uint32_t smc_mdb_channel_largest_free_block_get( smc_channel_t* smc_channel )
{
    pool_t* pool = NULL;
    assert( smc_channel!=NULL );

    pool = smc_channel->smc_mdb_info->pool_out;

    if( pool!=NULL )
    {
        uint32_t fli = tlsf_fls(pool->fl_bitmap);
        uint32_t sli = tlsf_fls(pool->sl_bitmap[fli]);
        uint32_t size = 1U << (fli + FL_INDEX_SHIFT - 1);

        size += sli << (fli + FL_INDEX_SHIFT - 1 - SL_INDEX_COUNT_LOG2);

        return size;
    }
    else
    {
        assert(pool!=NULL);
        return 0;
    }
}

