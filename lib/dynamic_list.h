//
// Created by Johnny on 06/09/2018.
//

#ifndef LIRC_LIST_H
#define LIRC_LIST_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include <stddef.h>

// a common function used to free malloc'd objects
typedef void (*freeFunction)(void *);
typedef bool (*listIterator)(void *);
typedef bool (*listCompare)(void *, void *);

typedef struct _listNode {
    void *data;
    struct _listNode *next;
} dynamic_list_node;

typedef struct {
    int logicalLength;
    size_t elementSize;
    dynamic_list_node *head;
    dynamic_list_node *tail;
    freeFunction freeFn;
} dynamic_list;

/**
 * Initializes a linked list to store elements of elementSize and to call
 * freeFunction for each element when destroying a list
 * @param list: a pointer to the list
 * @param elementSize: the size of the elements being stored
 * @param freeFn: a function to be called for each element when the list is destroyed
 */
void list_new(dynamic_list *list, size_t elementSize, freeFunction freeFn);
/**
 * frees dynamically allocated nodes and optionally calls
 * freeFunction with each node’s data pointer
 * @param list
 */
void list_destroy(dynamic_list *list);
/**
 * adds a node to the head of the list
 * @param list
 * @param element
 */
void list_prepend(dynamic_list *list, void *element);
/**
 * adds a node to the tail of the list
 * @param list
 * @param element
 */
void list_append(dynamic_list *list, void *element);
/**
 * returns the number of items in the list
 * @param list
 * @return
 */
int list_size(dynamic_list *list);
/**
 * calls the supplied iterator function with the data element
 * of each node (iterates over the list)
 * @param list
 * @param iterator
 */
void list_for_each(dynamic_list *list, listIterator iterator);
/**
 * returns the head of the list (optionally removing it at the same time)
 * @param list
 * @param element
 * @param removeFromList
 */
void list_head(dynamic_list *list, void *element, bool removeFromList);
/**
 * returns the tail of the list
 * @param list
 * @param element
 */
void list_tail(dynamic_list *list, void *element);
/**
 * calls the supplied comparison function for each node of the list
 * compare(listnode, data)
 * @param list
 * @param data
 * @param compare
 * @return
 */
bool list_contains(dynamic_list *list, void* data, listCompare compare);

#ifdef __cplusplus
}
#endif

#endif //LIRC_LIST_H
