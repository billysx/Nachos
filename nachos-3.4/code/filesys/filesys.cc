// filesys.cc
//  Routines to manage the overall operation of the file system.
//  Implements routines to map from textual file names to files.
//
//  Each file in the file system has:
//     A file header, stored in a sector on disk
//      (the size of the file header data structure is arranged
//      to be precisely the size of 1 disk sector)
//     A number of data blocks
//     An entry in the file system directory
//
//  The file system consists of several data structures:
//     A bitmap of free disk sectors (cf. bitmap.h)
//     A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//  files.  Their file headers are located in specific sectors
//  (sector 0 and sector 1), so that the file system can find them
//  on bootup.
//
//  The file system assumes that the bitmap and directory files are
//  kept "open" continuously while Nachos is running.
//
//  For those operations (such as Create, Remove) that modify the
//  directory and/or bitmap, if the operation succeeds, the changes
//  are written immediately back to disk (the two files are kept
//  open during all this time).  If the operation fails, and we have
//  modified part of the directory and/or bitmap, we simply discard
//  the changed version, without writing it back to disk.
//
//  Our implementation at this point has the following restrictions:
//
//     there is no synchronization for concurrent accesses
//     files have a fixed size, set when the file is created
//     files cannot be bigger than about 3KB in size
//     there is no hierarchical directory structure, and only a limited
//       number of files can be added to the system
//     there is no attempt to make the system robust to failures
//      (if Nachos exits in the middle of an operation that modifies
//      the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "time.h"
#include "disk.h"
#include "bitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"
#include "system.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known
// sectors, so that they can be located on boot-up.
#define FreeMapSector       0
#define DirectorySector     1
#define FilenameSector      2
#define PipeSector          3

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number
// of files that can be loaded onto the disk.
#define FreeMapFileSize     (NumSectors / BitsInByte)
#define NumDirEntries       10
#define FilenameFileSize    40
#define DirectoryFileSize   (sizeof(DirectoryEntry) * NumDirEntries)
#define PipeFileSize        100
//----------------------------------------------------------------------
// FileSystem::FileSystem
//  Initialize the file system.  If format = TRUE, the disk has
//  nothing on it, and we need to initialize the disk to contain
//  an empty directory, and a bitmap of free sectors (with almost but
//  not all of the sectors marked as free).
//
//  If format = FALSE, we just have to open the files
//  representing the bitmap and the directory.
//
//  "format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        BitMap *freeMap      = new BitMap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
        FileHeader *mapHdr   = new FileHeader;
        FileHeader *dirHdr   = new FileHeader;
        FileHeader *nameHdr  = new FileHeader;
        FileHeader *pipeHdr  = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FreeMapSector);
        freeMap->Mark(DirectorySector);
        freeMap->Mark(FilenameSector);
        freeMap->Mark(PipeSector);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
        ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));
        ASSERT(nameHdr->Allocate(freeMap, FilenameFileSize));
        ASSERT(pipeHdr->Allocate(freeMap, PipeFileSize));

        // Flush the bitmap and directory FileHeaders back to disk
        // We need to do this before we can "Open" the file, since open
        // reads the file header off of disk (and currently the disk has garbage
        // on it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapHdr->WriteBack(FreeMapSector);
        dirHdr->WriteBack(DirectorySector);
        nameHdr->WriteBack(FilenameSector);
        pipeHdr->WriteBack(PipeSector);
        // OK to open the bitmap and directory files now
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile   = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
        filenameFile  = new OpenFile(FilenameSector);
        pipeFile      = new OpenFile(PipeSector);

        // Once we have the files "open", we can write the initial version
        // of each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);     // flush changes to disk
        directory->WriteBack(directoryFile);

        if (DebugIsEnabled('f')) {
            freeMap->Print();
            directory->Print();
            delete freeMap;
            delete directory;
            delete mapHdr;
            delete dirHdr;
        }
    }
    else {
    // if we are not formatting the disk, just open the files representing
    // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
        filenameFile  = new OpenFile(FilenameSector);
        pipeFile      = new OpenFile(PipeSector);
    }
}

//----------------------------------------------------------------------
// FileSystem::Create
//  Create a file in the Nachos file system (similar to UNIX create).
//  Since we can't increase the size of files dynamically, we have
//  to give Create the initial size of the file.
//
//  The steps to create a file are:
//    Make sure the file doesn't already exist
//        Allocate a sector for the file header
//    Allocate space on disk for the data blocks for the file
//    Add the name to the directory
//    Store the new file header on disk
//    Flush the changes to the bitmap and the directory back to disk
//
//  Return TRUE if everything goes ok, otherwise, return FALSE.
//
//  Create fails if:
//          file is already in directory
//      no free space for file header
//      no free entry for file in directory
//      no free space for data blocks for the file
//
//  Note that this implementation assumes there is no concurrent access
//  to the file system!
//
//  "name" -- name of file to be created
//  "initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool
FileSystem::Create(char *name, int initialSize)
{
    Directory *directory;
    BitMap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;

    DEBUG('f', "Creating file %s, size %d\n", name, initialSize);

    // Find the parent directory of the target file
    int parent_dir_sector = FindDir(name);

    DEBUG("f","%s 's parent_dir_sector %d\n",name,parent_dir_sector);

    OpenFile* parent_dir = new OpenFile(parent_dir_sector);

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(parent_dir);

    int isdir = 0;
    if (initialSize==-1){
        isdir = 1;
        initialSize = DirectoryFileSize;
    }
    if (directory->Find(name) != -1){
        return FALSE;          // file is already in directory
    }
    else{
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
        sector = freeMap->Find();   // find a sector to hold the file header

        // printf("%s is allocated with sector %d\n",name,sector);
        if (sector == -1)
            success = FALSE;        // no free block for file header
        else if (!directory->Add(name, sector, isdir)){
                success =  FALSE;    // no space in directory
            }
        else{
            hdr = new FileHeader;
            if (!hdr->Allocate(freeMap, initialSize))
                    success = FALSE;    // no space on disk for data
            else {

                success = TRUE;
                // Add file type
                int i,j = 0;
                for (i=0;i<strlen(name);++i)
                    if(name[i]==".")
                        break;
                for(i = i+1;i<strlen(name);++i,++j){
                    hdr->type[j] = name[i];
                }
                hdr->type[j] = 0;

                // Add file createTime
                time_t c_time;
                time(&c_time);
                strncpy(hdr->CreateTime, asctime(gmtime(&c_time)), 25);
                hdr->CreateTime[24] = 0;
                if(isdir) printf("Directory ");
                else printf("File ");
                printf("%s is created, header in %d\n",name,sector);

                hdr->SectorPos = sector;
                hdr->SetLastVisit();
                hdr->SetLastEdit();

                // everthing worked, flush all changes back to disk
                hdr->WriteBack(sector);
                // printf("flush back\n");
                directory->WriteBack(parent_dir);
                // directory->Print();
                freeMap->WriteBack(freeMapFile);
                // if(!isdir){
                //     Directory* test_dir = new Directory(NumDirEntries);
                //     OpenFile* test_file = new OpenFile(parent_dir_sector);

                //     test_dir->FetchFrom(test_file);
                //     test_dir->Find(name);
                // }

                if(isdir){
                    Directory* dir = new Directory(NumDirEntries);
                    OpenFile* dir_file = new OpenFile(sector);
                    dir->WriteBack(dir_file);
                    delete dir;
                    delete dir_file;
                }

            }
            delete hdr;
        }
        delete freeMap;
    }

    delete directory;
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
//  Open a file for reading and writing.
//  To open a file:
//    Find the location of the file's header, using the directory
//    Bring the header into memory
//
//  "name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile *
FileSystem::Open(char *name)
{
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = NULL;
    int sector;

    int parent_dir_sector = FindDir(name);

    printf("Opening file %s under directory at sector %d\n", name,parent_dir_sector);
    // Open father directory
    if(parent_dir_sector >= 0){
        openFile = new OpenFile(parent_dir_sector);
    }
    directory->FetchFrom(openFile);

    // Find your file under father directory
    sector = directory->Find(name);
    if (sector >= 0){
        printf("%s is at %d\n",name,sector );
        openFile = new OpenFile(sector);    // name was found in directory
    }
    else{
        printf("file not found error at %d\n",sector);
    }
    delete directory;
    return openFile;                // return NULL if not found
}

//----------------------------------------------------------------------
// FileSystem::Remove
//  Delete a file from the file system.  This requires:
//      Remove it from the directory
//      Delete the space for its header
//      Delete the space for its data blocks
//      Write changes to directory, bitmap back to disk
//
//  Return TRUE if the file was deleted, FALSE if the file wasn't
//  in the file system.
//
//  "name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool
FileSystem::Remove(char *name)
{
    Directory *directory;
    BitMap *freeMap;
    FileHeader *fileHdr;
    OpenFile* openFile = NULL;

    int sector;
    int dir_sector = FindDir(name);
    if(dir_sector>=0)
        openFile = new OpenFile(dir_sector);

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(openFile);
    sector = directory->Find(name);
    printf("Removing file with %d users\n",
            synchDisk->ownerCount[sector]);
    if(synchDisk->ownerCount[sector] > 0){
        printf("Unable to delete file.%d users are still using this file at the moment\n",
            synchDisk->ownerCount[sector]);
        return false;
    }

    if (sector == -1) {
       delete directory;
       return FALSE;             // file not found
    }

    if(directory->IsDir(name)==1){
        Directory*tmp_dir  = new Directory(NumDirEntries);
        OpenFile *tmp_file = new OpenFile(sector);
        tmp_dir->FetchFrom(tmp_file);
        if(!tmp_dir->IsEmpty()){
            printf("omited file that is not empty.\n");
            return FALSE;
        }
        delete tmp_dir;
        delete tmp_file;
    }


    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);

    fileHdr->Deallocate(freeMap);       // remove data blocks
    freeMap->Clear(sector);         // remove header block
    directory->Remove(name);

    freeMap->WriteBack(freeMapFile);        // flush to disk
    directory->WriteBack(directoryFile);        // flush to disk
    delete fileHdr;
    delete directory;
    delete freeMap;
    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::List
//  List all the files in the file system directory.
//----------------------------------------------------------------------

void
FileSystem::List()
{
    Directory *directory = new Directory(NumDirEntries);

    directory->FetchFrom(directoryFile);
    directory->List();
    delete directory;
}

//----------------------------------------------------------------------
// FileSystem::Print
//  Print everything about the file system:
//    the contents of the bitmap
//    the contents of the directory
//    for each file in the directory,
//        the contents of the file header
//        the data in the file
//----------------------------------------------------------------------

void
FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    // printf("Bit map file header:\n");
    // bitHdr->FetchFrom(FreeMapSector);
    // bitHdr->Print();

    // printf("Directory file header:\n");
    // dirHdr->FetchFrom(DirectorySector);
    // dirHdr->Print();

    // freeMap->FetchFrom(freeMapFile);
    // freeMap->Print();

    directory->FetchFrom(directoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
}


//----------------------------------------------------------------------
// FileSystem::FindDir
//  Find directory position according to "/" in file name
//----------------------------------------------------------------------

int
FileSystem::FindDir(char*name){
    // Starting from root directory
    int sector = DirectorySector;
    OpenFile * parent_file = new OpenFile(sector);
    Directory* parent_dir  = new Directory(NumDirEntries);
    parent_dir->FetchFrom(parent_file);


    int dir_pos = 0;
    int subdir_pos = 0;
    char subdir[20];
    int nameLength = strlen(name);
    // printf("Finding parent directory for %s\n",name);
    while(dir_pos < nameLength){
        subdir[subdir_pos++] = name[dir_pos++];
        // printf("%c\n",name[dir_pos]);
        if(name[dir_pos] == '/'){
            subdir[subdir_pos] = 0;
            sector = parent_dir->Find(subdir);
            // printf("** sub dir is in %d\n",sector);
            parent_file = new OpenFile(sector);
            parent_dir  = new Directory(NumDirEntries);
            parent_dir->FetchFrom(parent_file);
            dir_pos++;
            subdir_pos = 0;
        }
    }
    return sector;

}

//----------------------------------------------------------------------
// FileSystem::RopPipe
//  Read data from the pipe
//----------------------------------------------------------------------
int
FileSystem::PopPipe(char*data){
    FileHeader*fileHdr = new FileHeader;
    fileHdr->FetchFrom(PipeSector);
    int length = fileHdr->size;
    fileHdr->WriteBack(PipeSector);
    pipeFile->Read(data,length);
    pipeFile->Clear();
}



//----------------------------------------------------------------------
// FileSystem::PushPipe
//  Write data to the pipe
//----------------------------------------------------------------------
void
FileSystem::PushPipe(char*data){
    pipeFile->Write(data,strlen(data)+1);
    FileHeader*fileHdr = new FileHeader;
    fileHdr->FetchFrom(PipeSector);
    fileHdr->WriteBack(PipeSector);
}





