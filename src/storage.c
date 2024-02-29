#include <sys/types.h>
#include <curl/curl.h>
#include "config.h"
#include <razorback/storage.h>
#include <razorback/config_file.h>
#include <razorback/binary_buffer.h>
#include <razorback/messages.h>

static const char *StorageConfFile = ETC_DIR "/dispatcher.conf";
static uint32_t g_CurlBytesWritten = 0;
CURL *CURLinstance = NULL;

struct MessageInspectionTicket *g_StorageConfValues;

//Redo all this... directory creation should not be in filename generation
static char *generateFilename (struct Block *block, char *basepath) {

	uint32_t hashlength;
    char *hash;
	char strLength[21];
    char *dir;
	char *filename;
    char *dir2, *dir3, *dir4;

	if ((hash = Hash_ToText(block->pId->pHash)) == NULL) {
		rzb_log(LOG_ERR, "%s: Could not convert hash to text", __func__);
		return NULL;
	}

	//Check hash length against boundary values
	hashlength = Hash_StringLength(block->pId->pHash);

	snprintf(strLength, 21, "%ju", block->pId->iLength);

 
     if (asprintf(&dir2, "%s/%c", basepath, hash[0]) == -1) {
         rzb_log(LOG_ERR, "%s: Could not allocate directory string", __func__);
         free(hash);
         return NULL;
     }
 
     if (access(dir2, F_OK) == -1) { 
         if (mkdir (dir2, S_IRUSR | S_IWUSR | S_IXUSR |
                                   S_IRGRP | S_IXGRP | S_IROTH |
                                   S_IXOTH) == -1)
         {
             rzb_log(LOG_ERR, "%s: Error creating directory %s", __func__, dir2);
             free(hash);
             free(dir2);
             return NULL;
         }
     }
 
     if (asprintf(&dir3, "%s/%c/%c", basepath, hash[0], hash[1]) == -1) {
         rzb_log(LOG_ERR, "%s: Could not allocate directory string", __func__);
         free(hash);
         free(dir2);
         return NULL;
     }
 
     if (access(dir3, F_OK) == -1) { 
         if (mkdir (dir3, S_IRUSR | S_IWUSR | S_IXUSR |
                                   S_IRGRP | S_IXGRP | S_IROTH |
                                   S_IXOTH) == -1)
         {
             rzb_log(LOG_ERR, "%s: Error creating directory %s", __func__, dir3);
             free(hash);
             free(dir2);
             free(dir3);
             return NULL;
         }
     }
     
     if (asprintf(&dir4, "%s/%c/%c/%c", basepath, hash[0], hash[1], hash[2]) == -1) {
         rzb_log(LOG_ERR, "%s: Could not allocate directory string", __func__);
         free(hash);
         free(dir2);
         free(dir3);
         return NULL;
     }
 
     if (access(dir4, F_OK) == -1) { 
         if (mkdir (dir4, S_IRUSR | S_IWUSR | S_IXUSR |
                                   S_IRGRP | S_IXGRP | S_IROTH |
                                   S_IXOTH) == -1)
         {
             rzb_log(LOG_ERR, "%s: Error creating directory %s", __func__, dir4);
             free(hash);
             free(dir2);
             free(dir3);
             free(dir4);
             return NULL;
         }
     }


    if (asprintf(&dir, "%s/%c/%c/%c/%c", basepath, hash[0], hash[1], hash[2], hash[3]) == -1) {
		rzb_log(LOG_ERR, "%s: Could not allocate directory string", __func__);
		free(hash);
        free(dir2);
        free(dir3);
        free(dir4);
     	return NULL;
	}

	if (access(dir, F_OK) == -1) { 
	    if (mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR | 
						          S_IRGRP | S_IXGRP | S_IROTH | 
								  S_IXOTH) == -1) 
		{
			rzb_log(LOG_ERR, "%s: Error creating directory %s", __func__, dir);
            free(hash);
            free(dir);
            free(dir2);
            free(dir3);
            free(dir4);
			return NULL;
		}
	}

    if ((filename = malloc(hashlength + strlen(strLength) + strlen(dir) + 3)) == NULL) {
        rzb_log(LOG_ERR, "%s: Maloctile Dysfunction", __func__);
        free(hash);
        free(dir);
        free(dir2);
        free(dir3);
        free(dir4);
        return NULL;
    }

	sprintf(filename, "%s/%s.%s", dir, hash, strLength);

	free(hash);
    free(dir);
    free(dir2);
    free(dir3);
    free(dir4);

	return filename;

}

static char * generateURL(struct Block *block) {
    uint32_t hashlength;
    char *hash;
    char strLength[21];
    char *dir;
    char *filename;

    if ((hash = Hash_ToText(block->pId->pHash)) == NULL) {
        rzb_log(LOG_ERR, "%s: Could not convert hash to text", __func__);
        return NULL;
    }

    //Check hash length against boundary values
    hashlength = Hash_StringLength(block->pId->pHash);

    snprintf(strLength, 21, "%ju", block->pId->iLength);
    
    if (asprintf(&dir, "%c/%c/%c/%c", hash[0], hash[1], hash[2], hash[3]) == -1) {
        rzb_log(LOG_ERR, "%s: Could not allocate directory string", __func__);
        free(hash);
        free(filename);
        return NULL;
    }

    if (asprintf(&filename, "%s/%s.%s", dir, hash, strLength) == -1) {
        rzb_log(LOG_ERR, "%s: Could not allocate directory string", __func__);
        free(hash);
        free(filename);
        return NULL;
    }

    free(hash);
    free(dir);

    return filename;

}

static uint32_t writeWrap (int fd, uint8_t *data, uint64_t length) {

    int SizeDword;
	int totalbytes = 0;
	int bytessofar;

    SizeDword = (int)length;

    while (totalbytes < SizeDword) {
        bytessofar = write(fd, data+totalbytes, SizeDword-totalbytes);
        if (bytessofar == -1) {
            rzb_perror("writeWrap: Could not write data to file: %s");
            return 0;
        }
        totalbytes += bytessofar;
    }
    
    return 1; 
}

static uint32_t readWrap (int fd, uint8_t *data, uint64_t length) {

    int SizeDword;
    int totalbytes = 0;
    int bytessofar;

    SizeDword = (int)length;

    while (totalbytes < SizeDword) {
        bytessofar = read(fd, data+totalbytes, SizeDword-totalbytes);
        if (bytessofar == -1) {
            rzb_perror("readWrap: Could not write data to file: %s");
            return 0;
        }
        totalbytes += bytessofar;
    }

    return 1;
}

static uint32_t ticketSize (struct MessageInspectionTicket *ticket) {
    return (sizeof(ticket->filesize) + sizeof(ticket->localityId) + sizeof(ticket->retrievalType)
			+ sizeof(ticket->port) + strlen(ticket->basePath) 
			+ strlen(ticket->hostName)+ 2);
}

static void * serializeTicket (struct MessageInspectionTicket *ticket) {
	struct BinaryBuffer bb;
	bb.iOffset = 0;
	bb.iFlags = 0x00000000;
	bb.iLength = ticketSize(ticket);
	
    if ((bb.pBuffer = malloc(bb.iLength)) == NULL) {
		rzb_log(LOG_ERR, "%s: Malloctile Dysfunction", __func__);
		return NULL;
	}

    if (BinaryBuffer_Put_uint64_t (&bb, ticket->filesize) == false) {
        rzb_log(LOG_ERR, "%s: Could not serialize file size", __func__);
        free(bb.pBuffer);
        return NULL;
    }
    
	if (BinaryBuffer_Put_uint32_t (&bb, ticket->localityId) == false) {
		rzb_log(LOG_ERR, "%s: Could not serialize localityId", __func__);
		free(bb.pBuffer);
		return NULL;
	}

	if (BinaryBuffer_Put_uint32_t (&bb, ticket->retrievalType) == false) {
        rzb_log(LOG_ERR, "%s: Could not serialize retrievalType", __func__);
		free(bb.pBuffer);
		return NULL;
	}

	if (BinaryBuffer_Put_uint16_t (&bb, ticket->port) == false) {
		rzb_log(LOG_ERR, "%s: Could not serialize port", __func__);
		free(bb.pBuffer);
		return NULL;
	}

	if (BinaryBuffer_Put_String (&bb, (uint8_t *)ticket->hostName) == false) {
		rzb_log(LOG_ERR, "%s: Could not serialize hostName", __func__);
		free(bb.pBuffer);
		return NULL;
	}

	if (BinaryBuffer_Put_String (&bb, (uint8_t *)ticket->basePath) == false) {
		rzb_log(LOG_ERR, "%s: Could not serialize basePath", __func__);
		free(bb.pBuffer);
		return NULL;
	}

    return bb.pBuffer;
}

static struct MessageInspectionTicket * deserializeTicket (void *data, uint32_t size) {
    
	struct MessageInspectionTicket *ticket;
	struct BinaryBuffer bb;
	bb.iOffset = 0;
	bb.iFlags = 0x00000000;
	bb.iLength = size;
	bb.pBuffer = data;

	if ((ticket = (struct MessageInspectionTicket *)malloc(sizeof(struct MessageInspectionTicket))) == NULL) {
        rzb_log(LOG_ERR, "%s: Malloctile Dysfunction", __func__);
		return NULL;
	}

    if (BinaryBuffer_Get_uint64_t (&bb, &ticket->filesize) == false) {
	    rzb_log(LOG_ERR, "%s: Could not deserialize file size", __func__);
	    free(ticket);
	    return NULL;
	}

    if (BinaryBuffer_Get_uint32_t (&bb, &ticket->localityId) == false) {
        rzb_log(LOG_ERR, "%s: Could not deserialize localityId", __func__);
        free(ticket);
        return NULL;
    }

    if (BinaryBuffer_Get_uint32_t (&bb, &ticket->retrievalType) == false) {
        rzb_log(LOG_ERR, "%s: Could not deserialize retrievalType", __func__);
        free(ticket);
        return NULL;
    }
    
    if (BinaryBuffer_Get_uint16_t (&bb, &ticket->port) == false) {
        rzb_log(LOG_ERR, "%s: Could not deserialize port", __func__);
        free(ticket);
        return NULL;
    }
    
    if ((ticket->hostName = (char *)BinaryBuffer_Get_String (&bb)) == NULL) {
        rzb_log(LOG_ERR, "%s: Could not deserialize hostName", __func__);
        free(ticket);
        return NULL;
    }
    
    if ((ticket->basePath = (char *)BinaryBuffer_Get_String (&bb)) == NULL) {
        rzb_log(LOG_ERR, "%s: Could not serialize basePath", __func__);
        free(ticket);
        return NULL;
    }

	return ticket;
}

static uint32_t StoreDataTicket (const char *filename, void *data, uint64_t size) {

    int fd;	
	char *ticketfilename;
    
	if (asprintf(&ticketfilename, "%s.ticket", filename) == -1) {
        rzb_log(LOG_ERR, "StoreDataAsFile: Could not allocate ticket filename string");
        free(ticketfilename);
        return 0;
    }

    fd = open(ticketfilename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        rzb_perror("StoreDataAsFile: Could not open file for writing: %s");
        free(ticketfilename);
        return 0;
    }
  
    if ((writeWrap(fd, data, size)) == 0) {
        rzb_log(LOG_ERR, "StoreDataAsFile: Write failed.");
        free(ticketfilename);
        close(fd);
        return 0;
    }

    close(fd);
    free(ticketfilename);

	return 1;
}

struct MessageInspectionTicket * RetrieveDataTicket (const char *filename) {

    int fd;
    struct stat fs;
    char *ticketfilename;
    void *data;
	struct MessageInspectionTicket *ticket;

    if (asprintf(&ticketfilename, "%s.ticket", filename) == -1) {
        rzb_log(LOG_ERR, "%s: Could not allocate ticket filename string", __func__);
        free(ticketfilename);
        return 0;
    }

    fd = open(filename, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        rzb_perror("RetrieveDataTicket: Could not open file for writing: %s");
        free(ticketfilename);
        return 0;
    }

    if (fstat(fd, &fs) == -1) {
        rzb_perror("RetrieveDataTicket: Could not stat file: %s");
        free(ticketfilename);
        close(fd);
        return 0;
    }
 
    if ((data = malloc(fs.st_size)) == NULL) {
		rzb_log(LOG_ERR, "%s: Malloctile Dysfunction.", __func__);
		free(ticketfilename);
		close(fd);
    }
   
    if ((readWrap(fd, data, fs.st_size)) == 0) {
        rzb_log(LOG_ERR, "%s: Write failed.", __func__);
        free(data);
		free(ticketfilename);
        close(fd);
        return 0;
    }

	if ((ticket = deserializeTicket(data, fs.st_size)) == NULL) {
		rzb_log(LOG_ERR, "%s: Failed to deserialize ticket.", __func__);
		free(data);
		free(ticketfilename);
		close(fd);
	}

	free(data);
	free(ticketfilename);
	close(fd);

	return ticket;

}

static uint32_t StoreDataAsFile (struct Block *block) {
    ASSERT(block != NULL);
	ASSERT(block->pData != NULL);
	
    struct MessageInspectionTicket ticket;
	int fd;
	char *filename;

    ticket.localityId = g_StorageConfValues->localityId;
	ticket.retrievalType = g_StorageConfValues->retrievalType;
	ticket.basePath = g_StorageConfValues->basePath;
    ticket.filesize = block->pId->iLength;

	if ((filename = generateFilename (block, ticket.basePath)) == NULL) {
		rzb_log(LOG_ERR, "%s: failed to generate file name", __func__);
		return 0;
	}

    fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); 
    if (fd == -1) {
		rzb_perror("StoreDataAsFile: Could not open file for writing: %s"); 
		free(filename);
		return 0;
    }

    if ((writeWrap(fd, block->pData, block->pId->iLength)) == 0) {
        rzb_log(LOG_ERR, "%s: Write failed.", __func__);
		free(filename);
		close(fd);
        return 0;
    }

    close(fd);
	
	ticket.hostName = g_StorageConfValues->hostName;
	ticket.port = g_StorageConfValues->port;

    free(block->pData);

	if ((block->pData = serializeTicket(&ticket)) == NULL) {
		rzb_log(LOG_ERR, "%s: Could not serialize ticket", __func__);
		free(filename);
		return 0;
	}

	if (StoreDataTicket(filename, block->pData, ticketSize(&ticket)) == 0) {
		rzb_log(LOG_ERR, "%s: Could not store ticket", __func__);
		free(filename);
		return 0;
	}
	
	free(filename);

	return 1;

}

static uint32_t RetrieveDataAsFile (struct Block *block, struct MessageInspectionTicket *ticket) {
	ASSERT (block != NULL);
    ASSERT (block->pData != NULL);

    int fd;
	char *filename;
	struct stat fs;

    block->pId->iLength = ticket->filesize;

    if ((filename = generateFilename (block, ticket->basePath)) == NULL) {
        rzb_log(LOG_ERR, "%s: failed to generate file name", __func__);
        return 0;
    }

    fd = open(filename, O_RDONLY,  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        rzb_perror("RetrieveDataAsFile: Could not open file for reading: %s");
        free(filename);
        return 0;
    }

    if (fstat(fd, &fs) == -1) {
		rzb_perror("RetrieveDataAsFile: Could not stat file: %s");
		free(filename);
		close(fd);
		return 0;
	}
	
	free (block->pData);
	
	if ((block->pData = malloc(fs.st_size)) == NULL) {
		rzb_log(LOG_ERR, "%s: Malloctile Dysfunction", __func__);
		free(filename);
		close(fd);
		return 0;
	}

    if ((readWrap(fd, block->pData, fs.st_size)) == 0) {
        rzb_log(LOG_ERR, "%s: Write failed.", __func__);
        free(filename);
		close(fd);
        return 0;
    }

	//Do a sanity check on BiD

    free(filename);
    close(fd);
    
	return 1;
	
}

#if 0
static uint32_t StoreOverCurl (struct Block *block) {

    curl_easy_setopt(CURLinstance, CURLOPT_POSTFIELDS, (void *)block->pData);
	curl_easy_setopt(CURLinstance, CURLOPT_POSTFIELDSIZE, (long)block->pId->iLength);

	curl_easy_perform(CURLinstance);

	
	return 1;
}
#endif

SO_PUBLIC uint32_t binaryTicketSize(char *data) {
	uint32_t nonstrlen = sizeof(struct MessageInspectionTicket)-8;
	uint32_t hostNamelen = strlen(data + nonstrlen);
	hostNamelen++;
	uint32_t basePathlen = strlen(data + hostNamelen);
	basePathlen++;


	return (nonstrlen + hostNamelen + basePathlen);
}

static uint32_t RemoveDataAsFile() {

	return 1;
}

size_t WriteCurlData (void *ptr, size_t size, size_t nmemb, void *userdata) {
	
    struct Block *block = (struct Block *)userdata;

    if (g_CurlBytesWritten == 0) {

        free(block->pData);
    
        if ((block->pData = malloc(size*nmemb)) == NULL) {
            rzb_log(LOG_ERR, "%s: Malloctile Dysfunction.", __func__);
            return 0;
        }
    }

    memcpy(block->pData+g_CurlBytesWritten, ptr, size*nmemb);

    g_CurlBytesWritten += size*nmemb;

    if (g_CurlBytesWritten == block->pId->iLength)
        g_CurlBytesWritten = 0;

	return size*nmemb;
}

static uint32_t RetrieveOverCurl (struct Block *block, struct MessageInspectionTicket *ticket) {

    CURLcode code;
    char *filename, *url;

    block->pId->iLength = ticket->filesize;

    if ((filename = generateURL (block)) == NULL) {
        rzb_log(LOG_ERR, "%s: failed to generate file name", __func__);
        return 0;
    }

	if (asprintf(&url, "http://%s:%d/%s", ticket->hostName, ticket->port, filename) == -1) {
        rzb_log(LOG_ERR, "%s: failed to generate url", __func__);
		free(filename);
		return 0;
	}

    if ((code = curl_easy_setopt(CURLinstance, CURLOPT_WRITEDATA, (void *)block)) != 0) {
        rzb_log(LOG_ERR, "%s: Could not set curl write callback arguments", __func__);
        free(url);
        free(filename);
        return 0;
    }

    if ((code = curl_easy_setopt(CURLinstance, CURLOPT_URL, url)) != 0) {
		rzb_log(LOG_ERR, "%s: Could not set curl fetch url", __func__);
		free(url);
		free(filename);
		return 0;
	}

	if ((code = curl_easy_perform(CURLinstance)) != 0) {
		rzb_log(LOG_ERR, "%s: Could not fetch file with curl", __func__);
		free(url);
		free(filename);
		return 0;
	}

    free(url);
	free(filename);

	return 1;

}

SO_PUBLIC uint32_t StoreDataBlock (struct Block *block) {

	StoreDataAsFile (block);

	return 1;
}

SO_PUBLIC uint32_t RetrieveDataBlock (struct Block *block) {

    ASSERT (block != NULL);
    ASSERT (block->pData != NULL);

    struct MessageInspectionTicket *ticket;
    if ((ticket = deserializeTicket(block->pData, block->pId->iLength)) == NULL) {
        rzb_log(LOG_ERR, "%s: Could not deserialize ticket", __func__);
        return 0;
    }
//TODO: localityId needs to be unhardcoded
    if (ticket->localityId == 1 && ticket->localityId != 0) 
		RetrieveDataAsFile (block, ticket);
	else {
		switch (ticket->retrievalType) {
			case 0:
				RetrieveOverCurl(block, ticket);
				break;
			
			default:
				rzb_log(LOG_ERR, "%s: Unhandled type", __func__);
				free(ticket->basePath);
				free(ticket->hostName);		
				free(ticket);
				return 0;
		}

	}

	free(ticket->basePath);
	free(ticket->hostName);
	free(ticket);

	return 1;
}

SO_PUBLIC uint32_t RemoveDataBlock (struct Block *block) {

	//char *filename;
	struct MessageInspectionTicket *ticket;

	if ((ticket = RetrieveDataTicket("PlaceHolder")) == NULL) {
		rzb_log(LOG_ERR, "%s: Could not retrieve stored ticket.", __func__);
		return 0;
	}
    
	switch (ticket->retrievalType) {
		case 0:
			RemoveDataAsFile();
            break;

		default:
			rzb_log(LOG_ERR, "%s: Unhandled type", __func__);
			free(ticket);
			return 0;
	}

	free(ticket);

	return 1;
}


static bool readStorageConf () {

    config_t config;
    config_setting_t *list;
    
	const char *basepath;
    const char *hostname;

    memset (&config, 0, sizeof(config));
    config_init(&config);

    if ((list = getConfigArray(&config, StorageConfFile, "Storage")) == NULL) {
        config_destroy(&config);
        return false;
    }

    config_setting_lookup_int (list, "Locality", (conf_int_t *)&g_StorageConfValues->localityId);
    config_setting_lookup_int (list, "Type", (conf_int_t *)&g_StorageConfValues->retrievalType);
    config_setting_lookup_int (list, "Port", (conf_int_t *)&g_StorageConfValues->port);
	config_setting_lookup_string (list, "Basepath", &basepath);
	config_setting_lookup_string (list, "Hostname", &hostname);

    if ((g_StorageConfValues->basePath = (char *)malloc(strlen(basepath)+1)) == NULL) {
		rzb_log(LOG_ERR, "%s: Malloctile Dysfunction", __func__);
		return false;
	}

    if ((g_StorageConfValues->hostName = (char *)malloc(strlen(hostname)+1)) == NULL) {
        rzb_log(LOG_ERR, "%s: Malloctile Dysfunction", __func__);
        return false;
    }

    strcpy(g_StorageConfValues->basePath, basepath);
    strcpy(g_StorageConfValues->hostName, hostname);

    config_destroy(&config);

    return true;

}

bool Data_Storage_Curl_Initialize() {
   
    if ((CURLinstance = curl_easy_init()) == NULL) {
        rzb_log(LOG_ERR, "%s: Failed to initialize curl", __func__);
        return false;
    }

    if (curl_easy_setopt(CURLinstance, CURLOPT_WRITEFUNCTION, &WriteCurlData) != 0) {
        rzb_log(LOG_ERR, "%s: Could not set curl write callback", __func__);
        return false;
    }
   
    return true;

}

bool Data_Storage_Initialize() {

	if ((g_StorageConfValues = (struct MessageInspectionTicket *)calloc(1, sizeof(*g_StorageConfValues))) == NULL) {
		rzb_log(LOG_ERR, "%s: Malloctile Dysfunction", __func__);
		return false;
	}

	if (readStorageConf () == false) {
		rzb_log(LOG_ERR, "%s: Failed to read configuration", __func__);
		return false;
	}

    if (!Data_Storage_Curl_Initialize()) {
        rzb_log(LOG_ERR, "%s: Failed to initialize curl", __func__);
        return false;
    } 

	return true;
}

#if 0
void StorageTestFunction () {

    struct Block block;
    struct BlockId pid;

    if(!Data_Storage_Initialize())
		printf("Failure\n");

	block.pData = (void *)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	block.pId = &pid;
	pid.iLength = 10;

	RetrieveOverCurl(&block);

}
#endif
