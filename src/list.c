#include "config.h"
#include <razorback/debug.h>
#include <razorback/list.h>
#include <razorback/log.h>

#include <string.h>
#include <razorback/thread.h>

static void List_RemoveNode(struct List *list, struct ListNode *node);

static void List_Stack_Push(struct List *list, struct ListNode *node);
static void List_Queue_Push(struct List *list, struct ListNode *node);

static struct ListNode * List_Stack_Pop(struct List *list);
static struct ListNode * List_Queue_Pop(struct List *list);

SO_PUBLIC struct List *
List_Create(int mode, 
        int (*cmp)(void *, void *), 
        int (*keyCmp)(void *, void *), 
        void (*destroy)(void *),
        void *(*clone)(void *),
        void (*nodeLock)(void *),
        void (*nodeUnlock)(void *))
{
    struct List * list;

    if ((list = calloc(1,sizeof(struct List))) == NULL)
        return NULL;

    if ((list->lock = RWLock_Create() ) == NULL) {
        free(list);
        return NULL;
    }

    if (mode == LIST_MODE_QUEUE || mode == LIST_MODE_STACK)
    {
    	if ((list->sem = Semaphore_Create(true, 0)) == NULL)
    	{
    		free(list);
    		return NULL;
    	}
    }
    else
    	list->sem = NULL;

    list->mode = mode;
    list->cmp = cmp;
    list->keyCmp = keyCmp;
    list->destroy = destroy;
    list->clone = clone;
    list->nodeLock = nodeLock;
    list->nodeUnlock = nodeUnlock;
    return list;
}

struct FindData {
    struct List *list;
    void * ret;
    void * id;
};
static int List_FindKeyCmp(void * item, void *d) {
    struct FindData *data = (struct FindData *)d;
    if (data->list->keyCmp(item, data->id) == 0)
    {
        data->ret = item;
        return LIST_EACH_END;
    }
    return LIST_EACH_OK;
}

SO_PUBLIC void * 
List_Find(struct List *list, void *id)
{
    struct FindData data;
    ASSERT(list != NULL);
    ASSERT(id != NULL);
    ASSERT(list->keyCmp != NULL);


    if ((list == NULL) || (id == NULL) || (list->keyCmp == NULL))
        return NULL;

    data.list = list;
    data.id = id;
    data.ret = NULL;

    if (!List_ForEach(list, List_FindKeyCmp, &data)) {
        rzb_log(LOG_ERR, "%s: Error calling List_ForEach", __func__);
    }

    return data.ret;
}

SO_PUBLIC bool 
List_Push(struct List *list, void *item)
{
    struct ListNode *node;
    ASSERT(list != NULL);
    if (list == NULL)
        return false;
    if ((node = calloc(1, sizeof(struct ListNode))) == NULL)
        return false;

    node->item = item;


    RWLock_WriteLock(list->lock);
    switch (list->mode)
    {
    case LIST_MODE_GENERIC:
    case LIST_MODE_QUEUE:
        List_Queue_Push(list, node);
        break;
    case LIST_MODE_STACK:
        List_Stack_Push(list, node);
        break;
    }
    list->length++;
    RWLock_Unlock(list->lock);


    if (list->sem != NULL)
		Semaphore_Post(list->sem);
    return true;
}

SO_PUBLIC void *
List_Pop(struct List *list)
{
    struct ListNode *node = NULL;
    void * ret = NULL;
    ASSERT(list != NULL);
    if (list == NULL)
        return NULL;

    if (list->sem != NULL)
    	Semaphore_Wait(list->sem);

    RWLock_WriteLock(list->lock);
    switch (list->mode)
    {
    case LIST_MODE_GENERIC:
    case LIST_MODE_QUEUE:
        node = List_Queue_Pop(list);
        break;
    case LIST_MODE_STACK:
        node = List_Stack_Pop(list);
        break;
    }
    RWLock_Unlock(list->lock);

    if (node != NULL) {
        ret = node->item;
        free(node);
    }

    return ret;
}


SO_PUBLIC bool
List_ForEach(struct List *list, int (*op)(void *, void *), void *userData)
{
    struct ListNode *cur = NULL, *delNode;
    bool del = false;
    bool ret = true;
    bool last = false;
    int opRet;
    ASSERT(list != NULL);
    ASSERT(op != NULL);

    if (list == NULL)
        return false;
    if (op == NULL)
        return false;

    // List is empty we did what was asked successfully
    if (list->head == NULL) {
        return true;
    }
    RWLock_ReadLock(list->lock);
    cur = list->head;

    while (cur != NULL)
    {
        last = false;
        del = false;
        if (list->nodeLock)
            list->nodeLock(cur->item);

        opRet = op(cur->item, userData);
        if (list->nodeUnlock)
            list->nodeUnlock(cur->item);
        switch (opRet)
        {
            case LIST_EACH_ERROR:
                ret = false;
                __attribute__ ((fallthrough));
            case LIST_EACH_END:
                last = true;
                break;
            case LIST_EACH_REMOVE:
                rzb_log(LOG_DEBUG, "%s: Delete requested marking node for deletion: %p", __func__, cur);
                del = true;
                cur->del = true;
                break;
            default:
                break;
        }
        cur = cur->next;
        if (last)
            break;
    }
    RWLock_Unlock(list->lock);

    if (del) {
        rzb_log(LOG_DEBUG, "%s: Item deletion requested in loop pruning list", __func__);
        RWLock_WriteLock(list->lock);
        cur = list->head;
        while (cur != NULL) {
            if (cur->del) {
                delNode = cur;
                cur = delNode->next;
                rzb_log(LOG_ERR, "%s: Removing deleted node from list %p", __func__, delNode);
                List_RemoveNode(list, delNode);
                if (list->destroy)
                    list->destroy(delNode->item);
                free(delNode);
            } else {
                cur = cur->next;
            }
        }
        RWLock_Unlock(list->lock);
    }
    return ret;
}


static int List_FindRemove(void * curItem, void *d) {
    struct FindData *data = (struct FindData *)d;
    if ( (curItem == data->id) ||
         (
             (data->list->cmp != NULL) &&
             (data->list->cmp(curItem, data->id)== 0)
         ))
    {
        data->ret = curItem;
        return LIST_EACH_REMOVE;
    }
    return LIST_EACH_OK;
}

SO_PUBLIC void 
List_Remove(struct List *list, void *item)
{
    struct FindData data;
    ASSERT(list != NULL);
    ASSERT(item != NULL);
    if (list == NULL)
        return;
    if (item == NULL)
        return;
    data.id = item;
    data.list = list;
    data.ret = NULL;
    if (!List_ForEach(list, List_FindRemove, &data)) {
        rzb_log(LOG_ERR, "%s: Error calling List_ForEach", __func__);
    }
    if(data.ret == NULL) {
        rzb_log(LOG_ERR, "%s: Failed remove item", __func__);
    }

    return;
}

static void
List_RemoveNode(struct List *list, struct ListNode *node)
{
    ASSERT(list != NULL);
    ASSERT(node != NULL);
    ASSERT(list->head != NULL);
    if (list == NULL)
        return;
    if (node == NULL)
        return;
    if (list->head == NULL)
        return;

    if (node == list->head && node == list->tail)
    {
        list->head = NULL;
        list->tail = NULL;
    }
    else if (node == list->head)
    {
        list->head = node->next;
        list->head->prev = NULL;
    }
    else if (node == list->tail)
    {
        list->tail = node->prev;
        list->tail->next = NULL;
    }
    else
    {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    list->length--;
}

SO_PUBLIC void
List_Clear(struct List *list)
{
    void *item;
    ASSERT(list != NULL);
    if (list == NULL)
        return;

    item = List_Pop(list);
    while ( item != NULL )
    {
        if (list->destroy != NULL) {
            list->destroy(item);
        }
        item = List_Pop(list);
    }
}

SO_PUBLIC size_t
List_Length(struct List *list)
{
    ASSERT(list != NULL);
    if (list == NULL)
        return 0;

    return list->length;
}

// TODO: Depricated
SO_PUBLIC void
List_Lock(struct List *list)
{
    ASSERT(list != NULL);
    if (list == NULL)
        return;

//    Mutex_Lock(list->lock);
}

// TODO: Depricated
SO_PUBLIC void
List_Unlock(struct List *list)
{
    ASSERT(list != NULL);
    if (list == NULL)
        return;

//    Mutex_Unlock(list->lock);
}

SO_PUBLIC void
List_Destroy(struct List *list)
{
    ASSERT(list != NULL);
    if (list == NULL)
        return;
    
    List_Clear(list);
    free(list);
}

static int
List_Clone_Node(void *vItem, void *vDest)
{
    struct List *dest = (struct List*)vDest;
    void *new = dest->clone(vItem);
    if (new == NULL)
        return LIST_EACH_ERROR;
    if (List_Push(dest, new))
        return LIST_EACH_OK;
    else
        return LIST_EACH_ERROR;
}

SO_PUBLIC struct List*
List_Clone (struct List *source)
{
    struct List *dest;
    ASSERT(source != NULL);
    ASSERT(source->clone != NULL);
    if (source == NULL)
        return NULL;
    if (source->clone == NULL)
        return NULL;
    
    dest = List_Create(source->mode, 
            source->cmp, 
            source->keyCmp, 
            source->destroy, 
            source->clone,
            source->nodeLock,
            source->nodeUnlock);

    if(dest == NULL)
        return NULL;

    if (!List_ForEach(source, List_Clone_Node, dest))
    {
        return NULL;
    }
    return dest;
}



static void 
List_Stack_Push(struct List *list, struct ListNode *node)
{
    ASSERT(list != NULL);
    ASSERT(node != NULL);
    if (list == NULL)
        return;

    if (list->head == NULL)
    {
        list->head = node;
        list->tail = node;
    }
    else
    {
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
}
static void 
List_Queue_Push(struct List *list, struct ListNode *node)
{
    ASSERT(list != NULL);
    ASSERT(node != NULL);
    if (list == NULL || node == NULL)
        return;

    if (list->tail == NULL)
    {
        list->head = node;
        list->tail = node;
    }
    else
    {
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }
}

static struct ListNode * 
List_Stack_Pop(struct List *list)
{
    struct ListNode *ret;
    ASSERT(list != NULL);
    if (list == NULL)
        return NULL;

    if (list->head == NULL)
        return NULL;


    ret = list->head;
    List_RemoveNode(list, ret);

    return ret;
}

static struct 
ListNode * List_Queue_Pop(struct List *list)
{
    struct ListNode *ret;
    ASSERT(list != NULL);
    if (list == NULL)
        return NULL;

    if (list->tail == NULL)
        return NULL;

    ret = list->tail;
    List_RemoveNode(list, ret);

    return ret;
}

