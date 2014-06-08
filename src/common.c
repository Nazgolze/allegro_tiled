#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

SList *slist_prepend(SList *list, void *ptr)
{
	SList *new_elem = al_malloc(sizeof(SList));
	fail_fast(new_elem == NULL);

	new_elem->next = list;
	new_elem->data = ptr;

	return new_elem;
}

SList *slist_reverse(SList *list)
{
	SList *head = list;
	SList *iter = head;
	SList *new_head = NULL;

	for(; iter != NULL; iter = iter->next) {
		new_head = slist_prepend(new_head, iter->data);
	}
	slist_free(head);

	return new_head;
}

void slist_free(SList *list)
{
	slist_free_full(list, NULL);
}

void slist_free_full(SList *list, void(*ptr)(void *))
{
	SList *next;
	while(list != NULL) {
		next = list->next;
		if(ptr != NULL) {
			ptr(list->data);
		}
		al_free(list);
		list = next;
	}
}

int *intdup(int x)
{
	int *n = al_malloc(sizeof(int));
	*n = x;
	return n;
}

int intcmp(int *a, int *b)
{
#if 0
	printf("*a = %d\n", *a);
	printf("*b = %d\n", *b);
	fflush(stdout);
	if(*a > 32000)
		abort();
#endif
	if(*a == *b)
		return 0;
	else if(*a < *b)
		return -1;
	return 1;
}

char *strstrip(char *string)
{
	int ix, jx, count = 0, len;
	bool inc = false;
	len = strlen(string);
	char whitespace[]
		= {'\n', '\t', ' ', 12, '\v'};

	char temp[len];
	memset(temp, 0, len);

	for(ix = 0; ix < len; ix++) {
		for(jx = 0; jx < sizeof(whitespace) / sizeof(char); jx++) {
			if(string[ix] == whitespace[jx]) {
				inc = true;
			}
		}
		if(inc) {
			count++;
			inc = false;
		} else {
			break;
		}
	}

	strcpy(temp, string + count);
	memset(string, 0, len);
	strcpy(string, temp);
	len = strlen(string);
	count = 0;

	for(ix = len - 1; ix > 0; ix--) {
		for(jx = 0; jx < sizeof(whitespace) / sizeof(char); jx++) {
			if(string[ix] == whitespace[jx]) {
				inc = true;
			}
		}
		if(inc) {
			count++;
			inc = false;
		} else {
			break;
		}
	}

	memset(string + (len - count), 0, count);
	return string;
}

unsigned char *base64_decode(char *input, size_t *retlen)
{
	char index[]
		= {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
			 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
			 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
			 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
			 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
			 '8', '9', '+', '/'};
	int len = strlen(input);
	unsigned char temp[len];
	memset(&temp, 0, len);
	unsigned char *ret;
	int ix, jx;
	int count = 0;
	unsigned long margin = 0;

	*retlen = 0;

	for(ix = 0; ix < len; ix+=4) {
		for(jx = 0; jx < 64; jx++) {
			if(input[ix] == index[jx]) {
				temp[count] |= jx << 2;
			}
			if(input[ix + 1] == index[jx]) {
				temp[count] |= jx >> 4;
				temp[count + 1] |= jx << 4;
			}
			if(input[ix + 2] == index[jx]) {
				temp[count + 1] |= jx >> 2;
				temp[count + 2] |= jx << 6;
			}
			if(input[ix + 3] == index[jx]) {
				temp[count + 2] |= jx;
			}
		}
		count += 3;
	}

	if(input[len - 2] == '=' && input[len - 1] == '=') {
		count += 2;
	} else if(input[len - 1] == '=') {
		count += 1;
	}
	*retlen = (unsigned long)count;
	ret = al_calloc(count, sizeof(unsigned char));

	memcpy(ret, &temp, count);

	return ret;
}

char *n_strdup(char *str)
{
	if(!str) {
		return strdup("");
	}
	return strdup(str);
}
