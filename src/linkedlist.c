#include <stdlib.h>
#include <stdio.h>
#include <error.h>
#include <string.h>

// #include "redblack.h" // Not necessary to include in .c
#include "linkedlist.h"


linkedlist *llinit(linkedlist *ll) {
  ll->head = NULL;
  ll->tail = NULL;
  //ll->search = NULL;
  //ll->compare = NULL;
  ll->deallocateDataMembers = NULL;
  return ll;
}

// Returns the number of nodes in a list
int lllength(linkedlist *list) {
  int count = 0;
  llnode *current = list->head;
  
  while (current != NULL) {
    count++;
    current = current->next;
  }
  return count;
}

void lltraverse(linkedlist *list) {
  llnode *current = list->head;
  
  while (current != NULL) {
    //printf("%s (%d time(s))\n", current->filename, current->count);
    current = current->next;
  }
}

llnode *llsearch(linkedlist *list, void *target, int (*compare) (void *, void *)) {
  // use the list's compare() function and check if result is 0
  //
  llnode *current = list->head;
  while (current != NULL) {
    if (compare(current->data, target) == 0) {
      //printf("Data found within list!\n");
      return current;
    }
    current = current->next; // Don't forget this!
  }
  return NULL; // data not found in list
}

llnode *initllnode(llnode *node, void *data, size_t data_size) {
  // Question: do we set node->data = data?
  // Or do we memcpy data into node->data?
  // If we memcopy, we need the size as a parameter
  // But if we don't memcopy, then we have to be careful freeing
  // because the data pointer mught not point to malloced space!!!!! 

  node->data = malloc(data_size);
  // Note: data will be casted as needed.
  memcpy(node->data, data, data_size);

  node->next = NULL;

  return node;

}

void llinsert(linkedlist *list, llnode *node) { // Or could pass data direclt and make node inside
  // insert node at end
  
  llnode *curr = list->head;
  llnode *prev = NULL;
  while (curr != NULL) {
    prev = curr;
    curr = curr->next;
  }
  //in the case where you insert at the front of the list
  if (prev == NULL) { // i.e. curr is NULL, so the while loop above is never entered
    node->next = curr; // curr is NULL
    list->head = node;
  }
  else {
    //in the case where new node goes at the end of the list
    node->next = curr;
    prev->next = node;
  }

  list->tail = node;

}


void lldeleteallnodes(linkedlist *list) {
  llnode *curr = list->head;
  llnode *temp;
  while (curr != NULL) {
    temp = curr->next; // next node

    list->deallocateDataMembers(curr->data);
    free(curr->data);
    free(curr);

    curr = temp;
  }
  list->head = NULL;
  list->tail = NULL;
}


// Deletes the node with address node from the list, and returns 0.
// If no such node exists in the list, returns -1.
int lldeletenode(linkedlist *list, llnode *node) {
  llnode *prev = NULL;
  llnode *curr = list->head;
  if(curr == NULL){
    return -1;
  } 
  while(curr != NULL && curr != node) { //
    prev = curr;
    curr = curr->next;
  } 
  if(curr == NULL){
    //item doesn't exist
    //printf("Didn't find this node in the list.\n");
    return -1;
  }
  // used to be an else if
  if(prev == NULL){ // TODO assert curr == list->head
    //item is in the head of the list
    //list->head = curr;
    //printf("Changing head node.\n");
    list->head = curr->next;
  }
  else{
    // item is in middle of the list
    prev->next = curr->next;
  }
 
 if (curr == list->tail) {
    // item is in the tail of the list
    //printf("Changing tail node\n");
    list->tail = prev; // could be null
  }

  if(curr != NULL) { // TODO this check is redundant
    //printf("Node found in list. Deleting.\n");
    list->deallocateDataMembers(curr->data); // used to call this function on curr (when it accepted a node instead of a void *)
    free(curr->data);
    free(curr);
    
  }
  return 0; 

}

// returns 0 on success, -1 on not finding the item 
int lldeletebycompare(linkedlist *list, void *data, int (*compare) (void *, void *)) {
  llnode *prev = NULL;
  llnode *curr = list->head;
  if(curr == NULL){
    return -1;
  } 
  while(curr != NULL && compare(curr->data, data) != 0) { // what if the node we want is
    prev = curr;					// the tail? That's okay; we won't
    curr = curr->next;					// enter the loop, so curr won't become
  } 							// null.
  if(curr == NULL){
    //item doesn't exist
    printf("Didn't find item in list.\n");
    return -1;
  }
  // used to be an else if
  if(prev == NULL){ // TODO assert curr == list->head
    //item is in the head of the list
    //list->head = curr;
    printf("Changing head node.\n");
    list->head = curr->next;
  }
  else{
    // item is in middle of the list
    prev->next = curr->next;
  }
  if (curr == list->tail) {
    // item is in the tail of the list
    printf("Changing tail node\n");
    list->tail = prev; // could be null
  }

  if(curr != NULL) { // TODO this check is redundant.
    printf("Item found in list. Deleting.\n");
    list->deallocateDataMembers(curr->data); // used to call this function on curr (when it accepted a node instead of a void *)

    free(curr->data);
    free(curr);
    
  }
  return 0;
}

int lldeletehead(linkedlist *list) {
  int result = lldeletenode(list, list->head);
  return result;
}

/*
void llinsert_old(linkedlist *list, char *filename) {
  //check if linked list is empty
  if (list->head == NULL) {
    llnode *newNode = malloc(sizeof(llnode));
    newNode = initllnode(newNode, filename);
  }
  llnode *curr = list->head;
  llnode *prev = NULL;
  while (curr != NULL) {
    if (compare(curr->filename, filename) < 0){
      //update pointers
      prev = curr;
      curr = curr->next;
    }
    else if (compare(curr->filename, filename) > 0) {
      //first time we encounter a llnode in the list with a filename greater than inputted filename
      break;
    }
    else {
      //in this case, we encounter a llnode with the same filename as the inputted filename, so current increment the llnode's count	  
      curr->count++;
      return;
    }
  }
  //in the case where you insert at the front of the list
  if (prev == NULL) {
    llnode *newNode = malloc(sizeof(llnode));
    newNode = initllnode(newNode, filename);
    newNode->next = curr;
    list->head = newNode;
  }
  else {
    //in the case where new node goes in the middle or end of the list
    llnode *newNode = malloc(sizeof(llnode));
    newNode = initllnode(newNode, filename);
    newNode->next = curr;
    prev->next = newNode;
  }
}
void push(linkedlist *list, char *filename) {
  llnode *newNode = malloc(sizeof(llnode));
  newNode->filename = filename;
  newNode->next = list->head;
  list->head = newNode;
  newNode->count = 1;
}
*/


/*
llnode *mergeSort(llnode *head) {
  if (head == NULL || head->next == NULL) {
    return head;
  }
  else {
    llnode *middle = getMiddleNode(head);
    llnode *secondHalf = middle->next;
    middle->next = NULL;
    llnode *test = merge(mergeSort(head), mergeSort(secondHalf));
    return test;
  }
  return head;
}

llnode *getMiddleNode(llnode *head) {
  if (head == NULL) {
    return head;
  }
  llnode *slow = head; //slow traverses through the list half the speed that fast traverses through the list
  llnode *fast = head; //fast traverses through the list twice the speed that slow traverses through the list
  //the point of the loop below is to move slow one node at a time and fast two nodes at a time with each iteration
  //so that fast will be either the last or second to last node in the list while slow is the middle node of the list
  while (fast->next != NULL && fast->next->next != NULL) {
    slow = slow->next;
    fast = fast->next->next;
  }
  return slow;
}

llnode *merge(llnode *a, llnode *b) {
  llnode *dummyHead = (llnode *)malloc(sizeof(llnode));
  dummyHead = initllnode(dummyHead, ""); //dummyhead is just a node variable that is needed for reference purposes (the node after dummyHead is the actual head dummy)
  llnode *curr = dummyHead; //curr is the pointer that moves through the new merged list being created, always pointing to the last node in the new list
  //traverse through a and b until reaching the end of one of the lists, comparing elements from each to decide the order in which to add elements to the new list
  
  while (a != NULL && b != NULL) {
    if (a->count > b->count) {
      curr->next = a;
      a = a->next;
    }
    else if (a->count < b->count) {
      curr->next = b;
      b = b->next;
    }
    else {
      //if the counts are equal, then let the file names decide the order
      if (compare(a->filename, b->filename) <= 0) {
        curr->next = a;
        a = a->next;
      }
      else {
        curr->next = b;
        b = b->next;
      }
    }
    curr = curr->next;
  }
  //append the rest of the remaining list, not the list that traversed all the way through
  if (a == NULL) {
    curr->next = b;
  }
  else {
    curr->next = a;
  }
  
  return dummyHead->next;
}

*/






