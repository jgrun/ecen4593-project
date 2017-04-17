/*
* src/direct.c
* implementation of a direct mapped cache
*/

#include "direct.h"


extern int flags;

direct_cache_t * direct_cache_init(uint32_t num_blocks, uint32_t block_size){
    //The linear memory that the cache blocks point to
    word_t *words = (word_t *)malloc(sizeof(word_t)*num_blocks*block_size);
    //The cache struct itself
    direct_cache_t *cache = (direct_cache_t *)malloc(sizeof(direct_cache_t));
    //Each block that contains tag info, dirty and valid bits, etc
    direct_cache_block_t *blocks = (direct_cache_block_t *)malloc(sizeof(direct_cache_block_t) * num_blocks);

    //Individual valid bits for each word so we can have early start...
    bool *valids = (bool *)malloc(sizeof(bool) * num_blocks*block_size);

    //set up each block to point to its corresponding word in memory
    uint32_t i = 0;
    for(i = 0; i < num_blocks; i++){
        blocks[i].data = words + i * block_size;
        blocks[i].valid = valids + i * block_size;
    }

    cache->blocks = blocks;
    cache->words = words;
    //crash if unable to allocate memory
    if(cache == NULL || cache->blocks == NULL){
        printf(ANSI_C_RED "cache_init: Unable to allocate direct mapped cache\n" ANSI_C_RESET);
        assert(0);
    }
    cache->num_blocks = num_blocks;
    cache->block_size = block_size;

    //get the number of bits the index takes up
    uint32_t index_size = 1;
    while((num_blocks>>index_size) != 0) index_size+=1;
    cache->index_size = index_size;
    //cache size is 2^n blocks, so n bits are needed for the index
    //block size is 2^m words, m bits needed for word within block
    //tag bits = 32 - (n + m + 2)
    //This is magic. Look in the header file for some sort of explanation
    uint32_t inner_index_size = 1;
    while((block_size>>inner_index_size) != 0) inner_index_size+=1;
    cache->inner_index_size = inner_index_size;
    cache->inner_index_mask = ((1 << (inner_index_size + 1)) - 1) & ~3;
    cache->tag_size = 32 - cache->index_size - cache->inner_index_size - 2;
    cache->index_mask = ((1 << (cache->index_size + cache->inner_index_size + 2)) - 1);
    cache->tag_mask = ~cache->index_mask;
    cache->index_mask &= ~(cache->inner_index_mask | 0x3);

    if(flags & MASK_DEBUG){
        printf("creating cache masks...\n");
        printf("tag_mask: 0x%08x\n", cache->tag_mask);
        printf("index_mask: 0x%08x\n", cache->index_mask);
        printf("inner_index_mask: 0x%08x\n", cache->inner_index_mask);
    }

    //Set up the fetch variables
    cache->fetching = false;
    cache->penalty_count = 0;
    cache->subsequent_fetching = 0;

    //Invalidate all data in the cache
    uint8_t j;
    for(i = 0; i < cache->num_blocks; i++){
        for(j = 0; j < cache->block_size; j++){
            cache->blocks[i].valid[j] = false;
        }
    }
    return cache;
}

void direct_cache_free(direct_cache_t *cache){

    free(cache->words);
    free(cache->blocks);
    free(cache);
}


void direct_cache_digest(direct_cache_t *cache, memory_status_t proceed_condition){
    cache_access_t info;
    direct_cache_get_tag_and_index(&info, cache, &(cache->target_address));
    if(get_mem_status() == proceed_condition){
        //Increment the wait count
        cache->penalty_count++;
        if(flags & MASK_DEBUG){
            printf("\tdirect_cache_digest: Value of incremented penalty_count %d\n",cache->penalty_count);
        }
        if(cache->penalty_count == CACHE_MISS_PENALTY){
            //Finished waiting, get data and return it
            if(flags & MASK_DEBUG){
                printf("\tdirect_cache_digest: Reached stall count retreiveing data.\n");
            }
            mem_read_w(cache->target_address, &info.data);
            cache->blocks[info.index].data[info.inner_index] = info.data;
            cache->blocks[info.index].tag = info.tag;
            cache->blocks[info.index].valid[info.inner_index] = true;
            cache->blocks[info.index].dirty = false;
            cache->fetching = false;
            cache->penalty_count = 0;
            if(cache->subsequent_fetching != (cache->block_size - 1)){
                //get the second word in the block
                info.address |= (1 << 2);
                cache->subsequent_fetching = 1;
                direct_cache_queue_mem_access(cache, info);
            }
            return;
        }
        if(cache->subsequent_fetching && (cache->penalty_count == CACHE_MISS_SUBSEQUENT_PENALTY)){
            //Have the next word for the block
            mem_read_w(cache->target_address, &info.data);
            cache->blocks[info.index].data[info.inner_index] = info.data;
            cache->blocks[info.index].tag = info.tag;
            cache->blocks[info.index].valid[info.inner_index] = true;
            cache->blocks[info.index].dirty = false;
            cache->fetching = false;
            cache->penalty_count = 0;
            if(cache->subsequent_fetching != (cache->block_size - 1)){
                //get the next word for the block
                cache->subsequent_fetching++;
                info.address |= (cache->subsequent_fetching << 2);
                direct_cache_queue_mem_access(cache, info);
            }
            return;
        }
    }

}


cache_status_t direct_cache_read_w(direct_cache_t *cache, uint32_t *address, uint32_t *data){
    cache_access_t info;
    direct_cache_get_tag_and_index(&info, cache, address);
    info.request = CACHE_READ;

    //Some index checking to make sure we don't seg fault
    if(info.index >= cache->num_blocks){
        printf(ANSI_C_RED "direct_cache_read_w: Index %d requested out of range\n" ANSI_C_RESET, info.index);
        assert(0);
    }
    //Make sure memory is initialized
    if(cache->blocks == NULL){
        printf(ANSI_C_RED "direct_cache_read_w: Cache not initialized\n" ANSI_C_RESET);
        assert(0);
    }
    if(flags & MASK_DEBUG){
        printf("\tdirect_cache_read_w: Reading from cache block %d\n", info.index);
    }
    //Check to make sure the data is valid
    if(cache->blocks[info.index].valid[info.inner_index] == true && cache->blocks[info.index].tag == info.tag){
        info.data = cache->blocks[info.index].data[info.inner_index];
        info.dirty = cache->blocks[info.index].dirty;
        if(flags & MASK_DEBUG){
            printf("\tdirect_cache_read_w: CACHE_HIT Found valid data 0x%08x for address 0x%08x\n", info.data, info.address);
            if(info.dirty){
                printf("\tdirect_cache_read_w: Block is dirty\n");
            }
        }
        *data = info.data;
        return CACHE_HIT;
    }
    else {
        if(flags & MASK_DEBUG){
            printf("\tdirect_cache_read_w: CACHE_MISS: Data at requested address is not in the cache\n");
        }
        if(cache->fetching){
            if(flags & MASK_DEBUG){
                printf("\tdirect_cache_read_word: CACHE_MISS, cache is fetching data.\n");
            }
        } else {
            //Data is not in the cache. Start retrieval
            if(flags & MASK_DEBUG){
                printf("\tdirect_cache_read_word: CACHE_MISS, data is not in the cache. Queueing read\n");
            }
            info.request = CACHE_READ;
            direct_cache_queue_mem_access(cache, info);
        }
        return CACHE_MISS;
    }

}

cache_status_t direct_cache_write_w(direct_cache_t *cache, uint32_t *address, uint32_t *data){
    cache_access_t info;
    cache_status_t status;
    direct_cache_get_tag_and_index(&info, cache, address);
    info.data = *data;
    if(cache->blocks[info.index].valid[info.inner_index] == true && cache->blocks[info.index].tag == info.tag){
        status = CACHE_HIT;
        if(info.dirty && write_policy == WRITEBACK){
            //There is valid dirty data in the cache, and we must put it in the write buffer
            status = write_buffer_enqueue(info);
            if(status == CACHE_MISS){
                if(flags & MASK_DEBUG){
                    printf("\tdirect_cache_access_word: Write buffer is full. Cannot fill cache without losing data.\n");
                }
                //The write buffer is full! Don't fill the block
                return CACHE_MISS;
            }
        }
        cache->blocks[info.index].data[info.inner_index] = info.data;
        cache->blocks[info.index].tag = info.tag;
        cache->blocks[info.index].dirty = true;
    } else {
        //The processor is writing to a place in memory that isnt in the cache
        //The transaction becomes a READ MODIFY WRITE
        if(flags & MASK_DEBUG){
            printf("\tdirect_cache_access_word: no valid data in the cache for the specified address.\n");
        }
        return CACHE_MISS;
    }

    return status;
}




void direct_cache_queue_mem_access(direct_cache_t *cache, cache_access_t info){
    if(flags & MASK_DEBUG){
        printf("\tdirect_cache_queue_mem_access: Queueing memory access for address 0x%08x\n", info.address);
    }
    cache->fetching = true;
    if(cache->subsequent_fetching == 0){
        //We must get the first word in a block first
        cache->target_address = info.address & (cache->tag_mask | cache->index_mask);
    }
    else {
        cache->target_address = info.address;
    }
    cache->penalty_count = 0;
    if(flags & MASK_DEBUG && cache->block_size > 1){
        printf("\tdirect_cache_queue_mem_access: Actual requested address will be 0x%08x\n", cache->target_address);
    }
}

void direct_cache_get_tag_and_index(cache_access_t *info, direct_cache_t *cache, uint32_t *address){
    info->index = (*address & cache->index_mask) >> (2 + cache->inner_index_size);
    info->tag = (*address & cache->tag_mask) >> (2 + cache->index_size + cache->inner_index_size);
    info->inner_index = (*address & cache->inner_index_mask) >> 2;
    info->address = *address;
}
