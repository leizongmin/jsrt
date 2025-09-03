#ifndef __JSRT_UTIL_LIST_H__
#define __JSRT_UTIL_LIST_H__

#include <stdbool.h>
#include <stddef.h>

typedef struct JSRT_ListNode {
  void* data;
  struct JSRT_ListNode* prev;
  struct JSRT_ListNode* next;
} JSRT_ListNode;

typedef struct {
  JSRT_ListNode* head;
  JSRT_ListNode* tail;
  size_t length;
} JSRT_List;

JSRT_List* JSRT_ListNew();
void JSRT_ListFree(JSRT_List* list);

void JSRT_ListAppend(JSRT_List* list, void* data);
void JSRT_ListPrepend(JSRT_List* list, void* data);
void JSRT_ListInsert(JSRT_List* list, void* data, size_t index);
void JSRT_ListRemove(JSRT_List* list, size_t index);
void JSRT_ListClear(JSRT_List* list);

void* JSRT_ListGet(JSRT_List* list, size_t index);
void* JSRT_ListPop(JSRT_List* list);

static inline size_t JSRT_ListLength(JSRT_List* list) {
  return list->length;
}

static inline bool JSRT_ListIsEmpty(JSRT_List* list) {
  return list->length == 0;
}

static inline void* JSRT_ListHead(JSRT_List* list) {
  return list->head->data;
}

static inline void* JSRT_ListTail(JSRT_List* list) {
  return list->tail->data;
}

static inline void JSRT_ListForEach(JSRT_List* list, void (*callback)(void* data)) {
  JSRT_ListNode* node = list->head;
  while (node != NULL) {
    callback(node->data);
    node = node->next;
  }
}

static inline void JSRT_ListForEachReverse(JSRT_List* list, void (*callback)(void* data)) {
  JSRT_ListNode* node = list->tail;
  while (node != NULL) {
    callback(node->data);
    node = node->next;
  }
}

#endif
