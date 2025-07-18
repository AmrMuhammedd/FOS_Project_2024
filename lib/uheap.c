#include <inc/lib.h>

int num_of_processes_in_user = 0;
uint32 last_free_place_user = 0;
int first_time_allocating_user = 1;

//uint32 allocating = 0;

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
uint32 SearchUheapFF(uint32 size, int checkMark){
	int num_of_required_pages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	int continious_page_counter = 0;
	uint32 start_page = 0;

	if (last_free_place_user == 0 || first_time_allocating_user == 0){
		for (uint32 i = (uint32) myEnv->Hard_limit + PAGE_SIZE; i < USER_HEAP_MAX; i += PAGE_SIZE) {
			if (continious_page_counter == num_of_required_pages) {
				break;
			}

			int numOfPagesAfter = 0;
			int check_current_page;
			if (checkMark == 1){
				check_current_page = sys_check_marked_page(i, &numOfPagesAfter);
			}else{
				check_current_page = sys_check_shared_allocated_page(i, &numOfPagesAfter, 0);
			}


			if (check_current_page == 1) {
				continious_page_counter = 0;
				start_page = 0;
				if (numOfPagesAfter == 0){
					continue;
				}
				uint32 marked_size = (numOfPagesAfter) * (PAGE_SIZE);
				i += marked_size - PAGE_SIZE;
				continue;
			}

			if (start_page == 0) {
				start_page = i;
			}
			continious_page_counter++;
		}
	}else {
		start_page = last_free_place_user;

		if (((USER_HEAP_MAX - start_page) / PAGE_SIZE) >= num_of_required_pages){
			continious_page_counter = num_of_required_pages;
		}
	}


	if (continious_page_counter != num_of_required_pages || size > (USER_HEAP_MAX - start_page)) {
		return 0;
	}
	return start_page;
}
//=============================================
// [1] CHANGE THE BREAK LIMIT OF THE USER HEAP:
//=============================================
/*2023*/
void* sbrk(int increment) {
	return (void*) sys_sbrk(increment);
}

//=================================
// [2] ALLOCATE SPACE IN USER HEAP:
//=================================
void* malloc(uint32 size) {
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	if (size == 0)
		return NULL;
	//==============================================================
	//TODO: [PROJECT'24.MS2 - #12] [3] USER HEAP [USER SIDE] - malloc()
	// Write your code here, remove the panic and write your code
	//panic("malloc() is not implemented yet...!!");
	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
		return alloc_block_FF(size);
	} else {
		uint32 start_page = SearchUheapFF(size, 1);

		if (start_page == 0){
			return NULL;
		}

		sys_allocate_user_mem(start_page, size);

		int num_of_required_pages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
		num_of_processes_in_user++;
		last_free_place_user = start_page + (num_of_required_pages * PAGE_SIZE);

		return (void*) start_page;
	}
	return NULL;

//Use sys_isUHeapPlacementStrategyFIRSTFIT() and	sys_isUHeapPlacementStrategyBESTFIT()
//to check the current strategy

}

//=================================
// [3] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address) {
	//TODO: [PROJECT'24.MS2 - #14] [3] USER HEAP [USER SIDE] - free()
	// Write your code here, remove the panic and write your code
	// panic("free() is not implemented yet...!!");
	uint32 va = (uint32) virtual_address;

	if (va >= USER_HEAP_START && va <= (uint32) (myEnv->Break)) {
		free_block(virtual_address);
		return;
	} else if (va >= ((uint32) (myEnv->Hard_limit) + PAGE_SIZE)&& va <= USER_HEAP_MAX) {
		int numOfMarkedPagesAfter = 0;
		int page_is_marked = sys_check_marked_page(va, &numOfMarkedPagesAfter);

		if (page_is_marked == 0){
			return;
		}

		for (uint32 i = va; i < va + (numOfMarkedPagesAfter * PAGE_SIZE); i += PAGE_SIZE) {
			int page_num = (i - USER_HEAP_START) / PAGE_SIZE;
		}

		sys_free_user_mem(va, numOfMarkedPagesAfter * PAGE_SIZE);

		num_of_processes_in_user--;
		last_free_place_user = 0;
		first_time_allocating_user = 0;

	} else {
		panic("invalid address");
	}

}

//=================================
// [4] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable) {
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	if (size == 0)
		return NULL;
	//==============================================================
	//TODO: [PROJECT'24.MS2 - #18] [4] SHARED MEMORY [USER SIDE] - smalloc()
	// Write your code here, remove the panic and write your code
	//panic("smalloc() is not implemented yet...!!");
	if (size > USER_HEAP_MAX - (uint32)(myEnv->Hard_limit + PAGE_SIZE)){
		return NULL;
	}


	uint32 start_page = SearchUheapFF(size, 0);

	if (start_page == 0){
		return NULL;
	}

//	while(xchg(&allocating, 1) != 0);

	int res = sys_createSharedObject(sharedVarName, size, isWritable,(void*) start_page);

	if (res == E_SHARED_MEM_EXISTS || res == E_NO_SHARE) {
		return NULL;
	}

	int num_of_required_pages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;

	num_of_processes_in_user++;
	last_free_place_user = start_page + (num_of_required_pages * PAGE_SIZE);

//	allocating = 0;

	return (void*) start_page;

}

//========================================
// [5] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName) {
	//TODO: [PROJECT'24.MS2 - #20] [4] SHARED MEMORY [USER SIDE] - sget()
	// Write your code here, remove the panic and write your code
	int size = sys_getSizeOfSharedObject(ownerEnvID, sharedVarName);

	if (size == E_SHARED_MEM_NOT_EXISTS) {
		return NULL;
	}
	// [3] search ff for space
	uint32 start_page = SearchUheapFF(size, 0);

	if (start_page == 0){
		return NULL;
	}

	int id = sys_getSharedObject(ownerEnvID, sharedVarName,(uint32 *) start_page);

	if (id == E_SHARED_MEM_NOT_EXISTS) {
		return NULL;
	}

	int num_of_required_pages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	num_of_processes_in_user++;
	last_free_place_user = start_page + (num_of_required_pages * PAGE_SIZE);

	return (void*) start_page;
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_freeSharedObject(...); which switches to the kernel mode,
//	calls freeSharedObject(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the freeSharedObject() function is empty, make sure to implement it.

void sfree(void* virtual_address) {
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [USER SIDE] - sfree()
	// Write your code here, remove the panic and write your code
	// panic("sfree() is not implemented yet...!!");
//	uint32 va = (uint32)virtual_address;
//	int page_num = (va - USER_HEAP_START)/PAGE_SIZE;
//
//	int sharedObjectId = marked_page[page_num].shared_object_id;
//
//	sys_freeSharedObject(sharedObjectId, virtual_address);
//
//	for (int i = page_num; i < marked_page[page_num].num_of_marked_pages; i++){
//		marked_page[page_num].virtual_address = 0;
//		marked_page[page_num].num_of_marked_pages = 0;
//		marked_page[page_num].shared_object_id = 0;
//	}

	uint32 va = (uint32) virtual_address;
	if (va < USER_HEAP_START || va >= USER_HEAP_MAX) {
		panic("Invalid virtual address passed to sfree()");
		return;
	}
	int num_of_pages = 0;
	int sharedObjectID = 0;
//	int start_page_index = (va - USER_HEAP_START) / PAGE_SIZE;

	int page_is_marked = sys_check_shared_allocated_page(va, &num_of_pages, &sharedObjectID);

	if (sharedObjectID <= 0) {
		return;
	}

	int result = sys_freeSharedObject(sharedObjectID, (void*)va);
	if (result != 0) {
		panic("Failed to free shared object in the kernel\n");
		return;
	}
	num_of_processes_in_user--;
	last_free_place_user = 0;
	first_time_allocating_user = 0;
}

//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size) {
	//[PROJECT]
	// Write your code here, remove the panic and write your code
	panic("realloc() is not implemented yet...!!");
	return NULL;

}

//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//

void expand(uint32 newSize) {
	panic("Not Implemented");

}
void shrink(uint32 newSize) {
	panic("Not Implemented");

}
void freeHeap(void* virtual_address) {
	panic("Not Implemented");

}
