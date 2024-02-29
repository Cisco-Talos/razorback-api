#include "config.h"
#include <razorback/debug.h>
#include <razorback/types.h>
#include <razorback/log.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _MSC_VER
#include <io.h>
#include <direct.h>
#include "bobins.h"
#else
#include <sys/mman.h>
#endif
#include <fcntl.h>

#include "transfer/core.h"
#include "runtime_config.h"

static bool File_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher);
static bool File_Fetch(struct Block *block, struct ConnectedEntity *dispatcher);
static void File_Free(struct Block *block);
static struct TransportDescriptor descriptor =
{
    0,
    "File",
    "Transfer file via shared file system",
    File_Store,
    File_Fetch,
    File_Free
};

bool 
File_Init(void)
{
    return Transport_Register(&descriptor);
}

static bool
createDirectory(struct Block *block, const char *basepath)
{
    char *hash;
    char *dir;
    char *dir2, *dir3, *dir4;

    hash = Transfer_generateFilename(block);
    if (asprintf (&dir2, "%s/%c", basepath, hash[0]) == -1)
    {
        rzb_log (LOG_ERR, "%s: Could not allocate directory string",
                 __func__);
        free (hash);
        return false;
    }

    if (access (dir2, F_OK) == -1)
    {
#ifdef _MSC_VER
		if (_mkdir (dir2) != 0)
#else
        if (mkdir (dir2, S_IRUSR | S_IWUSR | S_IXUSR |
                   S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1)	
#endif
        {
            rzb_log (LOG_ERR, "%s: Error creating directory %s", __func__,
                     dir2);
            free (hash);
            free (dir2);
            return false;
        }
    }

    if (asprintf (&dir3, "%s/%c/%c", basepath, hash[0], hash[1]) == -1)
    {
        rzb_log (LOG_ERR, "%s: Could not allocate directory string",
                 __func__);
        free (hash);
        free (dir2);
        return false;
    }

    if (access (dir3, F_OK) == -1)
    {
#ifdef _MSC_VER
		if (_mkdir (dir3) != 0)
#else
        if (mkdir (dir3, S_IRUSR | S_IWUSR | S_IXUSR |
                   S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1)	
#endif
        {
            rzb_log (LOG_ERR, "%s: Error creating directory %s", __func__,
                     dir3);
            free (hash);
            free (dir2);
            free (dir3);
            return false;
        }
    }

    if (asprintf (&dir4, "%s/%c/%c/%c", basepath, hash[0], hash[1], hash[2])
        == -1)
    {
        rzb_log (LOG_ERR, "%s: Could not allocate directory string",
                 __func__);
        free (hash);
        free (dir2);
        free (dir3);
        return false;
    }

    if (access (dir4, F_OK) == -1)
    {
#ifdef _MSC_VER
		if (_mkdir (dir4) != 0)
#else
        if (mkdir (dir4, S_IRUSR | S_IWUSR | S_IXUSR |
                   S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1)	
#endif
        {
            rzb_log (LOG_ERR, "%s: Error creating directory %s", __func__,
                     dir4);
            free (hash);
            free (dir2);
            free (dir3);
            free (dir4);
            return false;
        }
    }


    if (asprintf
        (&dir, "%s/%c/%c/%c/%c", basepath, hash[0], hash[1], hash[2],
         hash[3]) == -1)
    {
        rzb_log (LOG_ERR, "%s: Could not allocate directory string",
                 __func__);
        free (hash);
        free (dir2);
        free (dir3);
        free (dir4);
        return false;
    }

    if (access (dir, F_OK) == -1)
    {
#ifdef _MSC_VER
		if (_mkdir (dir) != 0)
#else
        if (mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR |
                   S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1)	
#endif
        {
            rzb_log (LOG_ERR, "%s: Error creating directory %s", __func__,
                     dir);
            free (hash);
            free (dir);
            free (dir2);
            free (dir3);
            free (dir4);
            return false;
        }
    }
    
    return true;
}

static uint32_t
writeWrap (int fd, uint8_t * data, uint64_t length)
{

    int SizeDword;
    int totalbytes = 0;
    int bytessofar;

    SizeDword = (int) length;

    while (totalbytes < SizeDword)
    {
        bytessofar = write (fd, data + totalbytes, SizeDword - totalbytes);
        if (bytessofar == -1)
        {
            rzb_perror ("writeWrap: Could not write data to file: %s");
            return 0;
        }
        totalbytes += bytessofar;
    }

    return 1;
}

static bool 
File_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher)
{
    int fd;
    char *filename =NULL;
    char *path = NULL;
    struct BlockPoolData *dataItem = NULL;

	ASSERT (item != NULL);

    if ((filename = Transfer_generateFilename (item->pEvent->pBlock)) == NULL)
    {
        rzb_log (LOG_ERR, "%s: failed to generate file name", __func__);
        return false;
    }
    if (!createDirectory(item->pEvent->pBlock, Config_getLocalityBlockStore()))
    {
        rzb_log (LOG_ERR, "%s: failed to create storage dir", __func__);
        return false;
    }
    if (asprintf(&path, "%s/%c/%c/%c/%c/%s", Config_getLocalityBlockStore(),
                filename[0], filename[1], filename[2], filename[3], filename) == -1)
    {
        rzb_log (LOG_ERR, "%s: failed to generate file path", __func__);
        return false;
    }
    if ((fd = open(path, O_RDONLY, 0)) != -1)
    {
        close(fd);
        free(filename);
        free(path);
        return true;
    }
    fd = open (path, O_RDWR | O_CREAT | O_TRUNC,
               S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1)
    {
        rzb_perror ("StoreDataAsFile: Could not open file for writing: %s");
        free (filename);
        return 0;
    }
    dataItem = item->pDataHead;
    while (dataItem != NULL)
    {
        if ((writeWrap (fd, dataItem->pData, dataItem->iLength)) == 0)
        {
            rzb_log (LOG_ERR, "%s: Write failed.", __func__);
            free (filename);
            close (fd);
            return false;
        }
        dataItem = dataItem->pNext;
    }

    close (fd);

    free (filename);
    free (path);

    return true;

}

static bool 
File_Fetch(struct Block *block, struct ConnectedEntity *dispatcher)
{
    int fd;
    char *filename = NULL;
    char *path = NULL;
    struct stat fs;

	ASSERT (block != NULL);

    if ((filename = Transfer_generateFilename (block)) == NULL)
    {
        rzb_log (LOG_ERR, "%s: failed to generate file name", __func__);
        return false;
    }
    if (asprintf(&path, "%s/%c/%c/%c/%c/%s", Config_getLocalityBlockStore(),
                filename[0], filename[1], filename[2], filename[3], filename) == -1)
    {
        rzb_log (LOG_ERR, "%s: failed to generate file path", __func__);
        return false;
    }

    fd = open (path, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    free (filename); filename = NULL;
    free (path); path = NULL;
    if (fd == -1)
    {
        rzb_perror
            ("RetrieveDataAsFile: Could not open file for reading: %s");
        return false;
    }

    if (fstat (fd, &fs) == -1)
    {
        rzb_perror ("RetrieveDataAsFile: Could not stat file: %s");
        close (fd);
        return false;
    }

#ifdef _MSC_VER
	//MapViewOfFile(...)
	UNIMPLEMENTED();
#else //_MSC_VER
    block->pData = mmap (NULL, fs.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
#endif //_MSC_VER

    //Do a sanity check on BiD

    close (fd);

    return true;

}
static void 
File_Free(struct Block *block)
{
    if (block->pData == NULL)
        return;
#ifdef _MSC_VER
	UNIMPLEMENTED();
	//UnmapViewOfFile
#else //_MSC_VER
    munmap(block->pData, block->pId->iLength);
#endif
}

