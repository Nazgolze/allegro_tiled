#include "common.h"
#include <stdlib.h>
#include <stdio.h>

static int reversed = 0;

static const char black = 0;
static const char red = 1;

static void _rb_tree_insert(RBTree *tree, void *key, void *data, RBNode *node);

static void _delete_case1(RBTree *tree, RBNode *node);

static bool _is_black(RBNode *node)
{
	if(!node) {
		return true;
	}

	if(node->color == black) {
		return true;
	}
	return false;
}

RBTree *rb_tree_new(cmp_func cmp, destroy_func k, destroy_func d)
{
	RBTree *tree = al_calloc(1, sizeof(RBTree));
	tree->ref_count = 1;
	tree->cmp = cmp;
	tree->key_destroy_func = k;
	tree->data_destroy_func = d;

	//tree->mutex = al_create_mutex_recursive();

	return tree;
}

static RBNode *_rb_tree_lookup(RBTree *tree, RBNode *node, void *key)
{
	RBNode *ret_node = NULL;
	if(node == NULL) {
		return NULL;
	}
	int cmp_result = tree->cmp(key, node->key);
	if(cmp_result == 0) {
		ret_node = node;
	} else if(cmp_result < 0) {
		ret_node = _rb_tree_lookup(tree, node->left, key);
	} else {
		ret_node = _rb_tree_lookup(tree, node->right, key);
	}

	return ret_node;
}

void *rb_tree_lookup(RBTree *tree, void *key)
{
	RBNode *retnode = _rb_tree_lookup(tree, tree->root, key);
	if(retnode)
		return retnode->data;
	return NULL;
}

static RBNode *_grandparent(RBNode *node)
{
	if(node && node->parent) {
		return node->parent->parent;
	}
	return NULL;
}

static RBNode *_uncle(RBNode *node)
{
	RBNode *gp = _grandparent(node);
	if(!gp) {
		return NULL;
	}

	if(gp->left == node->parent) {
		return gp->right;
	}
	return gp->left;
}

static void _rotate_left(RBTree *tree, RBNode *node)
{
	if(!node) {
		return;
	}
	RBNode *parent = node;
	node = parent->right;
	RBNode *gp = parent->parent;
	RBNode *temp = node->left;

	if(gp) {
		if(parent == gp->left) {
			gp->left = node;
		} else if(parent == gp->right) {
			gp->right = node;
		}
	} else {
		tree->root = node;
	}
	node->parent = gp;
	node->left = parent;
	parent->parent = node;
	parent->right = temp;
	if(temp) {
		temp->parent = parent;
	}
}

static void _rotate_right(RBTree *tree, RBNode *node)
{
	if(!node) {
		return;
	}
	RBNode *parent = node;
	node = parent->left;
	RBNode *gp = parent->parent;
	RBNode *temp = node->right;

	if(gp) {
		if(parent == gp->left) {
			gp->left = node;
		} else if(parent == gp->right) {
			gp->right = node;
		}
	}	
	node->parent = gp;
	node->right = parent;
	parent->parent = node;
	parent->left = temp;
	if(temp) {
		temp->parent = parent;
	}
}
#define stuff printf("%s-%d\n", __FUNCTION__, __LINE__); fflush(stdout);
static void _rb_tree_insert_finalize(RBTree *tree, RBNode *node)
{
	if(!node) {
		return;
	}
	if(!node->parent) {
		node->color = black;
		tree->root = node;
		return;
	}
	if(node->parent->color == black) {
		return;
	}
	RBNode *parent = node->parent;
	RBNode *uncle = _uncle(node);
	RBNode *grandparent = _grandparent(node);

	int uncle_color;
	if(grandparent && !uncle) {
		uncle_color = black;
	} else {
		uncle_color = uncle->color;
	}

	if(uncle && uncle->color == red && parent->color == red) {
		parent->color = black;
		uncle->color = black;
		grandparent->color = red;
		_rb_tree_insert_finalize(tree, grandparent);
	} else if(parent->color == red && uncle_color == black) {
		if(node == parent->right && parent == grandparent->left) {
			_rotate_left(tree, parent);
			node = node->left;
		} else if(node == parent->left && parent == grandparent->right) {
			_rotate_right(tree, parent);
			node = node->right;
		}
		grandparent = _grandparent(node);
		node->parent->color = black;
		grandparent->color = red;
		if(node == parent->left && parent == grandparent->left) {
			_rotate_right(tree, grandparent);
		} else if(node == parent->right && parent == grandparent->right) {
			_rotate_left(tree, grandparent);
		}
	}
}

void _rb_tree_insert(RBTree *tree, void *key, void *data, RBNode *node)
{
	if(tree->cmp(key, node->key) < 0) {
		if(!node->left) {
			node->left = al_calloc(1, sizeof(RBNode));
			node->left->key = key;
			node->left->data = data;
			node->left->color = red;
			node->left->parent = node;
			_rb_tree_insert_finalize(tree, node->left);
		} else {
			_rb_tree_insert(tree, key, data, node->left);
		}
	} else if(tree->cmp(key, node->key) > 0) {
		if(!node->right) {
			node->right = al_calloc(1, sizeof(RBNode));
			node->right->key = key;
			node->right->data = data;
			node->right->color = red;
			node->right->parent = node;
			_rb_tree_insert_finalize(tree, node->right);
		} else {
			_rb_tree_insert(tree, key, data, node->right);
		}
	} else {
		if(node->data != data) {
			if(tree->data_destroy_func)
				tree->data_destroy_func(node->data);
			node->data = data;
		}
		if(node->key != key) {
			tree->key_destroy_func(node->key);
			node->key = key;
		}
		_rb_tree_insert_finalize(tree, node);
	}
	return;
}

void rb_tree_insert(RBTree *tree, void *key, void *data)
{
	if(tree->root == NULL) {
		tree->root = al_calloc(1, sizeof(RBNode));
		tree->root->key = key;
		tree->root->data = data;
	} else {
		_rb_tree_insert(tree, key, data, tree->root);
	}
	return;
}

/*
 * find the minimum value in right node
 */
static RBNode *_successor(RBNode *node)
{
	RBNode *ret = node->right;
	while(ret->left) {
		ret = ret->left;
	}
	return ret;
}

static void _rb_free_node(RBTree *tree, RBNode *node)
{
	if(tree->key_destroy_func) {
		tree->key_destroy_func(node->key);
	}
	if(tree->data_destroy_func) {
		tree->data_destroy_func(node->data);
	}
}

static RBNode *_sibling(RBNode *node)
{
	if(node == node->parent->left) {
		return node->parent->right;
	} else {
		return node->parent->left;
	}
}

static void _delete_case5(RBTree *tree, RBNode *node)
{
	RBNode *sibling = _sibling(node);

	sibling->color = node->parent->color;
	node->parent->color = black;

	if(node == node->parent->left) {
		sibling->right->color = black;
		_rotate_left(tree, node->parent);
	} else {
		sibling->left->color = black;
		_rotate_right(tree, node->parent);
	}
}

static void _delete_case4(RBTree *tree, RBNode *node)
{
	RBNode *sibling = _sibling(node);

	if(sibling->color == black) {
		if((node == node->parent->left) &&
			 (_is_black(sibling->right)) &&
			 (sibling->left->color == red)) {
			sibling->color = red;
			sibling->left->color = black;
			_rotate_right(tree, sibling);
		} else if((node == node->parent->right) &&
							(_is_black(sibling->left)) &&
							(sibling->right->color == red)) {
			sibling->color = red;
			sibling->right->color = black;
			_rotate_left(tree, sibling);
		}
	}
	_delete_case5(tree, node);
}

static void _delete_case3(RBTree *tree, RBNode *node)
{
	RBNode *sibling = _sibling(node);

	if((node->parent->color == red) &&
		 (sibling->color == black) &&
		 (_is_black(sibling->left)) &&
		 (_is_black(sibling->right))) {
		sibling->color = red;
		node->parent->color = black;
	} else {
		_delete_case4(tree, node);
	}
}

static void _delete_case2(RBTree *tree, RBNode *node)
{
	RBNode *sibling = _sibling(node);

	if((node->parent->color == black) &&
		 (sibling->color == black) &&
		 (_is_black(sibling->left)) &&
		 (_is_black(sibling->right))) {
		sibling->color = red;
		_delete_case1(tree, node->parent);
	} else {
		_delete_case3(tree, node);
	}
}

static void _delete_case1(RBTree *tree, RBNode *node)
{
	if(node->parent == NULL) {
		return;
	}

	RBNode *sibling = _sibling(node);
	if(sibling->color == red) {
		node->parent->color = red;
		sibling->color = black;
		if(node == node->parent->left) {
			_rotate_left(tree, node->parent);
		} else {
			_rotate_right(tree, node->parent);
		}
	}
	_delete_case2(tree, node);
}

static bool _delete_one_child(RBTree *tree, RBNode *node, RBNode *child)
{
	if(node == node->parent->left) {
		node->parent->left = child;
	} else if(node == node->parent->right) {
		node->parent->right = child;
	}
	child->parent = node->parent; 
	if((node->color == red) ||
		 (node->color == black && child->color == red)) {
		child->color = black;
	} else {
		_delete_case1(tree, child);
	}
	_rb_free_node(tree, node);
	al_free(node);
	return true;
}

bool rb_tree_remove(RBTree *tree, void *key)
{
	RBNode *node = _rb_tree_lookup(tree, tree->root, key);
	RBNode *child = NULL;
	RBNode *successor = NULL;
	if(!node) {
		return false;
	}
	
	// if I have no kid (trivial)
	if(!node->left && !node->right) {
		if(node->parent) {
			if(node == node->parent->left) {
				node->parent->left = NULL;
			} else if(node == node->parent->right) {
				node->parent->right = NULL;
			}
		} else {
			tree->root = NULL;
		}
		_rb_free_node(tree, node);
		al_free(node);
		return true;
	}
	
	// if I have 1 kid (the hard case)
	if(node->left && !node->right) {
		child = node->left;
	} else if(!node->left && node->right) {
		child = node->right;
	}

	if(child) {
		return _delete_one_child(tree, node, child);
	}

	// if I have two kids (the easy case)
	if(node->left && node->right) {
		successor = _successor(node);
		_rb_free_node(tree, node);
		node->key = successor->key;
		node->data = successor->data;
		
		if(successor->parent) {
			if(successor == successor->parent->left) {
				successor->parent->left = NULL;
			} else if(successor == successor->parent->right) {
				successor->parent->right = NULL;
			}
		}
		al_free(successor);
		return true;
	}
}

void rb_tree_preorder_print(RBNode *node)
{
	if(!node) {
		return;
	}

	//char *key = node->key;
	int key = *(int *)node->key;

	if(!node->parent) {
		printf("root: %d\ncolor: %s\n", key, node->color ? "red" : "black");
		printf("%d: %p\n", key, node);
		printf("%d->left: %p\n", key, node->left);
		printf("%d->right: %p\n", key, node->right);
		printf("%d->parent: %p\n", key, node->parent);
	} else if(node == node->parent->left) {
		printf("left: %d\ncolor: %s\n", key, node->color ? "red" : "black");
		printf("%d: %p\n", key, node);
		printf("%d->left: %p\n", key, node->left);
		printf("%d->right: %p\n", key, node->right);
		printf("%d->parent: %p\n", key, node->parent);
	} else {
		printf("right: %d\ncolor: %s\n", key, node->color ? "red" : "black");
		printf("%d: %p\n", key, node);
		printf("%d->left: %p\n", key, node->left);
		printf("%d->right: %p\n", key, node->right);
		printf("%d->parent: %p\n", key, node->parent);
	}
	rb_tree_preorder_print(node->left);
	rb_tree_preorder_print(node->right);
}

static void _rb_tree_inorder_print(RBNode *node)
{
	if(!node) {
		return;
	}
	rb_tree_inorder_print(node->left);
	printf("%s ", (char *)node->key);
	_rb_tree_inorder_print(node->right);
}

void rb_tree_inorder_print(RBNode *node)
{
	_rb_tree_inorder_print(node);
	printf("\n");
}

void rb_tree_delete(RBTree *tree)
{
	while(tree->root) {
		rb_tree_remove(tree, tree->root->key);
	}
	al_free(tree);
}

void rb_tree_ref(RBTree *tree)
{
	tree->ref_count++; // use mutex
}

void rb_tree_unref(RBTree *tree)
{
	tree->ref_count--;
	if(tree->ref_count < 1) {
		rb_tree_delete(tree);
	}
}

#if 0
void rb_tree_debug_print()
{
	if(!reversed) {
		debug_list = slist_reverse(debug_list);
	}
	SList *iter = debug_list;
	for(; iter != NULL; iter = iter->next) {
		char *key = (char *)((RBNode *)iter->data)->key;
		printf("%s: %p\n", key, iter->data);
		printf("%s->left: %p\n", key, ((RBNode *)iter->data)->left);
		printf("%s->right: %p\n", key, ((RBNode *)iter->data)->right);
		printf("%s->parent: %p\n", key, ((RBNode *)iter->data)->parent);
	}
}
#endif
