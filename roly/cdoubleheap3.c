/*
Copyright (c) 2011 J. David Lee. All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are
met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.  
*/


/*
 * The number of children has a maximum of 8 due to the manual loop-
 * unrolling used in the code below. 
 */
#define NUM_CHILDREN 8

// Minimum of two numbers. 
#define min(a, b) (((a) < (b)) ? (a) : (b))

// Find indices of parent, first, and last child. 
#define P_IDX(i) ((i) - 1) / NUM_CHILDREN
#define FC_IDX(i) NUM_CHILDREN * (i) + 1
#define LC_IDX(i) (i) / NUM_CHILDREN + NUM_CHILDREN - 1


struct mm_node {
  npy_uint32   small;   // 1 if the node is in the small heap. 
  npy_uint64   idx;     // The node's index in the heap array.
  npy_float64  val;     // The node's value. 
  struct mm_node *next; // The next node in order of insertion. 
};


struct mm_handle {
  int              odd;       // 1 if the window size is odd, 0 otherwise.
  npy_uint64       n_s;       // The number of elements in the min heap.
  npy_uint64       n_l;       // The number of elements in the max heap. 
  struct mm_node **s_heap;    // The min heap.
  struct mm_node **l_heap;    // The max heap.
  struct mm_node **nodes;     // All the nodes. s_heap and l_heap point
                              // into this array. 
  struct mm_node  *node_data; // Pointer to memory location where nodes live. 
  struct mm_node  *first;     // The node added first to the list of nodes. 
  struct mm_node  *last;      // The last (most recent) node added. 
  
  // Most nodes are leaf nodes, therefore it makes sense to have a
  // quick way to check if a node is a leaf to avoid processing.
  npy_uint64 s_first_leaf; // First leaf index in the small heap.
  npy_uint64 l_first_leaf; // First leaf index in the large heap.
};


/*
 * Return the smallest child node of the given node, if it doesn't exist, 
 * return the node itself. 
 */
inline struct mm_node *get_smallest_child(struct mm_node **heap,
                                          npy_uint64       len,
                                          struct mm_node  *node) {
  npy_uint64 idx = node->idx;
  npy_int64 i_first = FC_IDX(idx);
  npy_int64 num_children = min(i_first + NUM_CHILDREN, len) - i_first;

  struct mm_node *ch;
  switch(num_children) {
  case  8: ch = heap[i_first +  7]; if(ch->val < node->val) { node = ch; }
  case  7: ch = heap[i_first +  6]; if(ch->val < node->val) { node = ch; }
  case  6: ch = heap[i_first +  5]; if(ch->val < node->val) { node = ch; }
  case  5: ch = heap[i_first +  4]; if(ch->val < node->val) { node = ch; }
  case  4: ch = heap[i_first +  3]; if(ch->val < node->val) { node = ch; }
  case  3: ch = heap[i_first +  2]; if(ch->val < node->val) { node = ch; }
  case  2: ch = heap[i_first +  1]; if(ch->val < node->val) { node = ch; }
  case  1: ch = heap[i_first     ]; if(ch->val < node->val) { node = ch; }
  }
  return node;
}


/*
 * Return the largest child node of the given node, if it doesn't exist, 
 * return the node itself. 
 */
inline struct mm_node *get_largest_child(struct mm_node **heap,
                                         npy_uint64       len,
                                         struct mm_node  *node) {
  npy_int64 i_first = NUM_CHILDREN * node->idx + 1;
  npy_int64 num_children = min(i_first + NUM_CHILDREN, len) - i_first;

  struct mm_node *ch;  
  switch(num_children) {
  case  8: ch = heap[i_first +  7]; if(ch->val > node->val) { node = ch; }
  case  7: ch = heap[i_first +  6]; if(ch->val > node->val) { node = ch; }
  case  6: ch = heap[i_first +  5]; if(ch->val > node->val) { node = ch; }
  case  5: ch = heap[i_first +  4]; if(ch->val > node->val) { node = ch; }
  case  4: ch = heap[i_first +  3]; if(ch->val > node->val) { node = ch; }
  case  3: ch = heap[i_first +  2]; if(ch->val > node->val) { node = ch; }
  case  2: ch = heap[i_first +  1]; if(ch->val > node->val) { node = ch; }
  case  1: ch = heap[i_first     ]; if(ch->val > node->val) { node = ch; }
  }
  return node;
}


/* 
 * Swap two nodes. 
 */ 
inline void swap_nodes(struct mm_node **heap, 
                       struct mm_node  *node1, 
                       struct mm_node  *node2) {
  npy_uint64 idx1 = node1->idx;
  npy_uint64 idx2 = node2->idx;
  heap[idx1] = node2;
  heap[idx2] = node1;
  node1->idx = idx2;
  node2->idx = idx1;
}


/*
 * Move the given node up through the heap to the appropriate position. 
 */
inline void move_up_small(struct mm_node **heap,
                          npy_uint64       len,
                          struct mm_node  *node) {
  struct mm_node *parent;
  while(node->idx !=  0) {
    parent = heap[P_IDX(node->idx)];
    if(node->val > parent->val) {
      swap_nodes(heap, node, parent);
    } else {
      break;
    }
  }
}


/*
 * Move the given node down through the heap to the appropriate position. 
 */ 
inline void move_down_small(struct mm_node **heap,
                            npy_uint64       len,
                            struct mm_node  *node) {
  struct mm_node *child = get_largest_child(heap, len, node);
  while(node->val < child->val) {
    swap_nodes(heap, node, child);
    child = get_largest_child(heap, len, node);
  }
}


/*
 * Move the given node down through the heap to the appropriate
 * position.
 */ 
inline void move_down_large(struct mm_node **heap,
                            npy_uint64       len,
                            struct mm_node  *node) {
  struct mm_node *parent;
  while(node->idx != 0) {
    parent = heap[P_IDX(node->idx)];
    if(node->val < parent->val) {
      swap_nodes(heap, node, parent);
    } else {
      break;
    }
  }
}


/*
 * Move the given node down through the heap to the appropriate position. 
 */ 
inline void move_up_large(struct mm_node **heap,
                          npy_uint64       len,
                          struct mm_node  *node) {
  struct mm_node *child = get_smallest_child(heap, len, node);
  while(node->val > child->val) {
    swap_nodes(heap, node, child);
    child = get_smallest_child(heap, len, node);
  }
}


/*
 * Rebalance the heaps.
 */
inline void swap_heap_heads(struct mm_handle *mm) {
  struct mm_node *s_node = mm->s_heap[0];
  struct mm_node *l_node = mm->l_heap[0];
  l_node->small = 1;
  s_node->small = 0;
  mm->s_heap[0] = l_node;
  mm->l_heap[0] = s_node;
  move_up_large(mm->l_heap, mm->n_l, s_node);
  move_down_small(mm->s_heap, mm->n_s, l_node);
}


/*
 *   ----------------
 *   Public functions
 *   ----------------
 */


/* 
 * Construct the double heap with the given total number of values. 
 * 
 * Arguments:
 * len -- The total number of values in the double heap. 
 *
 * Return: The mm_handle structure, uninitialized. 
 */
struct mm_handle *mm_new(const npy_uint64 len) {
  struct mm_handle *mm = malloc(sizeof(struct mm_handle));
  mm->odd = len % 2;
  mm->n_l = 0;
  mm->n_s = 0;

  mm->nodes = malloc(len * sizeof(struct mm_node*));
  mm->node_data = malloc(len * sizeof(struct mm_node));

  mm->s_heap = mm->nodes;
  mm->l_heap = &mm->nodes[len/2 + len % 2];
  
  return mm;
}


/*
 * Update the running median with a new value. 
 */
void mm_update(struct mm_handle *mm, npy_float64 val) {
  struct mm_node *new_node = mm->first;

  // Local variables.
  npy_uint64 idx = new_node->idx;
  struct mm_node **l_heap = mm->l_heap;
  struct mm_node **s_heap = mm->s_heap;

  // Replace value of first inserted node, and update first, last.
  new_node->val = val;
  mm->first = mm->first->next; 
  mm->last->next = new_node;
  mm->last = new_node;

  // In small heap.
  if(new_node->small) {

    // Move the node up.
    if(idx != 0 && val > s_heap[P_IDX(idx)]->val) {
      move_up_small(s_heap, mm->n_s, new_node);
    } 
    
    // Move the node down. 
    else if(idx < mm->s_first_leaf) {
      move_down_small(s_heap, mm->n_s, new_node);
    }

    // Swap heads if necessary. 
    if(val > l_heap[0]->val) {
      swap_heap_heads(mm);
    }
  }

  // In large heap. 
  else {

    // Move the node down.
    if(idx != 0 && val < l_heap[P_IDX(idx)]->val) {
      move_down_large(l_heap, mm->n_l, new_node);
    } 
    
    // Move the node up.
    else if(idx < mm->l_first_leaf) {
      move_up_large(l_heap, mm->n_l, new_node);
    }
    
    // Swap heads if necessary. 
    if(val < s_heap[0]->val) {
      swap_heap_heads(mm);
    }
  }
}


/*
 * Insert initial values into the double heap structure. 
 * 
 * Arguments:
 * mm  -- The double heap structure.
 * idx -- The index of the value running from 0 to len - 1. 
 * val -- The value to insert. 
 */
void mm_insert_init(struct mm_handle *mm, npy_float64 val) {
  struct mm_node *node;

  // The first node. 
  if(mm->n_s == 0) {
    node = mm->s_heap[0] = &mm->node_data[0];
    node->small = 1;
    node->idx   = 0;
    node->val   = val;
    node->next  = mm->l_heap[0];

    mm->n_s = 1;
    mm->first = mm->last = node;
    mm->s_first_leaf = 0;
  } 
  
  // Nodes after the first. 
  else {

    // Add to the large heap. 
    if(mm->n_s > mm->n_l) {
      node = mm->l_heap[mm->n_l] = &mm->node_data[mm->n_s + mm->n_l];
      node->small = 0;
      node->idx   = mm->n_l;
      node->next  = mm->first;

      mm->first = node;
      ++mm->n_l;
      mm->l_first_leaf = ceil((mm->n_l - 1) / (double)NUM_CHILDREN);
      mm_update(mm, val);
    } 
    
    // Add to the small heap.
    else {
      node = mm->s_heap[mm->n_s] = &mm->node_data[mm->n_s + mm->n_l];
      node->small = 1;
      node->idx   = mm->n_s;
      node->next  = mm->first;

      mm->first = node;
      ++mm->n_s;
      mm->s_first_leaf = ceil((mm->n_s - 1) / (double)NUM_CHILDREN);
      mm_update(mm, val);
    }
  }
}


/*
 * Return the current median value. 
 */
inline npy_float64 mm_get_median(struct mm_handle *mm) {
  if(mm->odd) {
    return mm->s_heap[0]->val;
  } else {
    return (mm->s_heap[0]->val + mm->l_heap[0]->val) / 2.0;
  }
}


/*
 * Print the two heaps to the screen.
 */
void mm_dump(struct mm_handle *mm) {
  printf("\n\nFirst: %f\n", (double)mm->first->val);
  printf("Last: %f\n", (double)mm->last->val);
  
  npy_uint64 i;
  printf("\n\nSmall heap:\n");
  for(i = 0; i < mm->n_s; ++i) {
    printf("%i %f\n", (int)mm->s_heap[i]->idx, mm->s_heap[i]->val);
  }
  
  printf("\n\nLarge heap:\n");
  for(i = 0; i < mm->n_l; ++i) {
    printf("%i %f\n", (int)mm->l_heap[i]->idx, mm->l_heap[i]->val);
  }
}


/*
 * Free memory allocated in mm_new.
 */
inline void mm_free(struct mm_handle *mm) {
  free(mm->node_data);
  free(mm->nodes);
  free(mm);
}
