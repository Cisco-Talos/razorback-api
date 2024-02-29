#ifndef RAZORBACK_LIST_H
#define RAZORBACK_LIST_H
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

#define LIST_MODE_GENERIC 0
#define LIST_MODE_STACK 1
#define LIST_MODE_QUEUE 2

#define LIST_EACH_OK 0
#define LIST_EACH_ERROR 1
#define LIST_EACH_REMOVE 2

struct ListNode
{
    struct ListNode *next;
    struct ListNode *prev;
    void *item;
};

struct List 
{
    struct ListNode *head;
    struct ListNode *tail;
    size_t length;
    int mode;
    int (*cmp)(void *, void *);
    int (*keyCmp)(void *, void *);
    void (*destroy)(void *);
    void *(*clone)(void *);
    void (*nodeLock)(void *);
    void (*nodeUnlock)(void *);
    pthread_mutex_t lock;
};

extern struct List * List_Create(int mode, 
        int (*cmp)(void *, void *), 
        int (*keyCmp)(void *, void *), 
        void (*destroy)(void *), 
        void *(*clone)(void *),
        void (*nodeLock)(void *),
        void (*nodeUnlock)(void *));

extern bool List_Push(struct List *list, void *item);
extern void * List_Pop(struct List *list);
extern void List_Remove(struct List *list, void *item);
extern void * List_Find(struct List *list, void *id);
extern bool List_ForEach(struct List *list, int (*op)(void *, void *), void *);
extern size_t List_Length(struct List *list);
extern void List_Clear(struct List *list);
extern void List_Lock(struct List *list);
extern void List_Unlock(struct List *list);
extern void List_Destroy(struct List *list);
extern struct List* List_Clone (struct List *source);

#endif //RAZORBACK_LIST_H
