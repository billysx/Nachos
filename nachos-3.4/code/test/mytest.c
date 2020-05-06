#include "syscall.h"

int main(){
    int fd1,fd2, result;
    char buffer[20];
    Create("write.txt");
    Create("read.txt");
    fd1 = Open("read.txt");
    fd2 = Open("write.txt");
    result = Read(buffer,7,fd1);
    Write(buffer, result, fd2);
    Close(fd1);
    Close(fd2);
    Halt();
}

