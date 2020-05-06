// exception.cc
//  Entry point into the Nachos kernel from user programs.
//  There are two kinds of things that can cause control to
//  transfer back to here from user code:
//
//  syscall -- The user code explicitly requests to call a procedure
//  in the Nachos kernel.  Right now, the only function we support is
//  "Halt".
//
//  exceptions -- The user code does something that the CPU can't handle.
//  For instance, accessing memory that doesn't exist, arithmetic errors,
//  etc.
//
//  Interrupts (which can also cause control to transfer from user
//  code into the Nachos kernel) are handled elsewhere.
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
#define RA_NFU  1

//  the replacement algorithm we choose
#define RA_TLB 1
#define RA_PT  1



void InvertPageTable(){
    int vpn = (unsigned) machine->registers[BadVAddrReg] / PageSize;

    // Check if there are free physical pages
    int ppn = machine->bitmap->Find();

    // If there isn't any free physical pages
    // Replace one of the PTE in pageTable
    if (ppn == -1){
        int rep = 0;
        if(RA_PT == RA_NFU){
            unsigned char latest_time = 255;

            for (int j=0;j<machine->pageTableSize;++j){
                if(machine->pageTable[j].valid == 1
                    && machine->pageTable[j].counter < latest_time
                    && machine->pageTable[j].thread_id == currentThread->get_threadID()){
                    latest_time = machine->pageTable[j].counter;
                    rep = j;
                }
            }
        }

        // printf("rep, %d\n",rep);
        // Exit(0);
        // Found the virtual page rep
        // extract its ppn
        ppn = machine->pageTable[rep].physicalPage;
        // Write Back
        if (machine->pageTable[rep].dirty){
            //CHECK IF THIS IS RIGHT
            machine->simDisk->WriteAt(&(machine->mainMemory[ppn*PageSize]), PageSize, machine->pageTable[rep].virtualPage*PageSize);
        }
        machine->pageTable[rep].valid = 0;

    }
    machine->simDisk->ReadAt(&(machine->mainMemory[ppn*PageSize]), PageSize, vpn*PageSize);

    machine->pageTable[ppn].virtualPage  = vpn;
    machine->pageTable[ppn].physicalPage = ppn;
    machine->pageTable[ppn].dirty        = false;
    machine->pageTable[ppn].use          = false;
    machine->pageTable[ppn].readOnly     = false;
    machine->pageTable[ppn].valid        = true;
    machine->pageTable[ppn].counter      = 0;
    machine->pageTable[ppn].thread_id    = currentThread->get_threadID();
}

// Read out the name through register
char* getname(int name_addr){
    char*name = new char[100];
    int pos = 0;
    int tmp;

    while(true){
        machine->ReadMem(name_addr+pos,1,&tmp);
        if(tmp == 0){
            name[pos] = 0;
            break;
        }
        name[pos++] = char(tmp);
    }
    return name;
}

// void Create(char *name);
void syscall_create(){
    int name_addr = machine->ReadRegister(4);
    getname(name_addr);
    char*str = getname(name_addr);
    printf("Starting creating file %s\n",str);
    fileSystem->Create(str,0);
    machine->updatePC();
}

// OpenFileId Open(char *name);
void syscall_open(){
    int name_addr = machine->ReadRegister(4);
    char*str = getname(name_addr);
    printf("Starting opening file %s\n",str);
    OpenFile* openfile = fileSystem->Open(str);
    machine->WriteRegister(2,int(openfile));
    machine->updatePC();
}

// void Close(OpenFileId id);
void syscall_close(){
    int fd = machine->ReadRegister(4);
    OpenFile *openfile = (OpenFile*)fd;
    delete openfile;
    machine->updatePC();
}

// int Read(char *buffer, int size, OpenFileId id);
void syscall_read(){
    int addr = machine->ReadRegister(4);
    int length = machine->ReadRegister(5);
    int fd = machine->ReadRegister(6);
    OpenFile* openfile = (OpenFile*)fd;
    char data[length];
    int len = openfile->Read(data,length);
    for(int i=0;i<length;++i){
        machine->WriteMem(addr+i,1,int(data[i]));
    }
    machine->WriteRegister(2,len);
    machine->updatePC();
}

// void Write(char *buffer, int size, OpenFileId id);
void syscall_write(){
    int addr = machine->ReadRegister(4);
    int length = machine->ReadRegister(5);
    int fd = machine->ReadRegister(6);
    char data[length];
    int tmp;

    for(int i=0;i<length;++i){
        machine->ReadMem(addr+i,1,&tmp);
        data[i] = char(data);
    }
    OpenFile* openfile = (OpenFile*)fd;
    openfile->Write(data, length);
    machine->updatePC();
}

// SpaceId Exec(char *name);
void
StartP(char *filename){
    printf("thread %d executing file %s\n",currentThread->get_threadID(),filename);
    OpenFile *executable = fileSystem->Open(filename);
    AddrSpace *space;

    if (executable == NULL) {
    printf("Unable to open file %s\n", filename);
    return;
    }
    space = new AddrSpace(executable);
    currentThread->space = space;

    delete executable;          // close file

    space->InitRegisters();     // set the initial register values
    space->RestoreState();      // load page table register

    machine->Run();         // jump to the user progam
    ASSERT(FALSE);          // machine->Run never returns;
                    // the address space exits
                    // by doing the syscall "exit"
}
void syscall_exec(){
    int addr = machine->ReadRegister(4);
    char*str = getname(addr);
    Thread* t = new Thread("exec");
    t->Fork(StartP, str);
    machine->updatePC();
}

// // void Fork(void (*func)());
void
myfork(int func_addr){
    //printf("thread %d executing file %s\n",currentThread->get_threadID(),filename);
    OpenFile *executable = fileSystem->Open(currentThread->getName());
    AddrSpace *space;

    if (executable == NULL) {
    printf("Unable to open file %s\n", currentThread->getName());
    return;
    }
    space = new AddrSpace(executable);
    currentThread->space = space;

    delete executable;          // close file

    space->InitRegisters();     // set the initial register values
    space->RestoreState();      // load page table register

    machine->WriteRegister(PCReg, func_addr);
    machine->WriteRegister(NextPCReg, func_addr+4);

    machine->Run();         // jump to the user progam
    ASSERT(FALSE);          // machine->Run never returns;
                    // the address space exits
                    // by doing the syscall "exit"
}
void syscall_fork(){
    int addr = machine->ReadRegister(4);
    Thread* t = new Thread(currentThread->getName());
    t->Fork(myfork, addr);
    machine->updatePC();
}

// struct Args{
//     int func_addr;
//     AddrSpace* space;
// };
// // void Fork(void (*func)());
// void myfork(int arg){
//     Args* args = (Args*)arg;
//     AddrSpace* space = args->space;
//     space->InitRegisters();
//     space->RestoreState();

//     // machine->WriteRegister(PCReg,args->func_addr);
//     // machine->WriteRegister(NextPCReg,args->func_addr+4);
//     printf("** myfork **\n");
//     machine->Run();
// }
// void syscall_fork(){
//     Args*args = new Args;

//     int func_addr = machine->ReadRegister(4);
//     args->func_addr = func_addr;

//     OpenFile *exec = fileSystem->Open(currentThread->getName());
//     AddrSpace *space = new AddrSpace(exec);
//     // space->CopySpace(currentThread->space);
//     args->space = currentThread->space;

//     Thread*t = new Thread(currentThread->getName());
//     t->Fork(myfork,(int)args);
//     delete exec;
//     machine->updatePC();
// }

// void Yield();
void syscall_yield(){
    machine->updatePC();
    currentThread->Yield();
}

// int Join(SpaceId id);
void syscall_join(){
    int id = machine->ReadRegister(4);
    while(USED_THREAD_ID[id]){
        currentThread->Yield();
    }
    printf("waiting done\n");
    machine->updatePC();
}

//----------------------------------------------------------------------
// ExceptionHandler
//  Entry point into the Nachos kernel.  Called when a user program
//  is executing, and either does a syscall, or generates an addressing
//  or arithmetic exception.
//
//  For system calls, the following is the calling convention:
//
//  system call code -- r2
//      arg1 -- r4
//      arg2 -- r5
//      arg3 -- r6
//      arg4 -- r7
//
//  The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//  "which" is the kind of exception.  The list of possible exceptions
//  are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if ( which == SyscallException ) {
        if(type == SC_Halt){
            DEBUG('a', "Shutdown, initiated by user program.\n");
            for(int i=0;i<machine->pageTableSize;++i){
                int ppn = machine->pageTable[i].physicalPage;
                if (machine->pageTable[i].valid==1 && machine->pageTable[i].thread_id == currentThread->get_threadID())
                    machine->bitmap->Clear(ppn);
            }
            interrupt->Halt();
        }

        // void Exit(int status);
        else if(type == SC_Exit){
            /* printf("miss time %d, hit time %d ,miss rate %.8f\n",machine->tlb_miss_time,machine->tlb_hit_time,
            float(machine->tlb_miss_time)/float(machine->tlb_hit_time));*/
            int status = machine->ReadRegister(4);
            currentThread->Yield();

            for(int i=0;i<machine->pageTableSize;++i){
                int ppn = machine->pageTable[i].physicalPage;
                if (machine->pageTable[i].valid==1 && machine->pageTable[i].thread_id == currentThread->get_threadID())
                    machine->bitmap->Clear(ppn);
            }
            printf("thread %d exits with status %d\n",currentThread->get_threadID(),status);
            currentThread->Finish();

            DEBUG('a', "Shutdown, initiated by user program.\n");
        }
        else if(type == SC_Create){
            syscall_create();
        }
        else if(type == SC_Open){
            syscall_open();
        }
        else if(type == SC_Close){
            printf("Starting closing file\n");
            syscall_close();
        }
        else if(type == SC_Read){
            printf("Starting reading file\n");
            syscall_read();
        }
        else if(type == SC_Write){
            printf("Starting writing file\n");
            syscall_write();
        }
        else if(type == SC_Exec){
            syscall_exec();
        }
        else if(type == SC_Fork){
            printf("Starting Forking at %d\n",currentThread->get_threadID());
            syscall_fork();
        }
        else if(type == SC_Yield){
            printf("Starting Yield at %d\n",currentThread->get_threadID());
            syscall_yield();
        }
        else if(type == SC_Join){
            printf("Starting join at %d\n",currentThread->get_threadID());
            syscall_join();
        }

    }


    else if(which == PageFaultException){
        // if (machine->cnttt++>100)
        //  ASSERT(0);
        // printf("%d\n",machine->cnttt++);
        // when tlb is used
        if(machine->tlb!=NULL){
            printf("tlb fault\n");
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
                if(RA_TLB == RA_FIFO){
                    i = machine->tlb_top;
                    machine->tlb_top = (machine->tlb_top+1) % TLBSize;
                }

                //NFU
                if(RA_TLB == RA_NFU){
                    int pos = 0;
                    unsigned char latest_time = machine->tlb[0].counter;

                    for (int j=0;j<TLBSize;++j){
                        if(machine->tlb[j].counter < latest_time){
                            latest_time = machine->tlb[j].counter;
                            pos = j;
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

        }

        // when pageTable is used
        else{
            // printf("page fault\n");
            InvertPageTable();
            return;
            int vpn = (unsigned) machine->registers[BadVAddrReg] / PageSize;

            // Check if there are free physical pages
            int ppn = machine->bitmap->Find();

            // If there isn't any free physical pages
            // Replace one of the PTE in pageTable
            if (ppn == -1){
                int rep = 0;
                if(RA_PT == RA_NFU){
                    unsigned char latest_time = 255;

                    for (int j=0;j<machine->pageTableSize;++j){
                        if(machine->pageTable[j].valid == 1 && machine->pageTable[j].counter < latest_time){
                            latest_time = machine->pageTable[j].counter;
                            rep = j;
                        }
                    }
                }


                // Found the virtual page rep
                // extract its ppn
                ppn = machine->pageTable[rep].physicalPage;
                // Write Back
                if (machine->pageTable[rep].dirty){
                    //CHECK IF THIS IS RIGHT
                    machine->simDisk->WriteAt(&(machine->mainMemory[ppn*PageSize]), PageSize, machine->pageTable[rep].virtualPage*PageSize);
                }
                machine->pageTable[rep].valid = 0;

            }
            machine->simDisk->ReadAt(&(machine->mainMemory[ppn*PageSize]), PageSize, vpn*PageSize);

            machine->pageTable[vpn].virtualPage  = vpn;
            machine->pageTable[vpn].physicalPage = ppn;
            machine->pageTable[vpn].dirty        = false;
            machine->pageTable[vpn].use          = false;
            machine->pageTable[vpn].readOnly     = false;
            machine->pageTable[vpn].valid        = true;
            machine->pageTable[vpn].counter      = 0;
        }
    }

    else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}
