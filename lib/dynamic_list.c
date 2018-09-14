//
// Created by Johnny on 06/09/2018.
//

#include "dynamic_list.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

void list_new(dynamic_list *list, size_t elementSize, freeFunction freeFn)
{
    assert(elementSize > 0);
    list->logicalLength = 0;
    list->elementSize = elementSize;
    list->head = list->tail = NULL;
    list->freeFn = freeFn;
}

void list_destroy(dynamic_list *list)
{
    dynamic_list_node *current;

    if(list == NULL)
        return;

    while(list->head != NULL) {
        current = list->head;
        list->head = current->next;

        if(list->freeFn) {
            list->freeFn(current->data);
        }

        free(current->data);
        free(current);
    }
}

void list_prepend(dynamic_list *list, void *element)
{
    dynamic_list_node *node = malloc(sizeof(dynamic_list_node));
    node->data = malloc(list->elementSize);
    memcpy(node->data, element, list->elementSize);

    node->next = list->head;
    list->head = node;

    // first node?
    if(!list->tail) {
        list->tail = list->head;
    }

    list->logicalLength++;
}

void list_append(dynamic_list *list, void *element)
{
    dynamic_list_node *node = malloc(sizeof(dynamic_list_node));
    node->data = malloc(list->elementSize);
    node->next = NULL;

    memcpy(node->data, element, list->elementSize);

    if(list->logicalLength == 0) {
        list->head = list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }

    list->logicalLength++;
}

void list_for_each(dynamic_list *list, listIterator iterator)
{
    assert(iterator != NULL);

    dynamic_list_node *node = list->head;
    bool result = true;
    while(node != NULL && result) {
        result = iterator(node->data);
        node = node->next;
    }
}

bool list_contains(dynamic_list *list, void* data, listCompare compare)
{
    assert(compare != NULL);

    dynamic_list_node *node = list->head;
    bool result = false;

    while (node != NULL && !result) {
        result = compare(node->data, data);
        node = node->next;
    }
    return result;
}

void list_head(dynamic_list *list, void *element, bool removeFromList)
{
    assert(list->head != NULL);

    dynamic_list_node *node = list->head;
    memcpy(element, node->data, list->elementSize);

    if(removeFromList) {
        list->head = node->next;
        list->logicalLength--;

        if(list->freeFn) {
            list->freeFn(node->data);
        }

        free(node->data);
        free(node);
    }
}

void list_tail(dynamic_list *list, void *element)
{
    assert(list->tail != NULL);
    dynamic_list_node *node = list->tail;
    memcpy(element, node->data, list->elementSize);
}

int list_size(dynamic_list *list)
{
    return list->logicalLength;
}