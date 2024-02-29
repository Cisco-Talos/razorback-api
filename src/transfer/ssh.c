#include "config.h"
#include <razorback/debug.h>
#include <razorback/types.h>
#include <razorback/list.h>
#include <razorback/log.h>
#include <razorback/thread.h>

#include <libssh/libssh.h>
#include <libssh/sftp.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef _MSC_VER
#include <io.h>
#include <stdio.h>
#include "bobins.h"
#define CREATE_MODE 755
#else //_MSC_VER
#include <sys/mman.h>
#define CREATE_MODE (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
#endif //_MSC_VER

#include <fcntl.h>
#include <errno.h>

#include "transfer/core.h"
#include "runtime_config.h"

#define KEYS_FOLDER ETC_DIR "/"
#define KNOWN_DISPATCHERS ETC_DIR "/known_dispatchers"

static struct List *sessionList = NULL;

static bool SSH_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher);
static bool SSH_Fetch(struct Block *block, struct ConnectedEntity *dispatcher);
static void SSH_Free(struct Block *block);
static int SSH_Session_KeyCmp(void *a, void *id);
static int SSH_Session_Cmp(void *a, void *b);
static struct SSH_Session * SSH_Get_Session(uuid_t nuggetId, struct ConnectedEntity *dispatcher);
static bool SSH_Check_Session(struct SSH_Session *session);
static bool SSH_Verify_Dispatcher(ssh_session session);

static struct TransportDescriptor descriptor =
{
    1,
    "SSH",
    "Transfer file via SSH (sftp)",
    SSH_Store,
    SSH_Fetch,
    SSH_Free
};

struct SSH_Session_Key
{
    uuid_t nuggetId;
    uuid_t dispatcherId;
	rzb_thread_t threadId;
};
struct SSH_Session
{
    struct SSH_Session_Key key;
    struct ConnectedEntity *dispatcher;
    ssh_session ssh;
    sftp_session sftp;
    char *hostname;
};

bool 
SSH_Init(void)
{
    sessionList = List_Create(LIST_MODE_GENERIC, 
            SSH_Session_Cmp, //Cmp
            SSH_Session_KeyCmp, //KeyCmp
            NULL, //Destroy
            NULL, //Clone,
            NULL, //Lock,
            NULL); //Unlock
    if (sessionList == NULL)
        return false;
    return Transport_Register(&descriptor);
}

static char *
createDirectory(struct Block *block, struct SSH_Session *session)
{
    char *hash;
    char *dir, *cdir;
    sftp_dir sdir;
    cdir = sftp_canonicalize_path(session->sftp, ".");
    hash = Transfer_generateFilename(block);
    if (asprintf (&dir, "%s/%c", cdir, hash[0]) == -1)
    {
        rzb_log (LOG_ERR, "%s: Could not allocate directory string",
                 __func__);
        free (hash);
        return false;
    }
    if ((sdir = sftp_opendir(session->sftp, dir)) == NULL)
    {
        if (sftp_mkdir (session->sftp, dir, CREATE_MODE) == -1)
        {
            rzb_log (LOG_ERR, "%s: Error creating directory %s", __func__,
                     dir);
            free (hash);
            free (dir);
            free (cdir);
            return false;
        }
    }
    else
        sftp_closedir(sdir);

    free (dir);

    if (asprintf (&dir, "%s/%c/%c", cdir, hash[0], hash[1]) == -1)
    {
        rzb_log (LOG_ERR, "%s: Could not allocate directory string",
                 __func__);
        free (hash);
        free (dir);
        return false;
    }
    if ((sdir = sftp_opendir(session->sftp, dir)) == NULL)
    {
        if (sftp_mkdir (session->sftp, dir, CREATE_MODE) == -1)
        {
            rzb_log (LOG_ERR, "%s: Error creating directory %s", __func__,
                     dir);
            free (hash);
            free (dir);
            free(cdir);
            return false;
        }
    }
    else
        sftp_closedir(sdir);

    free(dir);

    if (asprintf (&dir, "%s/%c/%c/%c", cdir, hash[0], hash[1], hash[2])
        == -1)
    {
        rzb_log (LOG_ERR, "%s: Could not allocate directory string",
                 __func__);
        free (hash);
        free (dir);
        free(cdir);
        return false;
    }
    if ((sdir = sftp_opendir(session->sftp, dir)) == NULL)
    {
        if (sftp_mkdir (session->sftp, dir, CREATE_MODE) == -1)
        {
            rzb_log (LOG_ERR, "%s: Error creating directory %s", __func__,
                     dir);
            free (hash);
            free (dir);
            free(cdir);
            return false;
        }
    }
    else
        sftp_closedir(sdir);

    free(dir);

    if (asprintf
        (&dir, "%s/%c/%c/%c/%c", cdir, hash[0], hash[1], hash[2],
         hash[3]) == -1)
    {
        rzb_log (LOG_ERR, "%s: Could not allocate directory string",
                 __func__);
        free (hash);
        free (dir);
        free(cdir);
        return false;
    }
    if ((sdir = sftp_opendir(session->sftp, dir)) == NULL)
    {
        if (sftp_mkdir (session->sftp, dir, CREATE_MODE) == -1)
        {
            rzb_log (LOG_ERR, "%s: Error creating directory %s", __func__,
                     dir);
            free (hash);
            free (dir);
            free(cdir);
            return false;
        }
    }
    else
        sftp_closedir(sdir);

    free(cdir);
    free(hash);
    return dir;
}

static uint32_t
writeWrap (sftp_file fd, uint8_t * data, uint64_t length)
{

    ssize_t size;
    ssize_t totalbytes = 0;
    ssize_t bytessofar;

    size = length;

    while (totalbytes < size)
    {
        bytessofar = sftp_write (fd, data + totalbytes, size - totalbytes);
        if (bytessofar < 0)
        {
            rzb_log (LOG_ERR, "%s: Could not write data to file", __func__);
            return 0;
        }
        totalbytes += bytessofar;
    }

    return 1;
}

static bool 
SSH_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher)
{
    struct RazorbackContext *ctx;
    struct SSH_Session *session;
    char *filename =NULL;
    char *path = NULL;
    char *fullpath = NULL;
    sftp_file fd;
    struct BlockPoolData *dataItem = NULL;

	ASSERT (item != NULL);

    ctx = Thread_GetContext(Thread_GetCurrent());
    if (ctx == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup thread context", __func__);
        return false;
    }
    session = SSH_Get_Session(ctx->uuidNuggetId, dispatcher);
    if (session == NULL) 
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup context protocol session", __func__);
        return false;
    }
    if (!SSH_Check_Session(session))
        return false;

    
    if ((filename = Transfer_generateFilename (item->pEvent->pBlock)) == NULL)
    {
        rzb_log (LOG_ERR, "%s: failed to generate file name", __func__);
        return false;
    }
    if (( path = createDirectory(item->pEvent->pBlock, session)) == NULL)
    {
        rzb_log (LOG_ERR, "%s: failed to create storage dir", __func__);
        free (filename);
        return false;
    }
    if (asprintf(&fullpath, "%s/%s", path, filename) == -1)
    {
        rzb_log (LOG_ERR, "%s: failed to generate file path", __func__);
        free(path);
        free (filename);
        return false;
    }
    // Check if its there already.
    //
    fd = sftp_open (session->sftp, fullpath, O_RDONLY,0);
    if (fd != NULL)
    {
        sftp_close(fd);
        free(fullpath);
        free(path);
        free(filename);
        return true;
    }
    
    fd = sftp_open (session->sftp, fullpath, O_RDWR | O_CREAT | O_TRUNC,
               S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == NULL)
    {
        rzb_log (LOG_ERR, "%s: Could not open file for writing: %s", __func__, ssh_get_error(session->ssh));
        free (filename);
        return false;
    }
    dataItem = item->pDataHead;
    while (dataItem != NULL)
    {
        if ((writeWrap (fd, dataItem->pData, dataItem->iLength)) == 0)
        {
            rzb_log (LOG_ERR, "%s: Write failed.", __func__);
            sftp_close (fd);
            free(fullpath);
            free(path);
            free (filename);
            return false;
        }
        dataItem = dataItem->pNext;
    }

    sftp_close (fd);

    free (filename);
    free (path);
    free(fullpath);

    return true;
}

static bool 
SSH_Fetch(struct Block *block, struct ConnectedEntity *dispatcher)
{
    struct RazorbackContext *ctx;
    struct SSH_Session *session;
    sftp_file fd;
    char *filename = NULL;
    char *path = NULL;
    char *fullpath = NULL;
    char tmp_string[L_tmpnam];  // Temporary string to use for path to tmpfile
    FILE *out_file;             // Output stream to create a temporary file on tmpfs
	ssize_t read =0;
    ssize_t got =0;
    char buf[1024];
	
	ASSERT (block != NULL);

    ctx = Thread_GetContext(Thread_GetCurrent());
    if (ctx == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup thread context", __func__);
        return false;
    }
    session = SSH_Get_Session(ctx->uuidNuggetId, dispatcher);
    if (session == NULL) {
        rzb_log(LOG_ERR, "%s: Failed to lookup context protocol session", __func__);
        return false;
    }
    if (!SSH_Check_Session(session))
        return false;

    if ((filename = Transfer_generateFilename (block)) == NULL)
    {
        rzb_log (LOG_ERR, "%s: failed to generate file name", __func__);
        return false;
    }
    path = sftp_canonicalize_path(session->sftp, ".");
    if (asprintf(&fullpath, "%s/%c/%c/%c/%c/%s", path,
                filename[0], filename[1], filename[2], filename[3], filename) == -1)
    {
        rzb_log (LOG_ERR, "%s: failed to generate file path", __func__);
        free(path);
        free(filename);
        return false;
    }

    fd = sftp_open (session->sftp, fullpath, O_RDONLY, 0);

    free (filename); filename = NULL;
    free (path); path = NULL;
    free (fullpath); fullpath = NULL;

    if (fd == NULL)
    {
        rzb_log(LOG_ERR, "%s: Could not open file for reading: %s", __func__, ssh_get_error(session->ssh));
        return false;
    }
    
    tmp_string[0] = 0;
    if (tmpnam (tmp_string) == NULL)
    {
        rzb_perror("Cannot create temporary file name: %s");
        return false;
    }
    // Create tmpfile
    if ((out_file = fopen (tmp_string, "w")) == NULL)
    {
        rzb_perror("Cannot create temporary file: %s");
        return false;
    }
    
    while ((uint64_t)read < block->pId->iLength)
    {
        got = sftp_read(fd, buf, 1024);
        if (got < 0)
        {
            rzb_log(LOG_ERR, "%s: Failed to read: %s", __func__, ssh_get_error(session->ssh));
            sftp_close(fd);
            return false;
        }
        if (got == 0)
            break;
        fwrite(buf, 1, got, out_file); 
        read += got;
    }
    if ((uint64_t)read != block->pId->iLength)
    {
        rzb_log(LOG_ERR, "%s: Failed to read full data block", __func__);
        sftp_close (fd);
        fclose(out_file);
        remove(tmp_string);
        return false;
    }
    sftp_close (fd);
    fclose(out_file);
    out_file=fopen(tmp_string, "r");
#ifdef _MSC_VER
	UNIMPLEMENTED();
#else //_MSC_VER
    block->pData = mmap (NULL, block->pId->iLength, PROT_READ, MAP_PRIVATE, fileno(out_file), 0);
    if (block->pData == MAP_FAILED)
    {
        rzb_perror("%s");
        block->pData = NULL;
		fclose(out_file);
        return false;
    }
#endif //_MSC_VER
	fclose(out_file);
    remove(tmp_string);
    //Do a sanity check on BiD
    return true;

}

static void 
SSH_Free(struct Block *block)
{
    if (block->pData == NULL)
        return;
#ifdef _MSC_VER
	UNIMPLEMENTED();
#else //_MSC_VER
    munmap(block->pData, block->pId->iLength);
#endif
}

static int SSH_Session_KeyCmp(void *a, void *id)
{
    struct SSH_Session *ses = a;
    struct SSH_Session_Key *key = id;
    if ((uuid_compare(ses->key.nuggetId, key->nuggetId) == 0) &&
            (uuid_compare(ses->key.dispatcherId, key->dispatcherId) == 0) &&
			(ses->key.threadId == key->threadId))
        return 0;
    return -1;
}

static int SSH_Session_Cmp(void *a, void *b)
{
    struct SSH_Session *sesA = a;
    struct SSH_Session *sesB = b;
    if (a==b)
        return 0;

    if ((uuid_compare(sesA->key.nuggetId, sesB->key.nuggetId) == 0) &&
            (uuid_compare(sesA->key.dispatcherId, sesB->key.dispatcherId) == 0) &&
			(sesA->key.threadId == sesB->key.threadId))
        return 0;
    return -1;
}

static struct SSH_Session * 
SSH_Get_Session(uuid_t nuggetId, struct ConnectedEntity *dispatcher)
{
    struct SSH_Session *session;
    struct SSH_Session_Key key;
    uuid_copy(key.nuggetId, nuggetId);
    uuid_copy(key.dispatcherId, dispatcher->uuidNuggetId);
	key.threadId = Thread_GetCurrentId();

    session = List_Find(sessionList, &key);
    if (session != NULL)
        return session;
    
    if ((session = calloc(1,sizeof(struct SSH_Session))) == NULL)
        return NULL;

    uuid_copy(session->key.nuggetId, nuggetId);
    uuid_copy(session->key.dispatcherId, dispatcher->uuidNuggetId);
	session->key.threadId = key.threadId;
    session->dispatcher=dispatcher;
    //session.context
    List_Push(sessionList, session);
    return session;
}

static bool
SSH_Verify_Dispatcher(ssh_session session)
{
    int state, hlen;
    unsigned char *hash = NULL;
    char *hexa;

    state = ssh_is_server_known(session);

    hlen = ssh_get_pubkey_hash(session, &hash);
    if (hlen < 0)
        return false;

    switch (state)
    {
    case SSH_SERVER_KNOWN_OK:
        break; /* ok */

    case SSH_SERVER_KNOWN_CHANGED:
        hexa = ssh_get_hexa(hash, hlen);
        rzb_log(LOG_ERR, "%s: Host key for server changed. For security reasons, connection will be stopped. New key: %s", __func__, hexa);
        free(hexa);
        free(hash);
        return false;

    case SSH_SERVER_FOUND_OTHER:
        rzb_log(LOG_ERR, "%s: The host key for this server was not found but an other"
            "type of key exists. An attacker might change the default server key to"
            "confuse your client into thinking the key does not exist", __func__);
        free(hash);
        return false;

    case SSH_SERVER_FILE_NOT_FOUND:
        rzb_log(LOG_ERR, "%s: Could not find known host file, it will be automatically created.", __func__);
        /* fallback to SSH_SERVER_NOT_KNOWN behavior */

    case SSH_SERVER_NOT_KNOWN:
        hexa = ssh_get_hexa(hash, hlen);
        rzb_log(LOG_ERR,"%s The server is unknown. Adding the key: %s", __func__, hexa);
        free(hexa);
        if (ssh_write_knownhost(session) < 0)
        {
            rzb_log(LOG_ERR, "%s: %s", __func__, strerror(errno));
            free(hash);
            return false;
        }
        break;

    case SSH_SERVER_ERROR:
        fprintf(stderr, "Error %s", ssh_get_error(session));
        free(hash);
        return false;
    }

    free(hash);
    return true;
}

static bool 
SSH_Check_Session(struct SSH_Session *session)
{
    char user[UUID_STRING_LENGTH];
    struct ListNode * cur = NULL;
    bool connected = false;
    ASSERT(session != NULL);
    if (session == NULL)
        return false;

    // New connection
    if (session->ssh == NULL)
    {
        if ((session->ssh = ssh_new()) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to allocate ssh session", __func__);
            return false;
        }
        uuid_unparse(session->key.nuggetId, user);
        ssh_options_set(session->ssh, SSH_OPTIONS_PORT, &session->dispatcher->dispatcher->port);
        ssh_options_set(session->ssh, SSH_OPTIONS_USER, user);
        ssh_options_set(session->ssh, SSH_OPTIONS_KNOWNHOSTS,KNOWN_DISPATCHERS);
        connected = false;
        for (cur = session->dispatcher->dispatcher->addressList->head; cur != NULL && (!connected); cur = cur->next)
        {
            ssh_options_set(session->ssh, SSH_OPTIONS_HOST, cur->item);
            if (ssh_connect(session->ssh) != SSH_OK)
            {
                rzb_log(LOG_ERR, "%s: Failed to connect session (%s)", __func__, cur->item);
            }
            else
                connected = true;
        }
        if (!connected)
        {
            rzb_log(LOG_ERR, "%s: Failed to connected to dispatcher", __func__);
            ssh_disconnect(session->ssh);
            ssh_free(session->ssh);
            session->ssh = NULL;
            return false;
        }

        if (!SSH_Verify_Dispatcher(session->ssh))
        {
            rzb_log(LOG_ERR, "%s: Failed to verify dispatcher", __func__);
            ssh_disconnect(session->ssh);
            ssh_free(session->ssh);
            session->ssh = NULL;
            return false;
        }
        if (ssh_userauth_password(session->ssh, NULL, Razorback_Get_Transfer_Password()) != SSH_AUTH_SUCCESS)
        {
            rzb_log(LOG_ERR, "%s: Failed to authenticate: %s", __func__, ssh_get_error(session->ssh));
            ssh_disconnect(session->ssh);
            ssh_free(session->ssh);
            session->ssh = NULL;
            return false;
        }
        if ((session->sftp = sftp_new(session->ssh)) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to create sftp session: %s", __func__, ssh_get_error(session->ssh));
            ssh_disconnect(session->ssh);
            ssh_free(session->ssh);
            session->ssh = NULL;
            return false;
        }
        if (sftp_init(session->sftp) != 0)
        {
            rzb_log(LOG_ERR, "%s: Failed to init sftp session: %s", __func__, ssh_get_error(session->ssh));
            sftp_free(session->sftp);
            ssh_disconnect(session->ssh);
            ssh_free(session->ssh);
            
            session->sftp = NULL;
            session->ssh = NULL;
            return false;
        }
    }
    return true;
}

