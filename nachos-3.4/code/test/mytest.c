#include "syscall.h"

int fd1,fd2;
int result;
char buffer[20];

int main(){
	Create("write.txt");
	Create("read.txt");
	fd1 = Open("read.txt");
	fd2 = Open("write.txt");
	result = Read(buffer,20,fd1);
	Write(buffer, result, fd2);
	Close(fd1);
	Close(fd2);
	Halt();
}