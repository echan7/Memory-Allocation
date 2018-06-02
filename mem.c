
////////////////////////////////////////////////////////////////////////////////
// Main File:        mem.c
// This File:        mem.c
// Other Files:      (name of all other files if any)
// Semester:         CS 354 Fall 2016
//
// Author:           Eric Chan
// Email:            echan7@wisc.edu
// CS Login:         echan
//
//////////////////////////// 80 columns wide ///////////////////////////////////
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "mem.h"

/* this structure serves as the header for each block */
typedef struct block_hd{
	/* The blocks are maintained as a linked list */
	/* The blocks are ordered in the increasing order of addresses */
	struct block_hd* next;

	/* The size_status field is the size of the payload+padding and is always a multiple of 4 */
	/* ie, last two bits are always zero - can be used to store other information*/
	/* LSB = 0 => free block */
	/* LSB = 1 => allocated/busy block */

	/* So for a free block, the value stored in size_status will be the same as the block size*/
	/* And for an allocated block, the value stored in size_status will be one more than the block size*/

	/* The value stored here does not include the space required to store the header */

	/* Example: */
	/* For a block with a payload of 24 bytes (ie, 24 bytes data + an additional 8 bytes for header) */
	/* If the block is allocated, size_status should be set to 25, not 24!, not 23! not 32! not 33!, not 31! */
	/* If the block is free, size_status should be set to 24, not 25!, not 23! not 32! not 33!, not 31! */
	int size_status;

}block_header;

/* Global variable - This will always point to the first block */
/* ie, the block with the lowest address */
block_header* list_head = NULL;
/* Helper Functions to help simplify the program */
int isFree(block_header* meow);
void setAlloc(block_header* meow);
void setFree(block_header* meow);

/* Function used to Initialize the memory allocator */
/* Not intended to be called more than once by a program */
/* Argument - sizeOfRegion: Specifies the size of the chunk which needs to be allocated */
/* Returns 0 on success and -1 on failure */
int Mem_Init(int sizeOfRegion)
{
	int pagesize;
	int padsize;
	int fd;
	int alloc_size;
	void* space_ptr;
	static int allocated_once = 0;

	if(0 != allocated_once)
	{
		fprintf(stderr,"Error:mem.c: Mem_Init has allocated space during a previous call\n");
		return -1;
	}
	if(sizeOfRegion <= 0)
	{
		fprintf(stderr,"Error:mem.c: Requested block size is not positive\n");
		return -1;
	}

	/* Get the pagesize */
	pagesize = getpagesize();

	/* Calculate padsize as the padding required to round up sizeOfRegio to a multiple of pagesize */
	padsize = sizeOfRegion % pagesize;
	padsize = (pagesize - padsize) % pagesize;

	alloc_size = sizeOfRegion + padsize;

	/* Using mmap to allocate memory */
	fd = open("/dev/zero", O_RDWR);
	if(-1 == fd)
	{
		fprintf(stderr,"Error:mem.c: Cannot open /dev/zero\n");
		return -1;
	}
	space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (MAP_FAILED == space_ptr)
	{
		fprintf(stderr,"Error:mem.c: mmap cannot allocate space\n");
		allocated_once = 0;
		return -1;
	}

	allocated_once = 1;

	/* To begin with, there is only one big, free block */
	list_head = (block_header*)space_ptr;
	list_head->next = NULL;
	/* Remember that the 'size' stored in block size excludes the space for the header */
	list_head->size_status = alloc_size - (int)sizeof(block_header);

	return 0;
}


/* Function for allocating 'size' bytes. */
/* Returns address of allocated block on success */
/* Returns NULL on failure */
/* Here is what this function should accomplish */
/* - Check for sanity of size - Return NULL when appropriate */
/* - Round up size to a multiple of 4 */
/* - Traverse the list of blocks and allocate the best free block which can accommodate the requested size */
/* -- Also, when allocating a block - split it into two blocks when possible */
/* Tips: Be careful with pointer arithmetic */
void* Mem_Alloc(int size)
{
	if(size <= 0){
		return NULL;
	}
	// Check size and round it up to a multiple of 4
	int padsize = 0;
	/* keep track of padsize to round size to multiple of 4*/
	if(size % 4 != 0){
		padsize = size % 4;
		padsize = (4-padsize) % 4;
		size = size + padsize;
	}	
	// Search for the best fit block in the free list
	block_header *blockToCheck = list_head;
	block_header *blockToKeep = NULL;
	/* to check whether the best block to fund */
	int found = 0; 
	while(blockToCheck != NULL){
		/* if block size status is exactly the size wanted to alloc, alloc and return it */	
		if(blockToCheck->size_status == size){
			blockToKeep = blockToCheck;
			setAlloc(blockToKeep);
			found = 1;
			return (void*) (blockToKeep + sizeof(block_header));
		/*if block is freed and is more than size keep the block*/
		}else if(isFree(blockToCheck)){
			if(blockToCheck->size_status > size){
				if(blockToKeep == NULL){
				/*keep the first block the meets the requirements */
				blockToKeep = blockToCheck;
				found = 1;
			/* if the keep block size is bigger than current check block and > size,
			   reset keepblock */
			}else if(blockToCheck->size_status < blockToKeep->size_status){
				blockToKeep = blockToCheck;
				found = 1;
				}
			}
		}	
		/* est check block to next block, repeat while loop */
		blockToCheck = blockToCheck->next;
	}
	
	/* if cant find any suitable block return NULL */
	if(found == 0){
		return NULL;
	}
	// If a block is found, check to see if we can split it,
	// i.e it has space leftover for a new block(header + payload)
	int freeSpace = blockToKeep->size_status - size;
	int minBlock = 4+sizeof(block_header);	
	int padSize = 0;
	/* compare free space and min block, if free space is big enug as min block split into another
	   free block */
	if(freeSpace >= minBlock){
		/* assign new block address to freespace */
		block_header* newBlock = (block_header*) ((void*) blockToKeep + sizeof(block_header) + size);
	// If split, update the size of the resulting blocks
		newBlock->next = blockToKeep->next; 
		blockToKeep->next = newBlock;
		newBlock->size_status = freeSpace - sizeof(block_header);
	// Mark the allocated block and return it 
	}else if(freeSpace < minBlock){
		/* otherwise if freespace is not big enuf to be split, keep it as padsize */
		padSize = freeSpace;
	}
	/* remember to add padsize of freeSpace */
	blockToKeep->size_status = size + padSize;
	setAlloc(blockToKeep);
	/* return address to pay load not headre */
	return (void*)(blockToKeep + sizeof(block_header));
}

/* Function for freeing up a previously allocated block */
/* Argument - ptr: Address of the block to be freed up */
/* Returns 0 on success */
/* Retur -1 on failure */
/* Here is what this function should accomplish */
/* - Return -1 if ptr is NULL */
/* - Return -1 if ptr is not pointing to the first byte of a busy block */
/* - Mark the block as free */
/* - Coalesce if one or both of the immediate neighbours are free */
int Mem_Free(void *ptr)
{
	// Check if the pointer is pointing to the start of the payload of an allocated block
	if(ptr == NULL){
		return -1;
	}
	block_header *curr = list_head;
	block_header *prev = NULL;
	int found = 0;

	//check whether ptr is pointing to the first node
	/* assign a block to payload of a block */
	void *listhead = (void*)(curr + sizeof(block_header));
	/* if the address is the same as ptr */
	if(listhead == ptr){
		/* conlanfirm found to 1 */
		found = 1;
		/* set block to free and check whether next block is free to coalesce */
		if(isFree(curr)){
			return -1;
		}else if(!isFree(curr)){
			setFree(curr);
			if(isFree(curr->next)){
				/* remember to add size of block header as it is hiddern */
				curr->size_status = curr->size_status + sizeof(block_header) + (curr->next)->size_status;
				curr->next = (curr->next)->next;
			}
			/* return done yay but what if ptr is not referring to first block
			   continue next the method woot */
			return 0;
		}
	}

	//search for ptr in list
	while(curr->next!= NULL && found == 0){
		/* always keep track of the block prev and after curr hehe */
		prev = curr;
		curr = curr->next;
		void *listCheck = (void*) (curr + sizeof(block_header));
		if(listCheck == ptr){
			/* if its already freed, just return falled */
			if(isFree(curr)){
				return -1;
			}
			found = 1;
		}
	}
	// If so, free it.	
	if(found == 0){
		return -1;
	}else if(found == 1){
		if(!isFree(curr)){
			setFree(curr);
		}
	}
	// Check the blocks to the left and right to see if the block can be coalesced
	// remember to add hidden size of block header for any block to coalesce
	if(isFree(prev)){
		if(isFree(curr->next)){
			prev->size_status = prev->size_status + sizeof(block_header) + curr->size_status + sizeof(block_header) + (curr->next)->size_status;
			// set the new free block next to whatever next
			prev->next = (curr->next)->next;
		}else{
			prev->size_status = prev->size_status + sizeof(block_header) + curr->size_status;
			//update next
			prev->next = curr->next;
		} 
	}else if(isFree(curr->next)){
		curr->size_status = curr->size_status + sizeof(block_header) + (curr->next)->size_status;
		//update next
		curr->next = (curr->next)->next;
	}
	// with either or both of them
	return 0;
}

/* Function to be used for debug */
/* Prints out a list of all the blocks along with the following information for each block */
/* No.      : Serial number of the block */
/* Status   : free/busy */
/* Begin    : Address of the first useful byte in the block */
/* End      : Address of the last byte in the block */
/* Size     : Size of the block (excluding the header) */
/* t_Size   : Size of the block (including the header) */
/* t_Begin  : Address of the first byte in the block (this is where the header starts) */
void Mem_Dump()
{
	int counter;
	block_header* current = NULL;
	char* t_Begin = NULL;
	char* Begin = NULL;
	int Size;
	int t_Size;
	char* End = NULL;
	int free_size;
	int busy_size;
	int total_size;
	char status[5];

	free_size = 0;
	busy_size = 0;
	total_size = 0;
	current = list_head;
	counter = 1;
	fprintf(stdout,"************************************Block list***********************************\n");
	fprintf(stdout,"No.\tStatus\tBegin\t\tEnd\t\tSize\tt_Size\tt_Begin\n");
	fprintf(stdout,"---------------------------------------------------------------------------------\n");
	while(NULL != current)
	{
		t_Begin = (char*)current;
		Begin = t_Begin + (int)sizeof(block_header);
		Size = current->size_status;
		strcpy(status,"Free");
		if(Size & 1) /*LSB = 1 => busy block*/
		{
			strcpy(status,"Busy");
			Size = Size - 1; /*Minus one for ignoring status in busy block*/
			t_Size = Size + (int)sizeof(block_header);
			busy_size = busy_size + t_Size;
		}
		else
		{
			t_Size = Size + (int)sizeof(block_header);
			free_size = free_size + t_Size;
		}
		End = Begin + Size - 1;
		fprintf(stdout,"%d\t%s\t0x%08lx\t0x%08lx\t%d\t%d\t0x%08lx\n",counter,status,(unsigned long int)Begin,(unsigned long int)End,Size,t_Size,(unsigned long int)t_Begin);
		total_size = total_size + t_Size;
		current = current->next;
		counter = counter + 1;
	}
	fprintf(stdout,"---------------------------------------------------------------------------------\n");
	fprintf(stdout,"*********************************************************************************\n");

	fprintf(stdout,"Total busy size = %d\n",busy_size);
	fprintf(stdout,"Total free size = %d\n",free_size);
	fprintf(stdout,"Total size = %d\n",busy_size+free_size);
	fprintf(stdout,"*********************************************************************************\n");
	fflush(stdout);
	return;
}

int isFree(block_header* meow)
{
	if(meow->size_status % 4 == 0){
		return 1;
	}else if(meow->size_status % 4 == 1){
		return 0;
	}
  return 0;
}

void  setAlloc(block_header* meow)
{	
	if(meow->size_status % 4 == 1){
		printf("its already allocated bitch");
	}else if(meow->size_status % 4 == 0){
		meow->size_status += 1;
	}	
 }

void setFree(block_header* meow)
{
	if(meow->size_status % 4 == 0){
		printf("its already freed bitch");
	}else if(meow->size_status % 4 == 1){
		meow->size_status -= 1;
	}
}
