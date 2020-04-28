// synchdisk.cc
//	Routines to synchronously access the disk.  The physical disk
//	is an asynchronous device (disk requests return immediately, and
//	an interrupt happens later on).  This is a layer on top of
//	the disk providing a synchronous interface (requests wait until
//	the request completes).
//
//	Use a semaphore to synchronize the interrupt handlers with the
//	pending requests.  And, because the physical disk can only
//	handle one operation at a time, use a lock to enforce mutual
//	exclusion.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synchdisk.h"

//----------------------------------------------------------------------
// DiskRequestDone
// 	Disk interrupt handler.  Need this to be a C routine, because
//	C++ can't handle pointers to member functions.
//----------------------------------------------------------------------

static void
DiskRequestDone (int arg)
{
    SynchDisk* disk = (SynchDisk *)arg;

    disk->RequestDone();
}
static void DiskDone(int arg) { ((Disk *)arg)->HandleInterrupt(); }
//----------------------------------------------------------------------
// SynchDisk::SynchDisk
// 	Initialize the synchronous interface to the physical disk, in turn
//	initializing the physical disk.
//
//	"name" -- UNIX file name to be used as storage for the disk data
//	   (usually, "DISK")
//----------------------------------------------------------------------

SynchDisk::SynchDisk(char* name)
{
    semaphore = new Semaphore("synch disk", 0);
    lock = new Lock("synch disk lock");
    disk = new Disk(name, DiskRequestDone, (int) this);
    for (int i=0;i<NumSectors;++i){
        // printf("%d\n",i);
        writable[i] = new Semaphore("name",1);
        readerCount[i] = 0;
        ownerCount[i] = 0;
    }
    mutex = new Semaphore("name",1);
    for(int i=0;i<CacheSize;++i){
        cache[i].valid = 0;
        cache[i].sector = 0;
        cache[i].timestep = 0;
    }
}

//----------------------------------------------------------------------
// SynchDisk::~SynchDisk
// 	De-allocate data structures needed for the synchronous disk
//	abstraction.
//----------------------------------------------------------------------

SynchDisk::~SynchDisk()
{
    delete disk;
    delete lock;
    delete semaphore;
}

//----------------------------------------------------------------------
// SynchDisk::ReadSector
// 	Read the contents of a disk sector into a buffer.  Return only
//	after the data has been read.
//
//	"sectorNumber" -- the disk sector to read
//	"data" -- the buffer to hold the contents of the disk sector
//----------------------------------------------------------------------

void
SynchDisk::ReadSector(int sectorNumber, char* data)
{
    int index = -1;
    for(int i=0;i<CacheSize;++i){
        if(cache[i].valid == 1 && cache[i].sector == sectorNumber){
            index = i;
            break;
        }
    }
    //miss
    if(index == -1){
        printf("miss %d\n",sectorNumber);
        lock->Acquire();            // only one disk I/O at a time
        disk->ReadRequest(sectorNumber, data);
        semaphore->P();         // wait for interrupt
        lock->Release();
        // Find a replacement
        int rep = -1;
        for(int i=0;i<CacheSize;++i){
            if(!cache[i].valid){
                rep = i;
                break;
            }
        }
        if(rep==-1){
            int tmp = cache[0].timestep;
            int tmp_pos = 0;
            for(int i=1;i<CacheSize;++i){
                if(cache[i].timestep < tmp){
                    tmp = cache[i].timestep;
                    tmp_pos = i;
                }
            }
            rep = tmp_pos;
        }
        cache[rep].valid = 1;
        cache[rep].sector = sectorNumber;
        cache[rep].timestep = stats->totalTicks;
        bcopy(data,cache[rep].data,SectorSize);
    }
    else{
        printf("hit %d\n",sectorNumber);
        cache[index].timestep = stats->totalTicks;
        disk->ReadRequest(sectorNumber, data);
        semaphore->P();         // wait for interrupt
        bcopy(data,cache[index].data,SectorSize);
    }



    // lock->Acquire();         // only one disk I/O at a time
    // disk->ReadRequest(sectorNumber, data);
    // semaphore->P();         // wait for interrupt
    // lock->Release();
}

//----------------------------------------------------------------------
// SynchDisk::WriteSector
// 	Write the contents of a buffer into a disk sector.  Return only
//	after the data has been written.
//
//	"sectorNumber" -- the disk sector to be written
//	"data" -- the new contents of the disk sector
//----------------------------------------------------------------------

void
SynchDisk::WriteSector(int sectorNumber, char* data)
{
    lock->Acquire();			// only one disk I/O at a time
    disk->WriteRequest(sectorNumber, data);
    semaphore->P();			// wait for interrupt
    for(int i=0;i<CacheSize;++i){
        if(cache[i].sector == sectorNumber)
            cache[i].valid=0;
    }
    lock->Release();

}

//----------------------------------------------------------------------
// SynchDisk::RequestDone
// 	Disk interrupt handler.  Wake up any thread waiting for the disk
//	request to finish.
//----------------------------------------------------------------------

void
SynchDisk::RequestDone()
{
    semaphore->V();
}


//----------------------------------------------------------------------
// SynchDisk::StartReader
//  When the reader starting to read, check if there are writers writing
//----------------------------------------------------------------------

void
SynchDisk::StartReader(int sector)
{
    mutex->P();
    readerCount[sector]++;
    printf("readerCount is %d\n",readerCount[sector]);
    if(readerCount[sector]==1){
        writable[sector]->P();
    }
    mutex->V();
}



//----------------------------------------------------------------------
// SynchDisk::EndReader
//  When the reader finish reading, release the sem if
//  no readers are working.
//----------------------------------------------------------------------

void
SynchDisk::EndReader(int sector)
{
    mutex->P();
    readerCount[sector]--;
    if(readerCount[sector]==0)
        writable[sector]->V();
    mutex->V();
}


//----------------------------------------------------------------------
// SynchDisk::StartWriter
//  When the writer starts to read, check if there are writers writing
//  or readers reading
//----------------------------------------------------------------------

void
SynchDisk::StartWriter(int sector)
{
    writable[sector]->P();
}



//----------------------------------------------------------------------
// SynchDisk::EndWriter
//  When the writer finish reading, release the semaphore
//----------------------------------------------------------------------

void
SynchDisk::EndWriter(int sector)
{
    writable[sector]->V();
}

