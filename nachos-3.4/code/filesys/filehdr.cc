// filehdr.cc
//  Routines for managing the disk file header (in UNIX, this
//  would be called the i-node).
//
//  The file header is used to locate where on disk the
//  file's data is stored.  We implement this as a fixed size
//  table of pointers -- each entry in the table points to the
//  disk sector containing that portion of the file data
//  (in other words, there are no indirect or doubly indirect
//  blocks). The table size is chosen so that the file header
//  will be just big enough to fit in one disk sector,
//
//      Unlike in a real system, we do not keep track of file permissions,
//  ownership, last modification date, etc., in the file header.
//
//  A file header can be initialized in two ways:
//     for a new file, by modifying the in-memory data structure
//       to point to the newly allocated data blocks
//     for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "time.h"
#include "system.h"
#include "filehdr.h"
#include "directory.h"
#include "openfile.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
//  Initialize a fresh file header for a newly created file.
//  Allocate data blocks for the file out of the map of free disk blocks.
//  Return FALSE if there are not enough free blocks to accomodate
//  the new file.
//
//  "freeMap" is the bit map of free disk sectors
//  "fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{
    size = 0;
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
        return FALSE;        // not enough space
   // If indirect index is not needed
    if(numSectors < NumDirect){
        for (int i = 0; i < numSectors; i++)
            dataSectors[i] = freeMap->Find();
    }

    else{ // if indirect index is needed
        for (int i = 0; i < NumDirect; i++)   // Allocate the NumDirect sectors
            dataSectors[i] = freeMap->Find();
        int*secondary_index = new int[Sector2Int];
        for (int i=0;i<numSectors - (NumDirect-1);++i){
            secondary_index[i] = freeMap->Find();
        }
        synchDisk->WriteSector(dataSectors[NumDirect-1], (char*)secondary_index);
        delete []secondary_index;
    }

    printf("Allocate %d sectors\n",numSectors);
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
//  De-allocate all the space allocated for data blocks for this file.
//
//  "freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void
FileHeader::Deallocate(BitMap *freeMap)
{
    // Doesn't use secondary index
    if(numSectors < NumDirect){
        for (int i = 0; i < numSectors ; i++) {
            //ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
            freeMap->Clear((int) dataSectors[i]);
        }
    }

    else{
        char*secondary_index = new char[SectorSize];
        synchDisk->ReadSector(dataSectors[NumDirect-1], secondary_index);
        // numSectors - (NumDirect-1) = numSectors-8
        for (int i=0; i < numSectors - (NumDirect-1); ++i) {
            // ASSERT(freeMap->Test((int) secondary_index[i*sizeof(int)]));  // ought to be marked!
            freeMap->Clear((int) secondary_index[i*sizeof(int)]);
        }
        for(int i=0; i < NumDirect; ++i){
            // ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
            freeMap->Clear((int) dataSectors[i]);
        }
        delete []secondary_index;

    }

}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
//  Fetch contents of file header from disk.
//
//  "sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
//  Write the modified contents of the file header back to disk.
//
//  "sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
//  Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//  offset in the file) to a physical address (the sector where the
//  data at the offset is stored).
//
//  "offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    if(offset < (NumDirect-1)*SectorSize ){
        return(dataSectors[offset / SectorSize]);
    }
    else if(offset < ((NumDirect-1) + (Sector2Int-1))* SectorSize ){
        offset -= (NumDirect-1)*SectorSize;
        char*secondary_index = new char[SectorSize];
        synchDisk->ReadSector(dataSectors[NumDirect-1], secondary_index);
        int res = secondary_index[(offset / SectorSize)*sizeof(int)];
        delete []secondary_index;
        // printf("1 logical sector is %d, physical sector is %d\n",offset/SectorSize,res);
        return res;
    }
    else{
        int* secondary_index;
        int tmp_pos = offset/SectorSize - (NumDirect-1);
        char* tmp = new char[SectorSize];
        synchDisk->ReadSector(dataSectors[NumDirect-1], tmp);
        secondary_index = (int*)tmp;

        while( tmp_pos >= Sector2Int-1 ){
            // printf("%d read2 %d\n",tmp_pos,secondary_index[Sector2Int-1]);
            tmp_pos -= (Sector2Int-1);
            synchDisk->ReadSector(secondary_index[Sector2Int-1], tmp);
            secondary_index = (int*)tmp;
        }
        int res = secondary_index[tmp_pos];
        // printf("2 logical sector is %d, physical sector is %d\n",tmp_pos,res);
        return res;

    }

}

//----------------------------------------------------------------------
// FileHeader::FileLength
//  Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
//  Print the contents of the file header, and the contents of all
//  the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    printf("- FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    for (i = 0; i < numSectors; i++)
        printf("%d ", dataSectors[i]);
    printf("\n");
    printf("* Disk sector: %d, file type: %s\n",SectorPos,type);
    printf("* Create time: %s\n",CreateTime );
    printf("* Last visit time: %s\n", LastVisit);
    printf("* last edit time: %s\n", LastEdit);

    printf("\nFile contents:\n");
    if(numSectors < NumDirect){
        for (i = k = 0; i < numSectors; i++) {
            synchDisk->ReadSector(dataSectors[i], data);
            for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                    printf("%c", data[j]);
                else
                    printf("\\%x", (unsigned char)data[j]);
            }
            printf("\n");
        }
    }
    else{
        // print first index content
        for (i = k = 0; i < NumDirect-1; i++) {
            synchDisk->ReadSector(dataSectors[i], data);
            for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                    printf("%c", data[j]);
                else
                    printf("\\%x", (unsigned char)data[j]);
            }
            printf("\n");
        }

        // print secondary index content
        char*secondary_index = new char[SectorSize];
        synchDisk->ReadSector(dataSectors[NumDirect-1], secondary_index);
        for (i = 0; i < NumSectors- (NumDirect-1) ; i++) {
            synchDisk->ReadSector(secondary_index[i*sizeof(int)], data);
            for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                    printf("%c", data[j]);
                else
                    printf("\\%x", (unsigned char)data[j]);
            }
            printf("\n");
        }


    }

    delete [] data;
}

//----------------------------------------------------------------------
// FileHeader::SetLastVisit and SetLastEdit
//  When editing or reading happens, update its LastVisit and LastEdit variable
//----------------------------------------------------------------------

void
FileHeader::SetLastVisit(){
    time_t v_time;
    time(&v_time);
    strncpy(LastVisit, asctime(gmtime(&v_time)), 25);
    LastVisit[24] = 0;
    //printf("File is visited at %s\n", LastVisit);
}

void
FileHeader::SetLastEdit(){
    time_t e_time;
    time(&e_time);
    strncpy(LastEdit, asctime(gmtime(&e_time)), 25);
    LastEdit[24] = 0;
    //printf("File is edited at %s\n", LastEdit);
}



//----------------------------------------------------------------------
// FileHeader::Enlarge
//  Enlarge the
//----------------------------------------------------------------------
bool
FileHeader::Enlarge(BitMap* freeMap, int size){
    numBytes = numBytes + size;
    int oldNumSectors = numSectors;
    numSectors = divRoundUp(numBytes, SectorSize);
    int EnlargeSectors = numSectors - oldNumSectors;
    if(EnlargeSectors == 0)
        return true;
    else{
        int FreeSectors = freeMap->NumClear();
        // printf("numclear is %d\n",FreeSectors);
        if(FreeSectors < EnlargeSectors)
            return false;
    }
    printf("The file is increased for %d bytes, enlarging disk sectors from %d to %d\n",size,oldNumSectors,numSectors);
    if(numSectors <= NumDirect-1){
        for (int i = oldNumSectors; i < numSectors ; i++) {
            // printf("* %d\n",i);
            dataSectors[i] =  freeMap->Find();
        }
    }
    else if(numSectors <= (NumDirect-1) + (Sector2Int-1) ){ // if indirect index is needed
        int*secondary_index = new int[Sector2Int];
        int start_pos;       // where the secondary index starts
        if(oldNumSectors < NumDirect){
            printf("** started to using secondary_index **\n");
            // If it doesn't need to have secondary index before
            for (int i = oldNumSectors; i < NumDirect; i++) {
                dataSectors[i] = freeMap->Find();
            }
            start_pos = 0;
        }
        else{
            // If secondary index is acquired in the first place.
            char*tmp = new char[SectorSize];
            synchDisk->ReadSector(dataSectors[NumDirect-1], tmp);
            secondary_index = (int*)tmp;
            start_pos = oldNumSectors - (NumDirect-1);
        }
        for (int i=start_pos;i<numSectors - (NumDirect-1);++i){
            secondary_index[i] = freeMap->Find();
        }
        synchDisk->WriteSector(dataSectors[NumDirect-1], (char*)secondary_index);
        delete []secondary_index;
    }
    else{
        int* secondary_index;
        int* last_secondary_index;
        int last_secondary_index_addr;
        int tmp_pos = numSectors - (NumDirect-1);
        int start_pos = oldNumSectors - (NumDirect-1);
        char* tmp = new char[SectorSize];
        synchDisk->ReadSector(dataSectors[NumDirect-1], tmp);
        secondary_index = (int*)tmp;
        last_secondary_index_addr = dataSectors[NumDirect-1];

        while( tmp_pos > Sector2Int-1 ){
            // printf("start_pos %d tmp_pos %d\n",start_pos,tmp_pos);
            tmp_pos -= (Sector2Int-1);
            start_pos -= (Sector2Int-1);


            if( start_pos == 0 ){
                secondary_index[Sector2Int-1] = freeMap->Find();
                synchDisk->WriteSector(last_secondary_index_addr, (char*)secondary_index);
                last_secondary_index_addr = secondary_index[Sector2Int-1];
                // printf("---new allocation %d---\n",last_secondary_index_addr);
                secondary_index = new int [Sector2Int];
                break;
            }
            last_secondary_index_addr = secondary_index[Sector2Int-1];
            synchDisk->ReadSector(secondary_index[Sector2Int-1], tmp);


            secondary_index = (int*)tmp;
            // printf("last_secondary_index %d\n",last_secondary_index_addr);
        }

        //printf("writing into %d\n",last_secondary_index_addr);
        for (int i=start_pos;i<tmp_pos;++i){
            secondary_index[i] = freeMap->Find();
        }

        synchDisk->WriteSector(last_secondary_index_addr, (char*)secondary_index);
        delete []secondary_index;

    }
    return true;

}



