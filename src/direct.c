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

    //set up each block to point to its corresponding word in memory
    int i = 0;
    for(i = 0; i < num_blocks*block_size; i+=block_size){
        blocks[i].data = words + i * block_size;
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
    uint32_t i = 0;
    for(i = 0; i < cache->num_blocks; i++){
        cache->blocks[i].valid = false;
    }
    return cache;
}

void direct_cache_free(void){
    int i;
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
        if(cache->penalty_count == CACHE_MISS_PENALTY){
            //Finished waiting, get data and return it
            direct_cache_fill_word(info);
            cache->fetching = false;
            cache->penalty_count = 0;
            if(cache->subsequent_fetching != (cache->block_size - 1)){
                //get the second word in the block
                info.address |= (1 << 2)
                cache->subsequent_fetching = 1;
                direct_cache_queue_mem_access(cache, info);
            }
            return;
        }
        if(cache->subsequent_fetching && (cache->penalty_count == CACHE_MISS_SUBSEQUENT_PENALTY)){
            //Have the next word for the block
            direct_cache_fill_word(cache, info);
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


cache_status_t direct_cache_read_word(direct_cache_t *cache, uint32_t *address, uint32_t *data){
    cache_access_t info;
    direct_cache_get_tag_and_index(&info, cache, address);

    //Some index checking to make sure we don't seg fault
    if(info.index >= cache->num_blocks){
        printf(ANSI_C_RED "direct_cache_read_word: Index %d requested out of range\n" ANSI_C_RESET, info.index);
        assert(0);
    }
    //Make sure memory is initialized
    if(cache->blocks == NULL){
        printf(ANSI_C_RED "direct_cache_read_word: Cache not initialized\n" ANSI_C_RESET);
        assert(0);
    }

    cache_status_t status = direct_cache_access_word(cache, &info);
    if(status == CACHE_HIT){
        *data = info.data;
        return CACHE_HIT;
    } else {
        if(cache->fetching){
            if(flags & MASK_DEBUG){
                printf("\tCache is busy\n");
            }
            return CACHE_MISS;
        } else {
            //Data is not in the cache. Either start the retrieval, wait, or fill depending on the stall count
            direct_cache_queue_mem_access(cache, info);
            return CACHE_MISS;
        }
    }
}

cache_status_t direct_cache_write_w(direct_cache_t *cache, uint32_t *address, uint32_t *data){
    cache_access_t info;
    direct_cache_get_tag_and_index(&info, cache, address);
    direct_cache_block_t block = cache->blocks[info.index];

}


void direct_cache_fill_word(direct_cache_t *cache, cache_access_t info){
    //Some preventative measures to make sure a block contains data from the same tag
    if(info.inner_index != 0){
        //make sure the current tag matches the tag thats already in the block
        if(info.tag != cache->blocks[info.index].tag){
            printf(ANSI_C_RED "direct_cache_fill_word: Inconsistent Tag Data. Aborting\n"ANSI_C_RESET);
            assert(0);
        }
        //make sure the first word in the block is valid
        if(cache->blocks[info.index].valid == false){
            printf(ANSI_C_RED "direct_cache_fill_word: First word in block is not valid. Aborting\n"ANSI_C_RESET);
            assert(0);
        }
    }
    uint32_t temp = 0;
    mem_read_w(info.address, &temp);
    cache->blocks[info.index].data[info.inner_index] = temp;
    cache->blocks[info.index].tag = info.tag;
    cache->blocks[info.index].valid = true;
}

cache_status_t direct_cache_access_word(direct_cache_t *cache, cache_access_t *info){
    //Helper function for reading a word out of a cache block.
    //Check to make sure the data is valid
    if(cache->blocks[info->index].valid == true){
        info->data = cache->blocks[info->index].data[inner_index];
        return CACHE_HIT;
    }
    else {
        return CACHE_MISS;
    }
}

void direct_cache_queue_mem_access(direct_cache_t *cache, cache_access_t info){
    cache->fetching = true;
    if(cache->subsequent_fetching == 0){
        //We must get the first word in a block first
        cache->target_address = info.address & (cache->tag_mask | cache->index_mask);
    }
    else {
        cache->target_address = info.address;
    }
    cache->penalty_count = 0;
}

void direct_cache_get_tag_and_index(cache_access_t *info, direct_cache_t *cache, uint32_t *address){
    info->index = (*address & cache->index_mask) >> (2 + cache->inner_index_size);
    info->tag = (*address & cache->tag_mask) >> (2 + cache->index_size + cache->inner_index_size);
    info->inner_index = (*address & cache->inner_index_mask) >> 2;
    info->address = *address;
}
