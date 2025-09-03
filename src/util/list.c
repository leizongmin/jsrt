#include "list.h"

#include <stdlib.h>

JSRT_List* JSRT_ListNew() {
  JSRT_List* list = malloc(sizeof(JSRT_List));
  list->head = NULL;
  list->tail = NULL;
  list->length = 0;
  return list;
}

void JSRT_ListFree(JSRT_List* list) {
  JSRT_ListNode* node = list->head;
  while (node != NULL) {
    JSRT_ListNode* next = node->next;
    free(node);
    node = next;
  }
  free(list);
}

void JSRT_ListAppend(JSRT_List* list, void* data) {
  JSRT_ListNode* node = malloc(sizeof(JSRT_ListNode));
  node->data = data;
  node->next = NULL;
  if (list->tail != NULL) {
    list->tail->next = node;
  }
  list->tail = node;
  if (list->head == NULL) {
    list->head = node;
  }
  list->length++;
}

void JSRT_ListPrepend(JSRT_List* list, void* data) {
  JSRT_ListNode* node = malloc(sizeof(JSRT_ListNode));
  node->data = data;
  node->next = list->head;
  list->head = node;
  if (list->tail == NULL) {
    list->tail = node;
  }
  list->length++;
}

void JSRT_ListInsert(JSRT_List* list, void* data, size_t index) {
  if (index == 0) {
    JSRT_ListPrepend(list, data);
    return;
  }
  if (index == list->length) {
    JSRT_ListAppend(list, data);
    return;
  }
  JSRT_ListNode* node = malloc(sizeof(JSRT_ListNode));
  node->data = data;
}

void JSRT_ListRemove(JSRT_List* list, size_t index) {
  if (index == 0) {
    JSRT_ListNode* node = list->head;
    list->head = node->next;
    free(node);
    list->length--;
    return;
  }
  if (index == list->length - 1) {
    JSRT_ListNode* node = list->tail;
    list->tail = node->prev;
    free(node);
    list->length--;
    return;
  }
}

void JSRT_ListClear(JSRT_List* list) {
  JSRT_ListNode* node = list->head;
  while (node != NULL) {
    JSRT_ListNode* next = node->next;
    free(node);
    node = next;
  }
  list->head = NULL;
  list->tail = NULL;
  list->length = 0;
}

void* JSRT_ListGet(JSRT_List* list, size_t index) {
  JSRT_ListNode* node = list->head;
  for (size_t i = 0; i < index; i++) {
    node = node->next;
  }
  return node->data;
}

void* JSRT_ListPop(JSRT_List* list) {
  void* data = list->tail->data;
  JSRT_ListNode* node = list->tail;
  list->tail = node->prev;
  free(node);
  list->length--;
  return data;
}
