#ifndef LIST_H
#define LIST_H
#include <stdlib.h>

typedef struct int_list {
    int capicity;
    int size;
    int *element;
} *list;

list create_list(int capicity) {
    list result;
    if (capicity <= 0) {
        return NULL;
    } else {
        result = (list) malloc(sizeof(struct int_list));
        result->capicity = capicity;
        result->size = 0;
        result->element = (int*) calloc(sizeof(int), capicity);
        return result;
    }
}

int list_length(list lst) {
    return lst->size;
}

int list_get(list lst, int index) {
    return lst->element[index];
}

void list_insert(list lst, int a, int index) {
    int *tmp_element;
    int i;
    if (lst->capicity == lst->size) {
        tmp_element = (int*) calloc(sizeof(int), lst->capicity * 2);
        for (i = 0; i != lst->capicity; ++i)
            tmp_element[i] = lst->element[i];
        free(lst->element);
        lst->capicity = lst->capicity * 2;
        lst->element = tmp_element;
    }
    for (i = lst->size; i != index; --i)
        lst->element[i] = lst->element[i - 1];
    lst->element[index] = a;
    ++lst->size;
}

void list_add(list lst, int a) {
    list_insert(lst, a, lst->size);
}

void list_remove(list lst, int index) {
    int i;
    for (i = index; i != lst->size-1; ++i)
        lst->element[i] = lst->element[i + 1];
    --lst->size;
}

void list_clean(list *lst) {
    free((*lst)->element);
    free(lst);
}

#endif
