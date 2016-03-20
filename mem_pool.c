/*
 * Created by Ivo Georgiev on 2/9/16.
 */

#include <stdlib.h>
#include <assert.h>
#include <stdio.h> // for perror()
#include "mem_pool.h"

/* Constants */
#define MEM_FILL_FACTOR 0.75;
#define MEM_EXPAND_FACTOR 2;
#define true 1;
#define false 0;

static const unsigned   MEM_POOL_STORE_INIT_CAPACITY    = 20;
static const float      MEM_POOL_STORE_FILL_FACTOR      = MEM_FILL_FACTOR;
static const unsigned   MEM_POOL_STORE_EXPAND_FACTOR    = MEM_EXPAND_FACTOR;

static const unsigned   MEM_NODE_HEAP_INIT_CAPACITY     = 40;
static const float      MEM_NODE_HEAP_FILL_FACTOR       = MEM_FILL_FACTOR;
static const unsigned   MEM_NODE_HEAP_EXPAND_FACTOR     = MEM_EXPAND_FACTOR;

static const unsigned   MEM_GAP_IX_INIT_CAPACITY        = 40;
static const float      MEM_GAP_IX_FILL_FACTOR          = MEM_FILL_FACTOR;
static const unsigned   MEM_GAP_IX_EXPAND_FACTOR        = MEM_EXPAND_FACTOR;

/* Type declarations */
typedef struct _node {
    alloc_t alloc_record;
    unsigned used;
    unsigned allocated;
    struct _node *next, *prev; // doubly-linked list for gap deletion
} node_t, *node_pt;

typedef struct _gap {
    size_t size;
    node_pt node;
} gap_t, *gap_pt;

typedef struct _pool_mgr {
    pool_t pool;
    node_pt node_heap;
    unsigned total_nodes;
    unsigned used_nodes;
    struct _gap *gap_ix;
    unsigned gap_ix_capacity;
} pool_mgr_t, *pool_mgr_pt;


/* Static global variables */
static pool_mgr_pt *pool_store = NULL; // an array of pointers, only expand
static unsigned pool_store_size = 0;
static unsigned pool_store_capacity = 0;


/* Forward declarations of static functions */
static alloc_status _mem_resize_pool_store();
static alloc_status _mem_resize_node_heap(pool_mgr_pt pool_mgr);
static alloc_status _mem_resize_gap_ix(pool_mgr_pt pool_mgr);
static alloc_status _mem_add_to_gap_ix(pool_mgr_pt pool_mgr, size_t size, node_pt node);
static alloc_status _mem_remove_from_gap_ix(pool_mgr_pt pool_mgr, size_t size, node_pt node);
static alloc_status _mem_sort_gap_ix(pool_mgr_pt pool_mgr);


/* Definitions of user-facing functions */
alloc_status mem_init()
{
    // TODO implement
    if(pool_store != NULL)
    {
        return ALLOC_CALLED_AGAIN;
    }
    if((pool_store = (pool_mgr_pt*)calloc(MEM_POOL_STORE_INIT_CAPACITY, sizeof(pool_mgr_pt)))== NULL)
    {
        return ALLOC_FAIL;
    }
    pool_store_capacity = MEM_POOL_STORE_INIT_CAPACITY;
    return ALLOC_OK;
}

alloc_status mem_free()
{
    // TODO implement
    if(pool_store == NULL)
    {
        return ALLOC_CALLED_AGAIN;
    }
    for(int i = 0; i < pool_store_capacity; ++i)
    {
        if(pool_store[i] != NULL)
        {
            return ALLOC_NOT_FREED;
        }
    }
    free(pool_store);
    pool_store_size = 0;
    pool_store_capacity = 0;
    pool_store = NULL;
    return ALLOC_OK;
}

pool_pt mem_pool_open(size_t size, alloc_policy policy)
{
    // TODO implement
    pool_mgr_pt temMr = NULL;
    node_pt temN = NULL;
    gap_pt temG = NULL;
    if(pool_store_size > 0)
    {
        ++pool_store;
    }
    if(pool_store_size >= (pool_store_capacity*MEM_POOL_STORE_FILL_FACTOR))
    {
        if(_mem_resize_pool_store() != ALLOC_OK)
        {
            return NULL;
        }
    }
    if((temMr = (pool_mgr_pt)malloc(sizeof(pool_mgr_t))) == NULL)
    {
        return NULL;
    }
    if((temMr->pool.mem = (char *)calloc(size, sizeof(char)))== NULL)
    {
        free(temMr);
        return NULL;
    }
    if((temN = (node_pt)calloc(MEM_NODE_HEAP_INIT_CAPACITY, sizeof(node_t))) == NULL)
    {
        free(temMr->pool.mem);
        free(temMr);
        return NULL;
    }
    temN->alloc_record.mem = temMr->pool.mem;
    temN->alloc_record.size = size;
    temN->allocated = 0;
    temN->used = 1;
    temMr->total_nodes = MEM_NODE_HEAP_INIT_CAPACITY;
    temMr->used_nodes =1;
    temMr->pool.policy = policy;
    temMr->pool.total_size = size;
    if((temG = (gap_pt)calloc(MEM_GAP_IX_INIT_CAPACITY, sizeof(gap_t))) == NULL)
    {
        free(temN);
        free(temMr->pool.mem);
        free(temMr);
        return NULL;
    }
    temG[0].size = size;
    temG[0].node = temN;
    temMr->gap_ix = temG;
    temMr->node_heap = temN;
    temMr->node_heap->next = NULL;
    temMr->node_heap->prev = NULL;
    temMr->pool.num_gaps = 1;
    temMr->gap_ix_capacity = MEM_GAP_IX_INIT_CAPACITY;
    *pool_store = temMr;
    ++pool_store_size;
    return (pool_pt)*pool_store;
}

alloc_status mem_pool_close(pool_pt pool)
{
    // TODO implement
    pool_mgr_pt boss = (pool_mgr_pt)pool;
    if(boss->pool.num_gaps > 1)
    {
        return ALLOC_NOT_FREED;
    }
    if(boss->pool.num_allocs > 0)
    {
        return ALLOC_NOT_FREED;
    }
    free(boss->pool.mem);
    free(boss->node_heap);
    free(boss->gap_ix);
    for(int i = 0; i < pool_store_size; ++i)
    {
        if(pool_store[i] == boss)
        {
            pool_store[i] = NULL;
            break;
        }
    }
    free(boss);
    boss = NULL;
    return ALLOC_OK;
}

alloc_pt mem_new_alloc(pool_pt pool, size_t size)
{
    // TODO implement
  // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt boss = (pool_mgr_pt)pool;
    int t = 0;
    for(t = 0; t < pool_store_size; ++t)
    {
        if(pool_store[t] == boss)
        {
            break;
        }
    }
    if(pool_store[t]->pool.num_gaps == 0)
    {
        return NULL;
    }
  // expand heap node, if necessary, quit on error
    if(pool_store[t]->used_nodes >= MEM_NODE_HEAP_FILL_FACTOR*pool_store[t]->total_nodes)
    {
        if(_mem_resize_node_heap(pool_store[t]) == ALLOC_FAIL)
        {
            perror("Couldn't resize heap\n");
        }
        if(pool_store[t]->used_nodes > pool_store[t]->total_nodes)
        {
            perror("Resize error\n");
        }
    }
  // get a node for allocation:
  // if FIRST_FIT, then find the first sufficient node in the node heap
  // if BEST_FIT, then find the first sufficient node in the gap index
    node_pt temN = pool_store[t]->node_heap;
    node_pt newNode = pool_store[t]->node_heap;
    if(pool_store[t]->pool.policy == FIRST_FIT)
    {
        int sniffer = false;
        while(!sniffer)
        {
            if(temN->allocated == 0 && temN->alloc_record.size >= size)
            {
                sniffer = true;
            }
            else
            {
                if(temN->next)
                {
                    temN = temN->next;
                }
                else
                {
                    return NULL;
                }
            }
        }
        if(size < temN->alloc_record.size)
        {
            while(newNode->used != 0 || newNode == temN)
            {
                newNode++;
            }
            newNode->used = 1;
            newNode->allocated = 0;
            newNode->alloc_record.size = temN->alloc_record.size - size;
            newNode->alloc_record.mem = temN->alloc_record.mem + size;
            temN->alloc_record.size = size;
            temN->allocated = 1;
            newNode->prev = temN;
            if(temN->next)
            {
               newNode->next = temN->next;
               temN->next->prev = newNode;
               temN->next = newNode;
            }
            else
            {
                temN->next = newNode;
            }
            if((_mem_remove_from_gap_ix(pool_store[t], size, temN)) == ALLOC_FAIL)
            {
                perror("Failed to remove from gap");
                exit(99);
            }
            if((_mem_add_to_gap_ix(pool_store[t], newNode->alloc_record.size, newNode)) == ALLOC_FAIL)
            {
                perror("FAILED TO ADD TO GAP");
                exit(99);
            }
        }
        else
        {
            temN->allocated = 1;
            if((_mem_remove_from_gap_ix(pool_store[t], size, temN)) == ALLOC_FAIL) 
            {
                perror("FAILED TO REMOVE FROM GAP");
                exit(99);
            }
            pool_store[t]->pool.num_allocs += 1;
            pool_store[t]->pool.alloc_size += size;
            return (alloc_pt)temN;
        }
        newNode = temN;
    }
    else if(pool_store[t]->pool.policy == BEST_FIT)
    {
        int found = false;
        int x = 0;
        for(int y = 0; y < pool_store[t]->pool.num_gaps; ++y)
        {
            if(size <= pool_store[t]->gap_ix[y].size)
            {
                x = y;
                found = true;
                break;
            }
        }
        if(!found)
        {
            return NULL;
        }
        if(pool_store[t]->gap_ix[x].size > size)
        {
            temN= pool_store[t]->gap_ix[x].node;
            while(newNode->used != 0 || newNode == temN)
            {
                ++newNode;
            }
            newNode->alloc_record.mem = temN->alloc_record.mem + size;
            newNode->alloc_record.size = temN->alloc_record.size - size;
            temN->alloc_record.size = size;
            temN->allocated = 1;
            newNode->used = 1;
            newNode->allocated = 0;
            newNode->prev = temN;
            if(temN->next)
            {
                newNode->next = temN->next;
                temN->next->prev = newNode;
                temN->next = newNode;
            }
            else
            {
                temN->next = newNode;
            }
            if((_mem_remove_from_gap_ix(pool_store[t], size, temN)) == ALLOC_FAIL)
            {
                perror("Failed to remove from gap");
            }
            if((_mem_add_to_gap_ix(pool_store[t], newNode->alloc_record.size, newNode)) == ALLOC_FAIL)
            {
                perror("Failed add to gap");
            }
            newNode = temN;
        }
        else
        {
            pool_store[t]->gap_ix[x].node->allocated = 1;
            newNode = pool_store[t]->gap_ix[x].node;
            if((_mem_remove_from_gap_ix(pool_store[t], size, newNode)) == ALLOC_FAIL)
            {
                perror("Failed to remove from gap");
            }
            pool_store[t]->pool.num_allocs += 1;
            pool_store[t]->pool.alloc_size += size;
            return (alloc_pt)newNode;
        }
    }
    else
    {
        perror("No policy given\n");
    }
    pool_store[t]->used_nodes += 1;
    pool_store[t]->pool.num_allocs += 1;
    pool_store[t]->pool.alloc_size += size;
    return (alloc_pt)newNode;
}

alloc_status mem_del_alloc(pool_pt pool, alloc_pt alloc)
{
    // TODO implement
    unsigned int i = 0;
    pool_mgr_pt boss = (pool_mgr_pt)pool;
    for(i = 0; i < pool_store_size; ++i)
    {
        if(pool_store[i] == boss)
        {
            break;
        }
    }
    node_pt finder = pool_store[i]->node_heap;
    node_pt deleter = (node_pt)alloc;
    while(finder != deleter)
    {
        finder = finder->next;
    }
    deleter = finder;
    deleter->allocated = 0;
    pool_store[i]->pool.num_allocs -= 1;
    pool_store[i]->pool.alloc_size -= deleter->alloc_record.size;
    if(deleter->next)
    {
        if(deleter->next->used == 1 && deleter->next->allocated == 0)
        {
            deleter->alloc_record.size += deleter->next->alloc_record.size;
            if((_mem_remove_from_gap_ix(pool_store[i], deleter->next->alloc_record.size, deleter->next)) == ALLOC_FAIL)
            {
                perror("FAILED TO REMOVE FROM GAP");
                exit(99);
            }
            deleter->next->used = 0;
            pool_store[i]->used_nodes -= 1;
            node_pt DNode = deleter->next;
            if(deleter->next->next)
            {
                deleter->next->next->prev = deleter;
                deleter->next = deleter->next->next;
            }
            else
            {
                deleter->next = NULL;
            }
            DNode->next = NULL;
            DNode->prev = NULL;
        }
    }

    if(deleter->prev)
    {
        if(deleter->prev->used == 1 && deleter->prev->allocated == 0)
        {
            deleter->used = 0;
            if((_mem_remove_from_gap_ix(pool_store[i], deleter->prev->alloc_record.size, deleter->prev)) == ALLOC_FAIL)
            {
                perror("Failed to remove from gap");
                exit(99);
            }
            deleter->prev->alloc_record.size += deleter->alloc_record.size;
            pool_store[i]->used_nodes -= 1;
            node_pt DNode = deleter;
            deleter = deleter->prev;
            if(deleter->next->next)
            {
                deleter->next = deleter->next->next;
                deleter->next->prev = deleter;
            }
            else
            {
                deleter->next->next = NULL;
            }
            DNode->next = NULL;
            DNode->prev = NULL;
        }
    }

    if((_mem_add_to_gap_ix(pool_store[i], deleter->alloc_record.size, deleter)) == ALLOC_OK)
    {
        return ALLOC_OK;
    }
    else
    {
        return ALLOC_FAIL;
    }
}

// NOTE: Allocates a dynamic array. Caller responsible for releasing.
void mem_inspect_pool(pool_pt pool, pool_segment_pt *segments, unsigned *num_segments)
{
    // TODO implement
    unsigned int i = 0;
    pool_mgr_pt boss = (pool_mgr_pt)pool;
    for(i = 0; i < pool_store_size; ++i)
    {
        if(pool_store[i] == boss)
        {
            break;
        }
    }
    pool_segment_pt segs = (pool_segment_pt)calloc(pool_store[i]->used_nodes, sizeof(pool_segment_t));

    node_pt sniffer = pool_store[i]->node_heap;
    for(int j = 0; j < pool_store[i]->used_nodes; ++j)
    {
        segs[j].allocated = sniffer->allocated;
        segs[j].size = sniffer->alloc_record.size;
        if(sniffer->next)
        {
            sniffer = sniffer->next;
        }
    }
    *segments = segs;
    *num_segments = pool_store[i]->used_nodes;
}


/* Definitions of static functions */
static alloc_status _mem_resize_pool_store()
{
    // TODO implement
    pool_store_capacity = pool_store_capacity * MEM_POOL_STORE_EXPAND_FACTOR;
    if((pool_store = realloc(pool_store, sizeof(pool_mgr_pt)*pool_store_capacity)) == NULL)
    {
        return ALLOC_FAIL;
    }
    return ALLOC_OK;
}

static alloc_status _mem_resize_node_heap(pool_mgr_pt pool_mgr)
{
    // TODO implement
    pool_mgr->total_nodes *= MEM_NODE_HEAP_EXPAND_FACTOR;
    if((pool_mgr->node_heap = realloc(pool_mgr->node_heap, sizeof(node_t)*pool_mgr->total_nodes)) == NULL)
    {
        return ALLOC_FAIL;
    }
    return ALLOC_OK;
}

static alloc_status _mem_resize_gap_ix(pool_mgr_pt pool_mgr)
{
    // TODO implement
    pool_mgr->gap_ix_capacity *= MEM_GAP_IX_EXPAND_FACTOR;
    if((pool_mgr->gap_ix = realloc(pool_mgr->gap_ix, sizeof(gap_t)*pool_mgr->gap_ix_capacity)) == NULL)
    {
        return ALLOC_FAIL;
    }
    return ALLOC_OK;
}

static alloc_status _mem_add_to_gap_ix(pool_mgr_pt pool_mgr, size_t size, node_pt node)
{
    // TODO implement
    if(pool_mgr->pool.num_gaps > (pool_mgr->gap_ix_capacity*MEM_GAP_IX_FILL_FACTOR))
    {
        if((_mem_resize_gap_ix(pool_mgr)) == ALLOC_FAIL)
        {
            perror("Gap failed to resize");
            return ALLOC_FAIL;
        }
    }
    pool_mgr->gap_ix[pool_mgr->pool.num_gaps].node = node;
    pool_mgr->gap_ix[pool_mgr->pool.num_gaps].size = size;
    pool_mgr->pool.num_gaps += 1;
    if((_mem_sort_gap_ix(pool_mgr)) == ALLOC_FAIL)
    {
        return ALLOC_FAIL;
    }

    return ALLOC_OK;

}

static alloc_status _mem_remove_from_gap_ix(pool_mgr_pt pool_mgr, size_t size, node_pt node)
{
    // TODO implement
    int i = 0;
    int sniffer = false;
    while(!sniffer && i < pool_mgr->pool.num_gaps)
    {
        if(pool_mgr->gap_ix[i].node == node)
        {
            sniffer = true;
        }
        else
        {
            ++i;
        }
    }
    if(sniffer)
    {
        if(pool_mgr->pool.num_gaps > 1)
        {
            int j = i;
            while(j < (pool_mgr->pool.num_gaps - 1))
            {
                 pool_mgr->gap_ix[j] = pool_mgr->gap_ix[j+1];
                 ++j;
            }
            pool_mgr->gap_ix[pool_mgr->pool.num_gaps-1].node = NULL;
            pool_mgr->gap_ix[pool_mgr->pool.num_gaps-1].size = 0;
            pool_mgr->pool.num_gaps -= 1;
            return ALLOC_OK;
        }
        pool_mgr->gap_ix[pool_mgr->pool.num_gaps-1].node = NULL;
        pool_mgr->gap_ix[pool_mgr->pool.num_gaps-1].size = 0;
        pool_mgr->pool.num_gaps -= 1;
        return ALLOC_OK;
    }
    else
    {
        return ALLOC_FAIL;
    }
}


static alloc_status _mem_sort_gap_ix(pool_mgr_pt pool_mgr)
{
    int swap = false;
    for(int i = 0; i < pool_mgr->pool.num_gaps-1; ++i)
    {
        for(int j = pool_mgr->pool.num_gaps - 1; j > i; --j)
        {
            if(pool_mgr->gap_ix[j-1].size > pool_mgr->gap_ix[j].size)
            {
                swap = true;
                node_pt temp = pool_mgr->gap_ix[j-1].node;
                size_t ts = pool_mgr->gap_ix[j-1].size;
                pool_mgr->gap_ix[j-1] = pool_mgr->gap_ix[j];
                pool_mgr->gap_ix[j].node = temp;
                pool_mgr->gap_ix[j].size = ts;
            }
            else if(pool_mgr->gap_ix[j-1].size == pool_mgr->gap_ix[j].size)
            {
                if(pool_mgr->gap_ix[j-1].node > pool_mgr->gap_ix[j].node)
                {
                    swap = true;
                    node_pt temp = pool_mgr->gap_ix[j-1].node;
                    size_t ts = pool_mgr->gap_ix[j-1].size;
                    pool_mgr->gap_ix[j-1] = pool_mgr->gap_ix[j];
                    pool_mgr->gap_ix[j].node = temp;
                    pool_mgr->gap_ix[j].size = ts;
                }
            }
        }
        if(!swap)
        {
            break;
        }
    }

    return ALLOC_OK;
}


