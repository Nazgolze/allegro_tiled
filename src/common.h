#ifndef _COMMON_H
#define _COMMON_H

#include <allegro5/allegro.h>
#include "RBTree.h"

struct SList {
	struct SList *next; 
	void *data;
};

typedef struct SList SList;

SList *slist_prepend(SList *list, void *ptr);
SList *slist_reverse(SList *list);
#define slist_next(list) list->next
void slist_free(SList *list);
void slist_free_full(SList *list, void(*ptr)(void *));

int *intdup(int);
int intcmp(int *a, int *b);

char *strstrip(char *);
unsigned char *base64_decode(char *, size_t *);
char *n_strdup(char *);

#define fail_fast(expr) do { if(expr) {abort();}} while(0)
#define streq(a, b) (strcmp((a), (b)) == 0)


#endif
