// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"

// all the replacement algorithms
#define RA_FIFO 0 
#define RA_LRU  1

//  the replacement algorithm we choose
#define RA_THIS 1
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if ((which == SyscallException) && (type == SC_Halt) ) {

		DEBUG('a', "Shutdown, initiated by user program.\n");
	   	interrupt->Halt();
    } 

    if ((which == SyscallException) && (type == SC_Exit) ) {

        currentThread->Yield();
        printf("miss time %d, hit time %d ,miss rate %.8f\n",machine->tlb_miss_time,machine->tlb_hit_time,
            float(machine->tlb_miss_time)/float(machine->tlb_hit_time));

        for(int i=0;i<machine->pageTableSize;++i){
            int ppn = machine->pageTable[i].physicalPage;

            machine->bitmap->Clear(ppn);
        }
        printf("thread %d exits\n",currentThread->get_threadID());
        currentThread->Finish();

        DEBUG('a', "Shutdown, initiated by user program.\n");

    } 

    else if(which == PageFaultException){
    	// if (machine->cnttt++>100)
    	// 	ASSERT(0);
    	//printf("%d\n",machine->cnttt++);
    	// when tlb is used
    	if(machine->tlb!=NULL){
    		unsigned int vpn = (unsigned) machine->registers[BadVAddrReg] / PageSize;
    		int i = 0;
    		bool have_empty = 0;
    		for(;i<TLBSize;++i){
    			//printf("counter %d : %d \n",i,machine->tlb[i].counter);
    			if(machine->tlb[i].valid == 0){
    				have_empty = 1;
    				break;
    			}
    		}

    		// replacement algorithm
    		if(have_empty==0){
    			//FIFO
    			if(RA_THIS == RA_FIFO){
    				i = machine->tlb_top;
    			}

    			//LRU
    			if(RA_THIS == RA_LRU){
    				int pos = 0;
    				unsigned char latest_time = machine->tlb[0].counter;

    				for (i=0;i<TLBSize;++i){
    					if(machine->tlb[i].counter > latest_time){
    						latest_time = machine->tlb[i].counter;
    						pos = i;
    					}
    				}
    				i = pos;

    			}

    		}
    		machine->tlb[i].virtualPage = vpn;
    		machine->tlb[i].physicalPage = machine->pageTable[vpn].physicalPage;
    		machine->tlb[i].dirty = false;
    		machine->tlb[i].use = false;
    		machine->tlb[i].readOnly = false;
    		machine->tlb[i].valid = true;
    		machine->tlb[i].counter = 0;
    		machine->tlb_top = (machine->tlb_top+1) % TLBSize;

    	}

    	// when pageTable is used
    	else{
    		ASSERT(0)
    	}
    }

    else {
		printf("Unexpected user mode exception %d %d\n", which, type);
		ASSERT(FALSE);
    }
}
