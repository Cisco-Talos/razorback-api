#include "config.h"
#include <razorback/debug.h>
#include <razorback/list.h>

#include <pthread.h>
#include <string.h>

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
        void *(*clone)(void *))
{
    pthread_mutexattr_t mutexAttr;
    struct List * list;

    ASSERT(cmp != NULL);
    ASSERT(keyCmp != NULL);
    
    if ((list = calloc(1,sizeof(struct List))) == NULL)
        return NULL;
    memset(&mutexAttr, 0, sizeof(pthread_mutexattr_t));
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&list->lock, &mutexAttr); 

    list->mode = mode;
    list->cmp = cmp;
    list->keyCmp = keyCmp;
    list->destroy = destroy;
    list->clone = clone;
    return list;
}

SO_PUBLIC void * 
List_Find(struct List *list, void *id)
{
    struct ListNode *cur;
    ASSERT(list != NULL);
    ASSERT(id != NULL);
    if ((list == NULL) || (id == NULL))
        return NULL;

    pthread_mutex_lock(&list->lock);
    for (cur = list->head; cur != NULL; cur = cur->next)
    {
        if (list->keyCmp(cur->item, id) == 0)
        {
            pthread_mutex_unlock(&list->lock);
            return cur->item;
        }
    }
    pthread_mutex_unlock(&list->lock);
    return NULL;
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
    pthread_mutex_lock(&list->lock);
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
    pthread_mutex_unlock(&list->lock);
    return true;
}

SO_PUBLIC void *
List_Pop(struct List *list)
{
    struct ListNode *node = NULL;
    void * ret;
    ASSERT(list != NULL);
    if (list == NULL)
        return NULL;
    
    pthread_mutex_lock(&list->lock);
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
    pthread_mutex_unlock(&list->lock);
    if (node == NULL)
        return NULL;

    ret = node->item;
    free(node);
    return ret;
}


SO_PUBLIC bool
List_ForEach(struct List *list, int (*op)(void *, void *), void *userData)
{
    struct ListNode *cur;

    ASSERT(list != NULL);
    ASSERT(op != NULL);

    if (list == NULL)
        return false;
    if (op == NULL)
        return false;

    pthread_mutex_lock(&list->lock);

    for (cur = list->head; cur != NULL; cur = cur->next)
    {
        switch (op(cur->item, userData))
        {
        case LIST_EACH_OK:
            break;
        case LIST_EACH_ERROR:
            pthread_mutex_unlock(&list->lock);
            return false;
        case LIST_EACH_REMOVE:
            if (list->destroy != NULL)
                list->destroy(cur->item);

            List_RemoveNode(list, cur);
            break;
        default:
            return false;
        }
    }
    pthread_mutex_unlock(&list->lock);
    return true;
}

SO_PUBLIC void 
List_Remove(struct List *list, void *item)
{
    struct ListNode *cur;
    ASSERT(list != NULL);
    ASSERT(item != NULL);
    if (list == NULL)
        return;
    if (item == NULL)
        return;
    pthread_mutex_lock(&list->lock);

    for (cur = list->head; cur != NULL; cur = cur->next)
    {
        if ( (cur->item == item) ||
                (list->cmp(item, cur->item) == 0))
        {
            List_RemoveNode(list, cur);
            pthread_mutex_unlock(&list->lock);
            return;
        }
    }
    pthread_mutex_unlock(&list->lock);
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
    }
    list->length--;
}

SO_PUBLIC void
List_Clear(struct List *list)
{
    struct ListNode *cur, *next;
    ASSERT(list != NULL);
    if (list == NULL)
        return;

    pthread_mutex_lock(&list->lock);
    next = NULL;
    cur = list->head; 
    while ( cur != NULL )
    {
        if (list->destroy != NULL)
            list->destroy(cur->item);

        List_RemoveNode(list, cur);
        next  =cur->next;
        free(cur);
        cur = next;
    }
    pthread_mutex_unlock(&list->lock);

}

SO_PUBLIC size_t
List_Length(struct List *list)
{
    size_t ret;
    ASSERT(list != NULL);
    if (list == NULL)
        return 0;

    pthread_mutex_lock(&list->lock);
    ret = list->length;
    pthread_mutex_unlock(&list->lock);
    return ret;
}

SO_PUBLIC void
List_Lock(struct List *list)
{
    ASSERT(list != NULL);
    if (list == NULL)
        return;

    pthread_mutex_lock(&list->lock);
}

SO_PUBLIC void
List_Unlock(struct List *list)
{
    ASSERT(list != NULL);
    if (list == NULL)
        return;

    pthread_mutex_unlock(&list->lock);
}

SO_PUBLIC void
List_Destroy(struct List *list)
{
    ASSERT(list != NULL);
    if (list == NULL)
        return;
    
    List_Clear(list);
    pthread_mutex_destroy(&list->lock);
    free(list);
}

int
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
            source->clone);

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

