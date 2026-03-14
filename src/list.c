/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "config.h"
#include <razorback/types.h>
#include <razorback/lock.h>
#include <razorback/debug.h>
#include <razorback/list.h>
#include <razorback/log.h>

#include <string.h>

/** List node structure.
 */
struct ListNode {
    struct ListNode *next;  ///< Next node
    struct ListNode *prev;  ///< Previous node
    void *item;             ///< Item data
    atomic_bool del;        ///< Node deletion marker
};

/** List structure.
 * Note - The list can not be mutated from the iterator!!!
 * Calls to List_Push/List_Pop/List_Remove from inside the
 * the iterator function will cause a deadlock.
 * The only supported mutation inside the iterator is to
 * request the current node is removed by returning
 * LIST_EACH_END.  List_ForEach will process the deleted
 * after passing through the entire list as it needs to
 * 'upgrade' to a write lock which can not be done while
 * holding the read lock.
 */
struct _List {
    struct ListNode * head;         ///< Head node
    struct ListNode * tail;         ///< Tail node
    atomic_size_t length;           ///< Number of items in the list
    size_t limit;                   ///< Maximum number of items in the list
    int mode;                       ///< Operation mode
    int (*cmp)(void *, void *);     ///< Node comparator
    int (*keyCmp)(void *, void *);  ///< Node key comparator
    void (*destroy)(void *);        ///< Node data destructor
    void *(*clone)(void *);         ///< Node data clone
    void (*nodeLock)(void *);       ///< Node lock function
    void (*nodeUnlock)(void *);     ///< Node unlock function
    Semaphore_t *sem;               ///< List event semaphore.
    RWLock_t *lock;                 ///< RW Lock
};


static void List_RemoveNode(List_t *list, struct ListNode *node);

static bool List_Stack_Push(List_t *list, struct ListNode *node);
static bool List_Queue_Push(List_t *list, struct ListNode *node);

static struct ListNode * List_Stack_Pop(List_t *list);
static struct ListNode * List_Queue_Pop(List_t *list);

SO_PUBLIC List_t *
List_Create(int mode,
        int (*cmp)(void *, void *),
        int (*keyCmp)(void *, void *),
        void (*destroy)(void *),
        void *(*clone)(void *),
        void (*nodeLock)(void *),
        void (*nodeUnlock)(void *)) {
    List_t * list;

    if ((list = calloc(1,sizeof(List_t))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: Failed to allocate list", __func__);
        return NULL;
    }

    if ((list->lock = RWLock_Create() ) == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: Failed to create list lock", __func__);
        free(list);
        return NULL;
    }

    if (mode == LIST_MODE_QUEUE || mode == LIST_MODE_STACK) {
        if ((list->sem = Semaphore_Create(true, 0)) == NULL) {
            rzb_log(LOG_ERR, LOG_C_LIST, "%s: Failed to create list semaphore", __func__);
            RWLock_Destroy(list->lock);
            free(list);
            return NULL;
        }
    } else {
        list->sem = NULL;
    }

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
    List_t *list;
    void * ret;
    void * id;
};

static int List_FindKeyCmp(void * item, void *d) {
    struct FindData *data = (struct FindData *)d;
    if (data->list->keyCmp(item, data->id) == 0) {
        data->ret = item;
        // TODO - Check the node lock function and call it if set
        return LIST_EACH_END;
    }
    return LIST_EACH_OK;
}

SO_PUBLIC void
List_SetLimit(List_t *list, size_t limit) {
    ASSERT(list != NULL);
    if (list == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: list is NULL", __func__);
        return;
    }
    RWLock_WriteLock(list->lock);
    list->limit = limit;
    RWLock_Unlock(list->lock);
}

SO_PUBLIC void *
List_Find(List_t *list, void *id) {
    struct FindData data;
    ASSERT(list != NULL);
    ASSERT(id != NULL);
    ASSERT(list->keyCmp != NULL);


    if ((list == NULL) || (id == NULL) || (list->keyCmp == NULL)) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: Invalid parameter", __func__);
        return NULL;
    }

    data.list = list;
    data.id = id;
    data.ret = NULL;

    if (!List_ForEach(list, List_FindKeyCmp, &data)) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: Error calling List_ForEach", __func__);
    }

    return data.ret;
}

SO_PUBLIC bool
List_Push(List_t *list, void *item) {
    struct ListNode *node;
    bool insert_result = false;
    ASSERT(list != NULL);
    if (list == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: list is NULL", __func__);
        return false;
    }

    if ((node = calloc(1, sizeof(struct ListNode))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: Failed to allocate list node", __func__);
        return false;
    }

    node->item = item;


    RWLock_WriteLock(list->lock);
    if (list->limit > 0 && list->length >= list->limit) {
        RWLock_Unlock(list->lock);
        free(node);
        return false;
    }
    switch (list->mode) {
        case LIST_MODE_GENERIC:
        case LIST_MODE_QUEUE:
            insert_result = List_Queue_Push(list, node);
            break;
        case LIST_MODE_STACK:
            insert_result = List_Stack_Push(list, node);
            break;
    }
    if (!insert_result) {
        free(node);
        RWLock_Unlock(list->lock);
        return false;
    }

    list->length++;
    RWLock_Unlock(list->lock);

    if (list->sem != NULL) {
        Semaphore_Post(list->sem);
    }
    return true;
}

static struct ListNode *
List_Do_Pop(List_t *list) {
    ASSERT(list != NULL);
    if (list == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: list is NULL", __func__);
        return NULL;
    }
    switch (list->mode) {
        case LIST_MODE_GENERIC:
        case LIST_MODE_QUEUE:
            return List_Queue_Pop(list);
        case LIST_MODE_STACK:
            return List_Stack_Pop(list);
    }
    return NULL;

}

SO_PUBLIC void *
List_Pop(List_t *list) {
    struct ListNode *node = NULL;
    void * ret = NULL;
    ASSERT(list != NULL);
    if (list == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: list is NULL", __func__);
        return NULL;
    }

    if (list->sem != NULL) {
        Semaphore_Wait(list->sem);
    }

    RWLock_WriteLock(list->lock);
    node = List_Do_Pop(list);
    RWLock_Unlock(list->lock);

    if (node != NULL) {
        ret = node->item;
        free(node);
    }

    return ret;
}


SO_PUBLIC bool
List_ForEach(List_t *list, int (*op)(void *, void *), void *userData) {
    struct ListNode *cur = NULL, *delNode;
    bool del = false;
    bool ret = true;
    bool last = false;
    int opRet;
    ASSERT(list != NULL);
    if (list == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: list is NULL", __func__);
        return false;
    }
    ASSERT(op != NULL);
    if (op == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST,"%s: op is NULL", __func__);
        return false;
    }
    // List is empty we did what was asked successfully
    if (list->head == NULL) {
        return true;
    }
    RWLock_ReadLock(list->lock);
    cur = list->head;

    while (cur != NULL)
    {
        last = false;
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
                rzb_log(LOG_DEBUG, LOG_C_LIST, "%s: Delete requested marking node for deletion: %p", __func__, cur);
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
        rzb_log(LOG_DEBUG, LOG_C_LIST, "%s: Item deletion requested in loop pruning list", __func__);
        RWLock_WriteLock(list->lock);
        cur = list->head;
        while (cur != NULL) {
            if (cur->del) {
                delNode = cur;
                cur = delNode->next;
                rzb_log(LOG_DEBUG, LOG_C_LIST, "%s: Removing deleted node from list %p", __func__, delNode);
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

SO_PUBLIC bool
List_Remove(List_t *list, void *item)
{
    struct FindData data;
    ASSERT(list != NULL);
    ASSERT(item != NULL);
    if (list == NULL)
        return false;
    if (item == NULL)
        return false;
    data.id = item;
    data.list = list;
    data.ret = NULL;
    if (!List_ForEach(list, List_FindRemove, &data)) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: Error calling List_ForEach", __func__);
    }
    if(data.ret == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: Failed remove item", __func__);
        return false;
    }

    return true;
}

static void
List_RemoveNode(List_t *list, struct ListNode *node)
{
    ASSERT(list != NULL);
    if (list == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: list is NULL", __func__);
        return;
    }
    ASSERT(node != NULL);
    if (node == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: node is NULL", __func__);
        return;
    }
    ASSERT(list->head != NULL);
    if (list->head == NULL) {
        return;
    }

    if (node == list->head && node == list->tail) {
        list->head = NULL;
        list->tail = NULL;
    } else if (node == list->head) {
        list->head = node->next;
        list->head->prev = NULL;
    } else if (node == list->tail) {
        list->tail = node->prev;
        list->tail->next = NULL;
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    list->length--;
}

SO_PUBLIC void
List_Clear(List_t *list)
{
    struct ListNode *item;
    ASSERT(list != NULL);
    if (list == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: list is NULL", __func__);
        return;
    }

    item = List_Do_Pop(list);
    while (item != NULL) {
        if (list->destroy != NULL) {
            list->destroy(item->item);
        }
        free(item);
        item = List_Do_Pop(list);
    }
}

SO_PUBLIC size_t
List_Length(List_t *list) {
    ASSERT(list != NULL);
    if (list == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: list is NULL", __func__);
        return 0;
    }

    return list->length;
}

SO_PUBLIC void
List_Destroy(List_t *list) {
    ASSERT(list != NULL);
    if (list == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: list is NULL", __func__);
        return;
    }

    List_Clear(list);
    RWLock_Destroy(list->lock);
    free(list);
}

static int
List_Clone_Node(void *vItem, void *vDest) {
    List_t *dest = (List_t*)vDest;
    void *new = dest->clone(vItem);
    if (new == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: Failed to clone node", __func__);
        return LIST_EACH_ERROR;
    }
    if (!List_Push(dest, new)) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: Failed to push cloned node to new list", __func__);
        return LIST_EACH_ERROR;
    }
    return LIST_EACH_OK;

}

SO_PUBLIC List_t*
List_Clone (List_t *source) {
    List_t *dest;
    ASSERT(source != NULL);
    if (source == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: source list is NULL", __func__);
        return NULL;
    }
    ASSERT(source->clone != NULL);
    if (source->clone == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: source list clone function is NULL", __func__);
        return NULL;
    }

    dest = List_Create(source->mode,
            source->cmp,
            source->keyCmp,
            source->destroy,
            source->clone,
            source->nodeLock,
            source->nodeUnlock);

    if(dest == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: Failed to create destination list", __func__);
        return NULL;
    }

    if (!List_ForEach(source, List_Clone_Node, dest)) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: Failed to clone list nodes", __func__);
        List_Destroy(dest);
        return NULL;
    }
    return dest;
}

SO_PUBLIC bool
List_Transfer (List_t *dest, List_t *source) {
    ASSERT(source != NULL);
    ASSERT(dest != NULL);
    if (source == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: source list is NULL", __func__);
        return false;
    }
    if (dest == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: dest list is NULL", __func__);
        return false;
    }

    RWLock_WriteLock(dest->lock);
    RWLock_WriteLock(source->lock);

    List_Clear(dest);
    dest->head = source->head;
    dest->tail = source->tail;
    dest->length = source->length;
    source->head = NULL;
    source->tail = NULL;
    source->length = 0;

    RWLock_Unlock(dest->lock);
    RWLock_Unlock(source->lock);

    return true;
}



static bool
List_Stack_Push(List_t *list, struct ListNode *node) {
    ASSERT(list != NULL);
    ASSERT(node != NULL);
    if (list == NULL || node == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: list or node is NULL", __func__);
        return false;
    }

    if (list->head == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    return true;
}

static bool
List_Queue_Push(List_t *list, struct ListNode *node) {
    ASSERT(list != NULL);
    ASSERT(node != NULL);
    if (list == NULL || node == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: list or node is NULL", __func__);
        return false;
    }

    if (list->tail == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }
    return true;
}

static struct ListNode *
List_Stack_Pop(List_t *list)
{
    struct ListNode *ret;
    ASSERT(list != NULL);
    if (list == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: list is NULL", __func__);
        return NULL;
    }

    if (list->head == NULL) {
        return NULL;
    }


    ret = list->head;
    List_RemoveNode(list, ret);

    return ret;
}

static struct
ListNode * List_Queue_Pop(List_t *list) {
    struct ListNode *ret;
    ASSERT(list != NULL);
    if (list == NULL) {
        rzb_log(LOG_ERR, LOG_C_LIST, "%s: list is NULL", __func__);
        return NULL;
    }

    if (list->tail == NULL) {
        return NULL;
    }

    ret = list->tail;
    // Clang scan-build complains about this line
    // but there is no bug. The analyzer is confused
    // by the fact that we are removing the node
    // from the list and it thinks that the tail
    // pointer is dangling.  The tail pointer is
    // updated in List_RemoveNode.
    List_RemoveNode(list, ret);

    return ret;
}
