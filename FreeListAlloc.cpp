#include "MemoryManager.h"
#include <iostream>


namespace MemoryManager
{
	const int MM_POOL_SIZE = 65536;
	char MM_pool[MM_POOL_SIZE];


	//Store the first empty block location at bytes 0 through 3 (int)
	inline void SetFirstEmptyBlockLocation(int FirstEmptyBlockLocation)
	{
		int* HeadNode = (int*)(&MM_pool[0]);
		*HeadNode = FirstEmptyBlockLocation;
	}

	//helper function to get the first empty block location and keep things more readable
	inline int GetFirstEmptyBlockLocation()
	{
		return *((int*)(&MM_pool[0]));
	}

	//Creates an allocation header which contains the size (allocation size + header size)
	inline void CreateAllocationHeader(int Location, int Size)
	{
		int* AllocationSize = (int*)(&MM_pool[Location]);
		*AllocationSize = Size + sizeof(int);
	}

	inline int GetAllocationSize(int Location)
	{
		return *((int*)(&MM_pool[Location]));
	}

	//Function for creating empty block header. Needs to be given the empty block location,
	//the size of the empty block, and the location of the next block in the linked list (zero if there are none)
	inline void WriteEmptyBlockHeader(int BlockLocation, int BlockSize, int NextBlockLocation)
	{
		int* Size = (int*)(&MM_pool[BlockLocation]);
		*Size = BlockSize;

		int* NextBlockAddress = (int*)(&MM_pool[BlockLocation + sizeof(int)]);
		*NextBlockAddress = NextBlockLocation;
	}

	//Reads and returns the size of the empty block, for readability.
	inline int ReadEmptyBlockHeaderSize(int BlockLocation)
	{
		return *((int*)(&MM_pool[BlockLocation]));
	}

	//Reads and returns the location of the next empty block, for readability
	inline int ReadEmptyBlockHeaderNextBlock(int BlockLocation)
	{
		return *((int*)(&MM_pool[BlockLocation + sizeof(int)]));
	}

	//Get previous empty block with respect to this address
	inline int GetPreviousEmptyBlock(void* AllocationPtr)
	{
		//used mostly for merging empty blocks when deallocating memory
		//if memory address is the empty block header, use the other function

		int CurrentBlock = GetFirstEmptyBlockLocation();

		if (CurrentBlock == 0)
			return 0;

		int NextBlock = ReadEmptyBlockHeaderNextBlock(CurrentBlock);

		while (NextBlock != 0)
		{
			if (&MM_pool[CurrentBlock] < AllocationPtr && &MM_pool[NextBlock] > AllocationPtr)
			{
				return CurrentBlock;
			}

			CurrentBlock = NextBlock;
			NextBlock = ReadEmptyBlockHeaderNextBlock(CurrentBlock);
		}

		//In the case there is one empty block, so while loop gets skipped...
		//and the current block is less than the memory address return that block.
		if (&MM_pool[CurrentBlock] < AllocationPtr)
		{
			return CurrentBlock;
		}

		//There is no previous empty block
		return 0;
	}

	//Get the next empty block with respect to this address
	inline int GetNextEmptyBlock(void* AllocationPtr)
	{
		//used mostly for merging empty blocks when deallocating memory
		//if memory address is the empty block header, use the other function

		int CurrentBlock = GetFirstEmptyBlockLocation();

		if (CurrentBlock == 0)
			return 0;

		//In the case there is one empty block, so while loop gets skipped...
		//and the current block is less than the memory address return that block.
		if (&MM_pool[CurrentBlock] > AllocationPtr)
		{
			return CurrentBlock;
		}
		
		int NextBlock = ReadEmptyBlockHeaderNextBlock(CurrentBlock);

		while (NextBlock != 0)
		{
			if (&MM_pool[NextBlock] > AllocationPtr && &MM_pool[CurrentBlock] < AllocationPtr)
			{
				return NextBlock;
			}

			CurrentBlock = NextBlock;
			NextBlock = ReadEmptyBlockHeaderNextBlock(NextBlock);
		}

		//There is no next empty block
		return 0;
	}

	// Initialize set up any data needed to manage the memory pool
	void initializeMemoryManager(void)
	{
		//First position is at sizeof(int), because the first few bytes are used to store the head node
		//which is used to store the location of the first empty block
		SetFirstEmptyBlockLocation(sizeof(int));

		//Initialize the Freelist allocator by creating the header for the first empty memory block.
		//In the freelist allocator each free memory block has a header and works like a linked list.
		//only need this first one to initalize. Starts at size of int, with a size of the full array minus the size of int.
		//(this is due to the head node data)
		WriteEmptyBlockHeader(sizeof(int), MM_POOL_SIZE - sizeof(int), 0);
	}

	// return a pointer inside the memory pool
	// If no chunk can accommodate aSize call onOutOfMemory()
	void* allocate(int aSize)
	{
		//Stored locations of relevant blocks
		int PreviousBlock = 0;
		int CurrentBlock = GetFirstEmptyBlockLocation();
		int NextBlock = ReadEmptyBlockHeaderNextBlock(CurrentBlock);

		//Going to use first fit rather than best fit or worst fit.
		int FirstFitLocation = 0;

		//Loop through the free block linked list for a best fit
		while (CurrentBlock != 0)
		{
			//Two main cases: Empty block is large enough but cant fit a new empty block
			//OR
			//Empty block is large enough for allocation AND a new empty block to be created

			//Empty block can allocate memory, but not large enough for a new empty block
			if (ReadEmptyBlockHeaderSize(CurrentBlock) == aSize + (int)(sizeof(int)))
			{
				//Save the CurrentBlock Location, this is where the memory will be allocated
				FirstFitLocation = CurrentBlock;

				//Three cases of the linked list:
				if (PreviousBlock == 0 && NextBlock == 0)
				{
					//This would be a situation where the entire memory chunk gets used up
					SetFirstEmptyBlockLocation(0);

					CreateAllocationHeader(CurrentBlock, aSize);

					//CurrentBlock is still the index where the allocation was made,
					//however need to offset it by the header to return the proper address for the pointer
					return &MM_pool[CurrentBlock + sizeof(int)];
				}
				else if (PreviousBlock == 0 && NextBlock != 0)
				{
					//Next block is valid, previous block isnt. We are at the head of the list
					//so we need to modify the head
					SetFirstEmptyBlockLocation(NextBlock);

					CreateAllocationHeader(CurrentBlock, aSize);

					return &MM_pool[CurrentBlock + (int)(sizeof(int))];
				}
				else if (PreviousBlock != 0 && NextBlock == 0)
				{
					//There is a previous block, but there isn't a next block
					//so we need to make sure the previous block no longer points to current
					
					WriteEmptyBlockHeader(PreviousBlock, ReadEmptyBlockHeaderSize(PreviousBlock), 0);
					CreateAllocationHeader(CurrentBlock, aSize);

					return &MM_pool[CurrentBlock + (int)(sizeof(int))];
				}
				else //All are valid empty blocks
				{
					//Fix close the gap in linked list caused by allocated the freeblock
					WriteEmptyBlockHeader(PreviousBlock, ReadEmptyBlockHeaderSize(PreviousBlock), NextBlock);

					CreateAllocationHeader(CurrentBlock, aSize);

					return &MM_pool[CurrentBlock + (int)(sizeof(int))];
				}
			}
			else if (ReadEmptyBlockHeaderSize(CurrentBlock) > aSize + (int)(sizeof(int)) + (int)(2 * sizeof(int)))
			{
				//if size of current block greater than the size needed for allocation+size header
				//and there is enough room for another empty block

				//Save the CurrentBlock Location, this is where the memory will be allocated
				FirstFitLocation = CurrentBlock;

				//Three cases of the linked list:
				if (PreviousBlock == 0 && NextBlock == 0)
				{
					//Case where there is one empty block taking up entire memory

					//Location of the new empty block
					int NewEmptyBlockLocation = CurrentBlock + (int)sizeof(int) + aSize;

					//Create header
					WriteEmptyBlockHeader(NewEmptyBlockLocation, ReadEmptyBlockHeaderSize(CurrentBlock) - (int)sizeof(int) - aSize, 0);

					//Set the head of the linked list
					SetFirstEmptyBlockLocation(NewEmptyBlockLocation);

					//Create allocation header which holds the allocation size
					CreateAllocationHeader(CurrentBlock, aSize);

					//CurrentBlock is still the index where the allocation was made,
					//however need to offset it by the header to return the proper address for the pointer
					return &MM_pool[CurrentBlock + (int)(sizeof(int))];
				}
				else if(PreviousBlock == 0 && NextBlock != 0)
				{
					//Location of the new empty block
					int NewEmptyBlockLocation = CurrentBlock + aSize + (int)sizeof(int);

					//Create header
					WriteEmptyBlockHeader(NewEmptyBlockLocation, ReadEmptyBlockHeaderSize(CurrentBlock) - aSize - (int)sizeof(int), NextBlock);

					//Set the head of the linked list to the new empty block
					SetFirstEmptyBlockLocation(NewEmptyBlockLocation);

					CreateAllocationHeader(CurrentBlock, aSize);

					return &MM_pool[CurrentBlock + (int)(sizeof(int))];
				}
				else if (PreviousBlock != 0 && NextBlock == 0)
				{
					//There is a previous block, but there isn't a next block

					//Location of the new empty block
					int NewEmptyBlockLocation = CurrentBlock + aSize + (int)sizeof(int);

					//Create header
					WriteEmptyBlockHeader(NewEmptyBlockLocation, ReadEmptyBlockHeaderSize(CurrentBlock) - aSize - (int)sizeof(int), NextBlock);

					//Fix close the gap in linked list caused by allocated the freeblock
					WriteEmptyBlockHeader(PreviousBlock, ReadEmptyBlockHeaderSize(PreviousBlock), NewEmptyBlockLocation);

					CreateAllocationHeader(CurrentBlock, aSize);

					return &MM_pool[CurrentBlock + (int)(sizeof(int))];
				}
				else //All are valid empty blocks
				{
					//Location of the new empty block
					int NewEmptyBlockLocation = CurrentBlock + aSize + (int)sizeof(int);

					//Create header
					WriteEmptyBlockHeader(NewEmptyBlockLocation, ReadEmptyBlockHeaderSize(CurrentBlock) - aSize - (int)sizeof(int), NextBlock);

					//Fix close the gap in linked list caused by allocated the freeblock
					WriteEmptyBlockHeader(PreviousBlock, ReadEmptyBlockHeaderSize(PreviousBlock), NewEmptyBlockLocation);

					CreateAllocationHeader(CurrentBlock, aSize);

					return &MM_pool[CurrentBlock + (int)(sizeof(int))];
				}
			}
			
			//Set up for next iteration
			PreviousBlock = CurrentBlock;
			CurrentBlock = NextBlock;
			NextBlock = ReadEmptyBlockHeaderNextBlock(CurrentBlock);
		}

		//No space found for allocation, out of memory
		onOutOfMemory();

		return ((void*)0);
	}

	// Free up a chunk previously allocated
	void deallocate(void* aPointer)
	{
		//Get the empty blocks that are near the address of the allocated memory
		int PreviousEmptyBlock = GetPreviousEmptyBlock(aPointer);
		int NextEmptyBlock = GetNextEmptyBlock(aPointer);

		//Get the size of allocation and the index of the start of the allocation (includes header)
		int AllocationIndex = (static_cast<char *>(aPointer) - sizeof(int)) - MM_pool;
		int AllocationSize = GetAllocationSize(AllocationIndex);

		//Booleans to check for nearby empty chunks and whether or not allocation is directly next to them
		bool PreviousBlockExists = false;
		bool NextBlockExists = false;

		bool TouchingPreviousBlock = false;
		bool TouchingNextBlock = false;

		//setting the booleans...
		if (PreviousEmptyBlock != 0)
		{
			PreviousBlockExists = true;

			//Check if previous block is directly touching the allocation
			if (AllocationIndex == PreviousEmptyBlock + ReadEmptyBlockHeaderSize(PreviousEmptyBlock))
				TouchingPreviousBlock = true;
		}

		if (NextEmptyBlock != 0)
		{
			NextBlockExists = true;

			//Check if next block is directly touching the allocation
			if (AllocationIndex == NextEmptyBlock - AllocationSize)
				TouchingNextBlock = true;
		}


		if (!PreviousBlockExists && !NextBlockExists)
		{
			//One Case:		... A ...

			//this case the memory is full (no empty chunks). set first empty block to allocated index
			SetFirstEmptyBlockLocation(AllocationIndex);

			WriteEmptyBlockHeader(AllocationIndex, AllocationSize, 0);
		}
		else if (PreviousBlockExists && !NextBlockExists)
		{
			//Two Cases:	O A ...
			//				O ... A ...

			if (TouchingPreviousBlock && !TouchingNextBlock)
			{
				WriteEmptyBlockHeader(PreviousEmptyBlock, ReadEmptyBlockHeaderSize(PreviousEmptyBlock) + AllocationSize, ReadEmptyBlockHeaderNextBlock(PreviousEmptyBlock));
			}
			else if (!TouchingPreviousBlock && !TouchingNextBlock)
			{
				WriteEmptyBlockHeader(AllocationIndex, AllocationSize, ReadEmptyBlockHeaderNextBlock(PreviousEmptyBlock));
				WriteEmptyBlockHeader(PreviousEmptyBlock, ReadEmptyBlockHeaderSize(PreviousEmptyBlock), AllocationIndex);
			}
		}
		else if (!PreviousBlockExists && NextBlockExists)
		{
			//Two Cases:	... A O
			//				... A ... O

			if (!TouchingPreviousBlock && TouchingNextBlock)
			{
				SetFirstEmptyBlockLocation(AllocationIndex);

				WriteEmptyBlockHeader(AllocationIndex, AllocationSize + ReadEmptyBlockHeaderSize(NextEmptyBlock), ReadEmptyBlockHeaderNextBlock(NextEmptyBlock));
			}
			else if (!TouchingPreviousBlock && !TouchingNextBlock)
			{
				SetFirstEmptyBlockLocation(AllocationIndex);

				WriteEmptyBlockHeader(AllocationIndex, AllocationSize, NextEmptyBlock);
			}
		}
		else if (PreviousBlockExists && NextBlockExists)
		{
			//Four Cases:	O A O
			//				O ... A O
			//				O A ... O
			//				O ... A ... O

			if (TouchingPreviousBlock && TouchingNextBlock)
			{
				int SizeOfCombinedBlock = ReadEmptyBlockHeaderSize(PreviousEmptyBlock) + AllocationSize + ReadEmptyBlockHeaderSize(NextEmptyBlock);

				WriteEmptyBlockHeader(PreviousEmptyBlock, SizeOfCombinedBlock, ReadEmptyBlockHeaderNextBlock(NextEmptyBlock));
			}
			else if (!TouchingPreviousBlock && TouchingNextBlock)
			{
				int SizeOfCombinedBlock = AllocationSize + ReadEmptyBlockHeaderSize(NextEmptyBlock);

				//Add new header at old allocation spot
				WriteEmptyBlockHeader(AllocationIndex, SizeOfCombinedBlock, ReadEmptyBlockHeaderNextBlock(NextEmptyBlock));

				//Previous empty block points to the new starting address
				WriteEmptyBlockHeader(PreviousEmptyBlock, ReadEmptyBlockHeaderSize(PreviousEmptyBlock), AllocationIndex);
			}
			else if (TouchingPreviousBlock && !TouchingNextBlock)
			{
				int SizeOfCombinedBlock = ReadEmptyBlockHeaderSize(PreviousEmptyBlock) + AllocationSize;

				WriteEmptyBlockHeader(PreviousEmptyBlock, SizeOfCombinedBlock, ReadEmptyBlockHeaderNextBlock(PreviousEmptyBlock));
			}
			else if (!TouchingPreviousBlock && !TouchingNextBlock)
			{
				//update previous block header to point to allocation location
				WriteEmptyBlockHeader(PreviousEmptyBlock, ReadEmptyBlockHeaderSize(PreviousEmptyBlock), AllocationIndex);

				//New block at allocation location
				WriteEmptyBlockHeader(AllocationIndex, AllocationSize, NextEmptyBlock);
			}
		}
	}

	//---
	//--- support routines
	//--- 

	// Will scan the memory pool and return the total free space remaining
	int freeRemaining(void)
	{
		int CurrentBlock = GetFirstEmptyBlockLocation();

		if (CurrentBlock == 0)
			return 0;

		int NextBlock = ReadEmptyBlockHeaderNextBlock(CurrentBlock);

		int NumOfBytes = ReadEmptyBlockHeaderSize(CurrentBlock);

		//Debug
		std::cout << "Free Block Address: " << CurrentBlock << "\t\tSize: "<< NumOfBytes << std::endl;

		while (NextBlock != 0)
		{
			CurrentBlock = NextBlock;
			NextBlock = ReadEmptyBlockHeaderNextBlock(CurrentBlock);

			//Debug: Lists out the size of all empty blocks in memory
			std::cout << "Free Block Address: " << CurrentBlock << "\t\tSize: " << ReadEmptyBlockHeaderSize(CurrentBlock) << std::endl;

			NumOfBytes += ReadEmptyBlockHeaderSize(CurrentBlock);
		}

		return NumOfBytes;
	}

	// Will scan the memory pool and return the largest free space remaining
	int largestFree(void)
	{
		int CurrentBlock = GetFirstEmptyBlockLocation();
		int NextBlock = ReadEmptyBlockHeaderNextBlock(CurrentBlock);

		int LargestBlock = GetFirstEmptyBlockLocation();

		while (NextBlock != 0)
		{
			CurrentBlock = NextBlock;
			NextBlock = ReadEmptyBlockHeaderNextBlock(CurrentBlock);

			if (ReadEmptyBlockHeaderSize(CurrentBlock) > ReadEmptyBlockHeaderSize(LargestBlock))
			{
				LargestBlock = CurrentBlock;
			}
		}

		return ReadEmptyBlockHeaderSize(LargestBlock);
	}

	// will scan the memory pool and return the smallest free space remaining
	int smallestFree(void)
	{
		int CurrentBlock = GetFirstEmptyBlockLocation();
		int NextBlock = ReadEmptyBlockHeaderNextBlock(CurrentBlock);

		int SmallestBlock = GetFirstEmptyBlockLocation();

		while (NextBlock != 0)
		{
			CurrentBlock = NextBlock;
			NextBlock = ReadEmptyBlockHeaderNextBlock(CurrentBlock);

			if (ReadEmptyBlockHeaderSize(CurrentBlock) < ReadEmptyBlockHeaderSize(SmallestBlock))
			{
				SmallestBlock = CurrentBlock;
			}
		}

		return ReadEmptyBlockHeaderSize(SmallestBlock);
	}
}