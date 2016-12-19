/* 
 isl_binary_heap.h - v1.0.0
 public domain library with priority queue implemented using indirect binary heap; 
 no warranty implied; use at your own risk

 author: Ilya Kolbin (iskolbin@gmail.com)
 url: github.com/iskolbin/isl_binary_heap

 Do this:
		#define ISL_BINARY_HEAP_IMPLEMENTATION
 before you include this file in *one* C or C++ file to create the implementation.
 
 LICENSE

 This software is dual-licensed to the public domain and under the following
 license: you are granted a perpetual, irrevocable license to copy, modify,
 publish, and distribute this file as you see fit.
*/

#ifndef ISLBH_INCLUDE_BINARY_HEAP_H_
#define ISLBH_INCLUDE_BINARY_HEAP_H_

#include <stddef.h>

#ifdef ISLBH_STATIC
#define ISLBH_DEF static
#else
#define ISLBH_DEF extern
#endif

typedef int (*islbh_cmp)(const void *, const void *);

typedef struct {
	const void *item;
	size_t index;
} islbh_node;

typedef struct {
	size_t length;
	size_t allocated;
	islbh_cmp cmp;
	islbh_node *nodes;
} islbh_binary_heap;

typedef enum {
	ISLBH_ERROR_OK,
	ISLBH_ERROR_BAD_ALLOC,
	ISLBH_ERROR_NOT_EXIST,
	ISLBH_ERROR_NULL_NODE,
} islbh_error;

#ifdef __cplusplus
extern "C" {
#endif

ISLBH_DEF islbh_binary_heap *islbh_create( size_t prealloc, islbh_cmp cmp );
ISLBH_DEF void islbh_destroy( islbh_binary_heap *self );
ISLBH_DEF islbh_node *islbh_enqueue( islbh_binary_heap *self, const void *item );
ISLBH_DEF const void *islbh_dequeue( islbh_binary_heap *self );
ISLBH_DEF const void *islbh_peek( islbh_binary_heap *self );
ISLBH_DEF islbh_error islbh_batchenq( islbh_binary_heap *self, const void *items, size_t itemsize, size_t count, islbh_node *nodes );
ISLBH_DEF void islbh_update( islbh_binary_heap *self, islbh_node *node );
ISLBH_DEF islbh_error islbh_remove( islbh_binary_heap *self, islbh_node *node );

#ifdef __cplusplus
}
#endif

#endif // ISLBH_INCLUDE_BINARY_HEAP_H_

#ifdef ISL_BINARY_HEAP_IMPLEMENTATION

#if !defined(ISLBH_MALLOC) && !defined(ISLBH_REALLOC) && !defined(ISLBH_FREE)
#include <stdlib.h>
#define ISLBH_MALLOC malloc
#define ISLBH_REALLOC realloc
#define ISLBH_FREE free
#elif defined(ISLBH_MALLOC) && defined(ISLBH_REALLOC) && defined(ISLBH_FREE)
#error "You have to define ISLBH_MALLOC, ISLBH_REALLOC and ISLBH_FREE to remove stdlib.h dependency"
#endif

islbh_binary_heap *islbh_create( size_t prealloc, islbh_cmp cmp ) {
	islbh_binary_heap *self = malloc( sizeof *self );
	if ( self != NULL ) {
		self->cmp = cmp;
		self->length = 0;
		self->nodes = ISLBH_MALLOC( prealloc * sizeof( islbh_node ));
		if ( self->nodes != NULL ) {
			self->allocated = prealloc;
		} else {
			ISLBH_FREE( self );
			self = NULL;
		}
	}	
	return self;
}

void islbh_destroy( islbh_binary_heap *self ) {
	if ( self != NULL ) {
		if ( self->nodes != NULL ) {
			ISLBH_FREE( self->nodes );
			self->nodes = NULL;
			self->allocated = 0;
			self->length = 0;
		}
		ISLBH_FREE( self );
	}
}

static islbh_error islbh__grow( islbh_binary_heap *self, size_t newalloc ) {
	islbh_node *newnodes = ISLBH_REALLOC( self->nodes, sizeof(islbh_node) * newalloc );
	if ( newnodes != NULL ) {
		self->allocated = newalloc;
		self->nodes = newnodes;
		return ISLBH_ERROR_OK; 
	} else {
		return ISLBH_ERROR_BAD_ALLOC;
	}
}

static void islbh__swap( islbh_binary_heap *self, size_t index1, size_t index2 ) {
	islbh_node tmp = self->nodes[index1];
	self->nodes[index1] = self->nodes[index2];
	self->nodes[index2] = tmp;
	self->nodes[index1].index = index1;
	self->nodes[index2].index = index2;
}

static size_t islbh__siftup( islbh_binary_heap *self, size_t index ) {
	size_t parent = (index-1) >> 1;
	while ( index > 0 && self->cmp( self->nodes[index].item, self->nodes[parent].item ) < 0 ) {
		islbh__swap( self, index, parent );
		index = parent;
		parent = (index-1) >> 1;	
	}
	return index;
}

static void islbh__siftdown_floyd( islbh_binary_heap *self, size_t index ) {
	size_t left = (index << 1) + 1;
	size_t right = left + 1;
	while ( left < self->length ) {
		size_t higher = ( right < self->length && self->cmp( self->nodes[right].item, self->nodes[left].item ) < 0 ) ? right : left;
		islbh__swap( self, index, higher );
		index = higher;
		left = (index << 1) + 1;
		right = left + 1;
	}
	islbh__siftup( self, index );
}

static void islbh__siftdown( islbh_binary_heap *self, size_t index ) {
	size_t left = (index << 1) + 1;
	size_t right = left + 1;
	while ( left < self->length ) {
		size_t higher = ( right < self->length && self->cmp( self->nodes[right].item, self->nodes[left].item ) < 0 ) ? right : left;
		if ( self->cmp( self->nodes[index].item, self->nodes[higher].item ) < 0 ) 
			break;
		islbh__swap( self, index, higher );
		index = higher;
		left = (index << 1) + 1;
		right = left + 1;
	}
}

static void islbh__floyd_algorithm( islbh_binary_heap *self ) {
	size_t n = self->length >> 1;
	size_t i;
	for ( i = 0; i < n; i++ ) {
		islbh__siftdown( self, n - i - 1 );
	}
}

islbh_node *islbh_enqueue( islbh_binary_heap *self, const void *item ) {
	size_t index = self->length;
	islbh_node node = {item, index};
	if ( self->allocated <= self->length ) {
		if ( islbh__grow( self, self->allocated * 2 ) != ISLBH_ERROR_OK ) {
			return NULL;
		}
	}
	self->nodes[index] = node;
	self->length++;
	index = islbh__siftup( self, index );
	return &self->nodes[index];
}

const void *islbh_dequeue( islbh_binary_heap *self ) {
	if ( self->length == 0 ) {
		return NULL;
	} else if ( self->length == 1 ) {
		self->length = 0;
		return self->nodes[0].item;
	} else {
		const void *item = self->nodes[0].item;
		islbh__swap( self, 0, --self->length );
		islbh__siftdown_floyd( self, 0 );
		return item;
	}
}

const void *islbh_peek( islbh_binary_heap *self ) {
	return self->nodes[0].item;
}

void islbh_update( islbh_binary_heap *self, islbh_node *node ) {
	islbh__siftdown( self, islbh__siftup( self, node->index ));
}	

islbh_error islbh_remove( islbh_binary_heap *self, islbh_node *node ) {
	if ( node == NULL ) {
		return ISLBH_ERROR_NULL_NODE;
	} else if ( self->length <= node->index || self->nodes + node->index != node ) {
		return ISLBH_ERROR_NOT_EXIST;
	} else {
		size_t index = node->index;
		size_t last = --self->length;
		if ( index != last ) {
			islbh__swap( self, index, last );
		}
		islbh_update( self, self->nodes+index );
		return ISLBH_ERROR_OK;
	}
}

islbh_error islbh_batchenq( islbh_binary_heap *self, const void *items, size_t itemsize, size_t count, islbh_node *nodes ) {
	size_t length = self->length;
	size_t allocated = self->allocated;
	while ( allocated < length + count ) {
		allocated *= 2;
	}
	if ( allocated > self->allocated && islbh__grow( self, allocated ) != ISLBH_ERROR_OK) {
		return ISLBH_ERROR_BAD_ALLOC;
	} else {
		size_t i;
		for ( i = 0; i < count; i++ ) {
			islbh_node node = {items + i*itemsize, length + i};
			self->nodes[length+i] = node;
		}
		if ( nodes != NULL ) {
			for ( i = 0; i < count; i++ ) {
				nodes[i] = self->nodes[length+i];
			}
		}
		self->length = length + count;
		islbh__floyd_algorithm( self );
	}
	return ISLBH_ERROR_OK;
}

#endif // ISL_BINARY_HEAP_IMPLEMENTATION
