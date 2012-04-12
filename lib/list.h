/*! List manipulation functions
 *
 * Double linked lists are used
 * List header points to first and last list element
 *
 */

#pragma once

/*! List element pointers */
typedef struct _list_h_
{
	struct _list_h_ *prev; /* pointer to previous list element */
	struct _list_h_ *next; /* pointer to next list element */
	void *object; /* pointer to object start */
}
list_h;

/*! list header type */
typedef struct _list_
{
	list_h *first;
	list_h *last;
}
list_t;

/*
 List element must be included in object that we want to put in list. Place and
 variable name is unimportant, but must be used when calling list functions.

 Simple list width only two elements will look like:

 struct some_object {
	 int something;
	 ...
	 list_h le1; // list element 1 for list1
	 ...
 } object1, object2;

 list_t some_list1;

 --when list is formed and both object are in list, data will lool like:

 some_list1.first = &object1.le1
 some_list1.last  = &object2.le1

 object1.le1.prev = NULL;			object2.le1.prev = &object2.le1;
 object1.le1.next = &object2.le1;		object2.le1.next = NULL;
 object1.le1.object = &object1			object2.le1.object = &object2;

 Same object can be in multiple list simultaneously if it have multiple list
 element data member (e.g. le2, le3).
*/

/* for static list elements initialization */
#define LIST_H_NULL	{ NULL, NULL, NULL }

/* for static list initialization (empty list) */
#define LIST_T_NULL	{ NULL, NULL }

#define FIRST	0	/* get first or last list element */
#define LAST	1

void list_init ( list_t *list );
void list_append ( list_t *list, void *object, list_h *hdr );
void list_prepend ( list_t *list, void *object, list_h *hdr );
void list_sort_add ( list_t *list, void *object, list_h *hdr,
				   int (*cmp) ( void *, void * ) );
void *list_get ( list_t *list, unsigned int flags );
void *list_remove ( list_t *list, unsigned int flags, list_h *ref );
void *list_get_next ( list_h *hdr );
