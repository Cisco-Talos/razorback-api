#include "config.h"

#include <stdio.h>
#include <pthread.h>

#include "rzb_cache.h"
#include "rzb_conf.h"

typedef struct _CACHE
{
    unsigned size;
    unsigned entries;
    unsigned listSize[LT_NONE];
    double target;
    ENTRY *LRU[LT_NONE];
    ENTRY *MRU[LT_NONE];
    ENTRY *entrylist;
} CACHE;

static pthread_once_t once_cache = PTHREAD_ONCE_INIT;
static pthread_mutex_t cachemutex = PTHREAD_MUTEX_INITIALIZER;

static CACHE goodcache;
static CACHE badcache;
static CACHE URLcache;

static void initcache(void);
static inline ENTRY *getNewEntry (CACHE *cache);
static inline ENTRY *PurgeLRU(LISTTYPE, CACHE *);
static inline void AddMRU(ENTRY *, LISTTYPE, CACHE *);
static inline ENTRY *FindMRU(LISTTYPE, CACHE *);
static inline ENTRY *FindLRU(LISTTYPE, CACHE *);
static inline ENTRY *FindEntry(ENTRY *, CACHE *, CACHETYPE);
static inline void removeEntry(ENTRY *, CACHE *);
static inline ENTRY *replace(CACHE *);
static inline double max(double, unsigned);
static inline double min(double, unsigned);

HRESULT checkLocalEntry(ENTRY *entry, CACHETYPE type)
{
    CACHE *cache;

    pthread_mutex_lock(&cachemutex);

    pthread_once(&once_cache, initcache);

    //Set Cache pointer based on CACHETYPE value
    switch (type)
    {
        case GOODMD5:
            cache = &goodcache;
            break;

        case BADMD5:
            cache = &badcache;
            break;

        case URL:
            cache = &URLcache;
            break;

        default:
            pthread_mutex_unlock(&cachemutex);
            return R_NOT_FOUND;
    }

    ENTRY *cachehit = FindEntry(entry, cache, type);

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
                printf("\nShould never get here\n");
        }

        pthread_mutex_unlock(&cachemutex);

        return R_FOUND;
    }

    pthread_mutex_unlock(&cachemutex);

    return R_NOT_FOUND;
}

HRESULT addLocalEntry(ENTRY *entry, CACHETYPE type)
{
    CACHE *cache;
    ENTRY *newentry;

    pthread_mutex_lock(&cachemutex);

    pthread_once(&once_cache, initcache);

    switch (type)
    {
        case GOODMD5:
            cache = &goodcache;
            break;

        case BADMD5:
            cache = &badcache;
            break;

        case URL:
            cache = &URLcache;
            break;
        default:
            pthread_mutex_unlock(&cachemutex);
            return R_FAIL;
    }

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
    newentry->size = entry->size;

    switch (type)
    {
        case GOODMD5:
        case BADMD5:
            memcpy(newentry->d.chksum, entry->d.chksum, sizeof(newentry->d.chksum));
            break;
        case URL:
            newentry->d.url = strdup(entry->d.url);
            break;
    }

    pthread_mutex_unlock(&cachemutex);
    return R_SUCCESS;
}


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
    else
        return NULL;
}

//Find entry in the cache
static inline ENTRY *FindEntry(ENTRY *entry, CACHE *cache, CACHETYPE type)
{
    unsigned i;
    switch (type)
    {
        case GOODMD5:
        case BADMD5:
            for (i = 0; i < cache->entries; i++)
            {
                if (cache->entrylist[i].size == entry->size && !memcmp(cache->entrylist[i].d.chksum, entry->d.chksum, entry->size))
                    return &cache->entrylist[i];
            }
            break;

        case URL:
            for (i = 0; i < cache->entries; i++)
            {
                if (cache->entrylist[i].size == entry->size && !strcmp(cache->entrylist[i].d.url, entry->d.url))
                    return &cache->entrylist[i];
            }
            break;
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

static void initcache(void)
{
    goodcache.size = rzbconfig.goodcachesize;
    goodcache.entries = 0;
    goodcache.entrylist = malloc(goodcache.size * 2 * sizeof(ENTRY));
    goodcache.target = goodcache.size;

    badcache.size = rzbconfig.badcachesize;
    badcache.entries = 0;
    badcache.entrylist = malloc(badcache.size * 2 * sizeof(ENTRY));
    badcache.target = badcache.size;

    URLcache.size = rzbconfig.urlcachesize;
    URLcache.entries = 0;
    URLcache.entrylist = malloc(URLcache.size * 2 * sizeof(ENTRY));
    URLcache.target = URLcache.size;

    printf("\nCache initialized\n");
}

void finicache(void)
{
    pthread_mutex_lock(&cachemutex);

    if (badcache.entrylist)
        free(badcache.entrylist);
    if (goodcache.entrylist)
        free(goodcache.entrylist);
    if (URLcache.entrylist)
    {
        unsigned i;

        for (i = 0; i < URLcache.entries; i++)
        {
            if (URLcache.entrylist[i].d.url)
                free(URLcache.entrylist[i].d.url);
        }

        free(URLcache.entrylist);
    }

    pthread_mutex_unlock(&cachemutex);
}

