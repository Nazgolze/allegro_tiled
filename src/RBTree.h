#ifndef _RBTREE_H
#define _RBTREE_H

#include <allegro5/allegro.h>
#include <stdbool.h>

struct rb_node {
	struct rb_node *left;
	struct rb_node *right;
	struct rb_node *parent;
	char color; // 0 = black, 1 = red
	void *key;
	void *data;
};

typedef int(*cmp_func)(void *, void *);
typedef void(*destroy_func)(void *);

struct rb_tree {
	struct rb_node *root;
	cmp_func cmp;
	destroy_func key_destroy_func;
	destroy_func data_destroy_func;
	int ref_count;
	//ALLEGRO_MUTEX *mutex;
};

typedef struct rb_tree RBTree;
typedef struct rb_node RBNode;

RBTree *rb_tree_new(cmp_func, destroy_func, destroy_func);

void rb_tree_insert(RBTree *, void *, void *);
void *rb_tree_lookup(RBTree *, void *);
bool rb_tree_remove(RBTree *, void *);

void rb_tree_delete(RBTree *);

void rb_tree_preorder_print(RBNode *);
void rb_tree_inorder_print(RBNode *);

void rb_tree_ref(RBTree *);
void rb_tree_unref(RBTree *);

//void rb_tree_debug_print();
#endif
