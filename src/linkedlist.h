#ifndef LINKEDLIST_H
#define LINKEDLIST_H

// #include "redblack.h"
// Not needed anymore, since now we have a red black tree of
// linked lists, and not vice versa
// #include "comparestrings.h" // don't forget this

typedef struct _linkedlist linkedlist;
typedef struct _llnode llnode;

struct _linkedlist {
  llnode *head;
  llnode *tail;
  // search function
  //llnode *(*search) (void *); // this is the search function
  //int (*compare) (void *, void *);
  void (*deallocateDataMembers)(void *); // TODO should this belong to each node instead?
  // Changed this function from accepting a node * to accepting a void *
};


// the node definition
struct _llnode {
  llnode *next;
  void *data; // generic!
      // e.g. a struct
};

linkedlist *llinit(linkedlist *);

// Returns the number of nodes in a list
int lllength(linkedlist *);

void lltraverse(linkedlist *);

llnode *initllnode(llnode *, void *, size_t);

llnode *llsearch(linkedlist *, void *, int (*) (void *, void *));

void llinsert(linkedlist *, llnode *);

void lldeleteallnodes(linkedlist *);

int lldeletenode(linkedlist *, llnode *);

int lldeletebycompare(linkedlist *, void *, int (*) (void *, void *));

int lldeletehead(linkedlist *);

void push(linkedlist *, void *);

llnode *mergeSort(llnode *);

llnode *getMiddleNode(llnode *);

llnode *merge(llnode *, llnode *);
#endif
