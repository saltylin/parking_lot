#ifndef QUEUE_H
#define QUEUE_H

#include "list.h"

struct int_node {
    int data;
    struct int_node *next;
};

typedef struct int_queue {
    struct int_node *first;
    struct int_node *last;
} *queue;

queue create_queue() {
    queue result;
    result = (queue) malloc(sizeof(struct int_queue));
    result->first = (struct int_node *) malloc(sizeof(struct int_node));
    result->first->next = NULL;
    result->last = result->first;
    return result;
}

void enqueue(queue q, int x) {
    q->last->next = (struct int_node *) malloc(sizeof(struct int_node));
    q->last = q->last->next;
    q->last->data = x;
    q->last->next = NULL;
}

int dequeue(queue q) {
    struct int_node *ptr = q->first->next;
    int result = ptr->data;
    free(q->first);
    q->first = ptr;
    return result;
}

int queue_first(queue q) {
    return q->first->next->data;
}

int queue_isempty(queue q) {
    return q->first == q->last;
}

void queue_clear(queue q) {
    struct int_node *tmp;
    while (q->first->next != NULL) {
        tmp = q->first;
        q->first = q->first->next;
        free(tmp);
    }
}

#endif
