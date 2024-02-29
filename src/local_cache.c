#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "local_cache.h"
#include "config.h"
#include "runtime_config.h"
#include <razorback/debug.h>
#include <razorback/log.h>

static CACHE Cache[ALL];

static inline ENTRY *getNewEntry (CACHE *cache);
static inline ENTRY *PurgeLRU(LISTTYPE, CACHE *);
static inline void AddMRU(ENTRY *, LISTTYPE, CACHE *);
static inline ENTRY *FindMRU(LISTTYPE, CACHE *);
static inline ENTRY *FindLRU(LISTTYPE, CACHE *);
static inline ENTRY *FindEntry(ENTRY *, CACHE *);
static inline void removeEntry(ENTRY *, CACHE *);
static inline ENTRY *replace(CACHE *);
static inline double max(double, unsigned);
static inline double min(double, unsigned);
static inline void destroyEntry(ENTRY *);

Lookup_Result checkLocalEntry(uint8_t *key, uint32_t size, uint32_t *sfflags, uint32_t *entflags, CacheType type) {
    
	CACHE *cache;
	ENTRY entry;

    //Set Cache pointer based on CACHETYPE value    
    if (type >= ALL) {
        rzb_log(LOG_ERR, "checkLocalEntry: Invalid CacheType passed as argument"); 
        return R_ERROR;
	}

	cache = &Cache[type];

    if (sfflags == NULL || entflags == NULL || key == NULL) {
        rzb_log(LOG_ERR, "checkLocalEntry: NULL pointer passed as argument"); 
		return R_ERROR;
	}
 
    pthread_mutex_lock(&cache->cachemutex);

	entry.next = NULL;
	entry.prev = NULL;
	entry.size = size;
	entry.key = key;
	entry.listtype = LT_NONE;
	entry.sfflags = *sfflags;
	entry.entflags = *entflags;

    ENTRY *cachehit = FindEntry(&entry, cache);

    if (cachehit != NULL)
    {
        switch (cachehit->listtype)
        {
            //Cache hits
            case LT_T1:
                //In T1? Cache is now frequent; grows T2, shrinks T1
                cache->listSize[LT_T1]--;
                cache->listSize[LT_T2]++;
            case LT_T2:
                //In T2? Reinsert entry as MRU, caches stay the same size
                //*****Check to make sure hit isn't already T2 MRU

                //Remove entry and insert it at top of T2
                removeEntry(cachehit, cache);
                AddMRU(cachehit, LT_T2, cache);
                break;

                //Ghost cache hits
            case LT_B1:
            case LT_B2:
                //If Cache hit in B1, trend target size towards Recency
                if (cachehit->listtype == LT_B1)
                {
                    cache->target = min(cache->target + max(cache->listSize[LT_B2]/cache->listSize[LT_B1], 1), cache->size);
                    cache->listSize[LT_B1]--;
                }
                //If Cache hit in B2, trend target size towards Frequency
                else
                {
                    cache->target = max(cache->target - max(cache->listSize[LT_B1]/cache->listSize[LT_B2], 1), 0);
                    cache->listSize[LT_B2]--;
                }

                //Remove from respective ghost cache
                removeEntry(cachehit, cache);
                //Shift whole list down
                replace(cache);
                //Reenter into frequency cache
                cache->listSize[LT_T2]++; //Think this needs to be here
                AddMRU(cachehit, LT_T2, cache);
                break;

            case LT_NONE:
                rzb_log(LOG_ERR, "checkLocalEntry: Unexpected listtype value, possible memory corruption");
				pthread_mutex_unlock(&cache->cachemutex);
				return R_ERROR;
        }

        pthread_mutex_unlock(&cache->cachemutex);

        *entflags = cachehit->entflags;
		*sfflags = cachehit->sfflags;

        return R_FOUND;
    }

    pthread_mutex_unlock(&cache->cachemutex);

    return R_NOT_FOUND;
}

Lookup_Result addLocalEntry(uint8_t *key, uint32_t size, uint32_t sfflags, uint32_t entflags, CacheType type)
{
    CACHE *cache;
    ENTRY *newentry;

    if (type >= ALL) {
		rzb_log(LOG_ERR, "addLocalEntry: Invalid CacheType passed as argument");
        return R_ERROR;
    }

    cache = &Cache[type];

    pthread_mutex_lock(&cache->cachemutex);
    
	//Cache Miss (aka: Addentry)
    //B1 + T1 full?
    if (cache->listSize[LT_T1] + cache->listSize[LT_B1] == cache->size)
    {
        if (cache->listSize[LT_T1] < cache->size)
        {
            newentry = PurgeLRU(LT_B1, cache); //remove LRU from B1, and give its old memory to newentry
            cache->listSize[LT_B1]--;
            replace(cache); //Shift values down list
        }
        else
        {
            newentry = PurgeLRU(LT_T1, cache); //remove LRU from T1, give its old memory to newentry
            cache->listSize[LT_T1]--;
        }
    }
    else
    {
        if (cache->entries >= cache->size)
        {
            if (cache->entries >= 2*cache->size)
            {
                newentry = PurgeLRU(LT_B2, cache);
                cache->listSize[LT_B2]--;
            }
            else
                newentry = getNewEntry(cache); //initialize new chksum
            replace(cache); //Make room for page
        }
        else
            newentry = getNewEntry(cache);
    }
    
	AddMRU(newentry, LT_T1, cache);
    cache->listSize[LT_T1]++;
    //size and protocol are a union and size is bigger
    //So, referencing the entire dword will copy either
    newentry->size = size;
	newentry->sfflags = sfflags;
	newentry->entflags = entflags;

    if (newentry->key != NULL)
		free(newentry->key);
	if ((newentry->key = (uint8_t *)malloc(size)) == 0) {
	    rzb_log(LOG_ERR, "addLocalEntry: malloctile dysfunction");
		pthread_mutex_unlock(&cache->cachemutex);
		return R_ERROR;
	}
    memcpy(newentry->key, key, size);

    pthread_mutex_unlock(&cache->cachemutex);
    return R_SUCCESS;
}

Lookup_Result updateLocalEntry(uint8_t *key, uint32_t size, uint32_t sfflags, uint32_t entflags, CacheType type) {

    CACHE *cache;
    ENTRY *entry;
    ENTRY temp;

	if (type >= ALL) {
        rzb_log(LOG_ERR, "updateLocalEntry: Invalid CacheType passed as argument"); 
        return R_ERROR;
    }

    cache = &Cache[type];

    if (key == NULL) {
        rzb_log(LOG_ERR, "updateLocalEntry: NULL key passed as argument"); 
        return R_ERROR;
    }

    pthread_mutex_lock(&cache->cachemutex);
    
	temp.next = NULL;
    temp.prev = NULL;
    temp.size = size;
    temp.key = key;
    temp.sfflags = sfflags;
    temp.entflags = entflags;
	
	entry = FindEntry(&temp, cache);

    if (entry == NULL) { 
        //Couldn't find entry to update, it's possible.
		pthread_mutex_unlock(&cache->cachemutex);
        return R_NOT_FOUND;
    }
 
    entry->sfflags = sfflags;
    entry->entflags = entflags;
    pthread_mutex_unlock(&cache->cachemutex);

    return R_SUCCESS;
}

Lookup_Result removeLocalEntry(uint8_t *key, uint32_t size, CacheType type) {
    CACHE *cache;
	ENTRY *entry;
	ENTRY temp;

	if (type >= ALL) {
		rzb_log(LOG_ERR, "removeLocalEntry: Invalid CacheType passed as argument");
		return R_ERROR;
	}

	cache = &Cache[type];

	if (key == NULL) {
		rzb_log(LOG_ERR, "removeLocalEntry: NULL key passed as argument");
		return R_ERROR;
	}

	pthread_mutex_lock(&cache->cachemutex);

	temp.next = NULL;
	temp.prev = NULL;
	temp.size = size;
	temp.key = key;

	entry = FindEntry(&temp, cache);

	if (entry == NULL) {
		pthread_mutex_unlock(&cache->cachemutex);
		return R_NOT_FOUND;
	}

	removeEntry(entry, cache);

	destroyEntry(entry);

	entry->size = 0;

	pthread_mutex_unlock(&cache->cachemutex);

    return R_SUCCESS;
}

/*
uint32_t clearLocalEntry(CacheType type, ClearMethod method) {
    //TODO
}
*/

static inline double max(double a, unsigned b)
{
    return(a > (double)b) ? a : b;
}

static inline double min(double a, unsigned b)
{
    return(a > (double)b) ? b : a;
}

static inline ENTRY *getNewEntry(CACHE *cache)
{
    ENTRY *newentry;

    if (cache->entries < (cache->size*2))
    {
        newentry = &cache->entrylist[cache->entries++];
        memset(newentry, 0, sizeof(*newentry));
        newentry->listtype = LT_NONE;
        return newentry;
    }
    
	rzb_log(LOG_ERR, "getNewEntry: returning NULL, the math is wrong somewhere");
    return NULL;
}

//Find entry in the cache
static inline ENTRY *FindEntry(ENTRY *entry, CACHE *cache)
{
    unsigned i;
    for (i = 0; i < cache->entries; i++)
    {
        if (!memcmp(cache->entrylist[i].key, entry->key, entry->size))
            return &cache->entrylist[i];
    }

    return NULL;
}

//Find MRU in a given list
static inline ENTRY *FindMRU(LISTTYPE listtype, CACHE *cache)
{
    if (cache->MRU[listtype] == NULL)
    {
        unsigned i;
        for (i = 0; i < cache->entries; i++)
        {
            if (cache->entrylist[i].next == NULL && cache->entrylist[i].listtype == listtype)
                cache->MRU[listtype] = &cache->entrylist[i];
        }
    }

    return cache->MRU[listtype];
}

//Find LRU in a given list
static inline ENTRY *FindLRU(LISTTYPE listtype, CACHE *cache)
{
    if (cache->LRU[listtype] == NULL)
    {
        unsigned i;
        for (i = 0; i < cache->entries; i++)
        {
            if (cache->entrylist[i].prev == NULL && cache->entrylist[i].listtype == listtype)
                cache->LRU[listtype] = &cache->entrylist[i];
        }
    }

    return cache->LRU[listtype];
}

//Removes the last entry in a given list
static inline ENTRY *PurgeLRU(LISTTYPE listtype, CACHE *cache)
{

    ENTRY *LRU = FindLRU(listtype, cache);
    //Check that LRU isn't also MRU
    if (LRU)
    {
        if (LRU->next)
        {
            LRU->next->prev = NULL;
            cache->LRU[listtype] = LRU->next;
        }
        else
        {
            //LRU is only entry
            cache->LRU[listtype] = NULL;
            cache->MRU[listtype] = NULL;
        }
    }
    else
    {
        //either you're trying to purge from an empty list
        //or LRU isn't registered or listtype isn't set properly
        printf("\nThis shouldn't happen\n");
        exit(-1);
    }

    LRU->prev = NULL;
    LRU->next = NULL;
    LRU->listtype = LT_NONE;

    return LRU;
}

//Adds an entry to the top of a given list
static inline void AddMRU(ENTRY *newentry, LISTTYPE listtype, CACHE *cache)
{
    ENTRY *MRU;

    //If MRU exists, shift it down for the new King
    if ((MRU = FindMRU(listtype, cache)) != NULL)
    {
        MRU->next = newentry;
        newentry->prev = MRU;
        newentry->next = NULL;
    }
    //Otherwise MRU doesn't exist and sum is now both
    //the new MRU and LRU.
    else
    {
        //Make note that sum is also the LRU
        cache->LRU[listtype] = newentry;
        newentry->prev = NULL;
        newentry->next = NULL;
    }

    //In both cases, set sum to new MRU
    //and copy size and MD5 info
    cache->MRU[listtype] = newentry;
    newentry->listtype = listtype;

    return;
}

//removes an a cache hit for reinsertion
static inline void removeEntry(ENTRY *cachehit, CACHE *cache)
{
    if (cachehit->next)
    {
        if (cachehit->prev)
        {
            //Cachehit is neither LRU nor MRU
            cachehit->next->prev = cachehit->prev;
            cachehit->prev->next = cachehit->next;
        }
        else
        {
            //prev is NULL and cachehit is LRU
            PurgeLRU(cachehit->listtype, cache);
        }
    }
    else
    {
        if (cachehit->prev != NULL)
        {
            //Cachehit is MRU
            cachehit->prev->next = NULL;
            cache->MRU[cachehit->listtype] = cachehit->prev;
        }
        else
        {
            //Cachehit is only member of list
            cache->MRU[cachehit->listtype] = NULL;
            cache->LRU[cachehit->listtype] = NULL;
        }
    }
    cachehit->listtype = LT_NONE;
    cachehit->prev = NULL;
    cachehit->next = NULL;
}

//Migrates entries from T lists to B lists
static inline ENTRY *replace(CACHE *cache)
{
    ENTRY *moveditem = NULL;

    if (cache->listSize[LT_T1] >= max(cache->target,1))
    {
        moveditem = PurgeLRU(LT_T1, cache);
        AddMRU(moveditem, LT_B1, cache);
        --cache->listSize[LT_T1];
        ++cache->listSize[LT_B1];
    }
    else
    {
        moveditem = PurgeLRU(LT_T2, cache);
        AddMRU(moveditem, LT_B2, cache);
        --cache->listSize[LT_T2];
        ++cache->listSize[LT_B2];
    }

    return moveditem;
}

static void destroyEntry(ENTRY *entry) {
	    free(entry->key);
}

//TODO Update config to store the different cache sizes differently
static void __attribute__((constructor)) initcache(void)
{
	unsigned i,j;
	Cache[0].size = Config_getCacheBadLimit ();
	Cache[1].size = Config_getCacheGoodLimit ();
	for (i = 2; i < ALL; i++)
		Cache[i].size = 256;
	for (i = 0; i < ALL; i++) {
        Cache[i].entries = 0;
        Cache[i].entrylist = malloc(Cache[i].size * 2 * sizeof(ENTRY));
        Cache[i].target = Cache[i].size;
		pthread_mutex_init(&Cache[i].cachemutex, NULL);

		for (j = 0; j < LT_NONE; j++) {
			Cache[i].listSize[j] = 0;
			Cache[i].LRU[j] = NULL;
			Cache[i].MRU[j] = NULL;
		}
    }
    rzb_log(LOG_DEBUG, "Cache initialized");
}


//Unused and unneeded
/*
void finicache(void)
{
    unsigned i,j;

    pthread_mutex_lock(&cachemutex);

	for (j = 0; j < ALL; j++) {
        if (Cache[j].entrylist != NULL)
        {
            for (i = 0; i < Cache[j].entries; i++)
            {
                if (Cache[j].entrylist[i].key != NULL)
                    destroyEntry(&Cache[j].entrylist[i]);
            }

            free(Cache[i].entrylist);
	    }
	}

    pthread_mutex_unlock(&cachemutex);
}
*/
