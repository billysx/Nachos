// fstest.cc
//	Simple test routines for the file system.
//
//	We implement:
//	   Copy -- copy a file from UNIX to Nachos
//	   Print -- cat the contents of a Nachos file
//	   Perftest -- a stress test for the Nachos file system
//		read and write a really large file in tiny chunks
//		(won't work on baseline system!)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "utility.h"
#include "filesys.h"
#include "system.h"
#include "thread.h"
#include "disk.h"
#include "stats.h"
#include "directory.h"
#include "openfile.h"

#define TransferSize 	10 	// make it small, just to be difficult

//----------------------------------------------------------------------
// Copy
// 	Copy the contents of the UNIX file "from" to the Nachos file "to"
//----------------------------------------------------------------------

void
Copy(char *from, char *to)
{
    FILE *fp;
    OpenFile* openFile;
    int amountRead, fileLength;
    char *buffer;

// Open UNIX file
    if ((fp = fopen(from, "r")) == NULL) {
	printf("Copy: couldn't open input file %s\n", from);
	return;
    }
// Figure out length of UNIX file
    fseek(fp, 0, 2);
    fileLength = ftell(fp);
    fseek(fp, 0, 0);

// Create a Nachos file of the same length

    DEBUG('f', "Creating nachos file %s, size %d, to file %s\n", from, fileLength, to);
    if (!fileSystem->Create(to, fileLength)) {	 // Create Nachos file
	printf("Copy: couldn't create output file %s\n", to);
	fclose(fp);
	return;
    }
    printf("-- Opening nachos file %s \n",to);

    openFile = fileSystem->Open(to);
    ASSERT(openFile != NULL);
    printf("Copying file %s, size %d, to file %s at %d\n", from, fileLength, to,openFile->GetHdrPos());
// Copy the data in TransferSize chunks
    DEBUG('f', "Copying file %s, size %d, to file %s\n", from, fileLength, to);
    buffer = new char[TransferSize];
    while ((amountRead = fread(buffer, sizeof(char), TransferSize, fp)) > 0){
        openFile->Write(buffer, amountRead);
    }
    delete [] buffer;
// Close the UNIX and the Nachos files
    delete openFile;
    fclose(fp);
}

//----------------------------------------------------------------------
// Print
// 	Print the contents of the Nachos file "name".
//----------------------------------------------------------------------

void
Print(char *name)
{
    OpenFile *openFile;
    int i, amountRead;
    char *buffer;
    if ((openFile = fileSystem->Open(name)) == NULL) {
	printf("Print: unable to open file %s\n", name);
	return;
    }
    buffer = new char[TransferSize];
    while ((amountRead = openFile->Read(buffer, TransferSize)) > 0)
	for (i = 0; i < amountRead; i++)
	    printf("%c", buffer[i]);
    printf("\n");
    delete [] buffer;
    // fileSystem->Print();
    delete openFile;		// close the Nachos file
    return;
}

void MakeDir(char*name){
    fileSystem->Create(name,-1);
}

//----------------------------------------------------------------------
// PerformanceTest
// 	Stress the Nachos file system by creating a large file, writing
//	it out a bit at a time, reading it back a bit at a time, and then
//	deleting the file.
//
//	Implemented as three separate routines:
//	  FileWrite -- write the file
//	  FileRead -- read the file
//	  PerformanceTest -- overall control, and print out performance #'s
//----------------------------------------------------------------------

#define FileName 	"TestFile"
#define Contents 	"1234567890"
#define ContentSize 	strlen(Contents)
#define FileSize 	20 //((int)(ContentSize * 5000))
#define MiniFileSize 1300

static void
FileWrite()
{
    OpenFile *openFile;
    int i, numBytes;

    printf("Sequential write of %d byte file, in %d byte chunks\n",
	FileSize, ContentSize);
    if (!fileSystem->Create(FileName, 0)) {
      printf("Perf test: can't create %s\n", FileName);
      return;
    }
    // printf("before writing\n");
    openFile = fileSystem->Open(FileName);
    if (openFile == NULL) {
	printf("Perf test: unable to open %s\n", FileName);
	return;
    }

    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Write(Contents, ContentSize);
    	if (numBytes < 10) {
    	    printf("Perf test: unable to write %s\n", FileName);
    	    delete openFile;
    	    return;
    	}
    }
    delete openFile;	// close file
}

static void
FileRead()
{
    OpenFile *openFile;
    char *buffer = new char[ContentSize];
    int i, numBytes;

    printf("Sequential read of %d byte file, in %d byte chunks\n",
	FileSize, ContentSize);
    if ((openFile = fileSystem->Open(FileName)) == NULL) {
	printf("Perf test: unable to open file %s\n", FileName);
	return;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Read(buffer, ContentSize);
        buffer[ContentSize] = 0;
        // printf("%d reading %d %s\n",i,numBytes,buffer);

    	if ((numBytes < 10) || strncmp(buffer, Contents, ContentSize)) {
    	    printf("Perf test: unable to read %s\n", FileName);
    	    delete openFile;
    	    delete [] buffer;
    	    return;
    	}
    }
    delete [] buffer;
    delete openFile;	// close file

}
void RemoveTest(){
    if (fileSystem->Remove(FileName)) {
      printf("Perf test: removing %s\n", FileName);
      return;
    }

}
void
PerformanceTest()
{
    printf("Starting file system performance test:\n");
    //stats->Print();
    FileWrite();
    // Thread*t1 = new Thread("test_synchdisk");
    // t1->Fork(RemoveTest,0);
    printf("%d reading ...",currentThread->get_threadID());
    FileRead();
    if (fileSystem->Remove(FileName)) {
      printf("Perf test: removing %s\n", FileName);
      return;
    }
    printf("reading done\n");
    //stats->Print();
}
void fileTest1(){
    char res[ContentSize] = Contents;
    fileSystem->PushPipe(Contents);
    currentThread->Yield();
    fileSystem->PushPipe("987654321");
}

void fileTest2(){
    char*ans = new char[ContentSize];
    fileSystem->PopPipe(ans);
    printf("Pipe reading out %s\n",ans);
    currentThread->Yield();
    fileSystem->PopPipe(ans);
    printf("Pipe reading out %s\n",ans);

}

void
PerformanceTest2(){
    Thread*t1 = new Thread("t1");
    Thread*t2 = new Thread("t2");
    t1->Fork(fileTest1,0);
    t2->Fork(fileTest2,0);
}