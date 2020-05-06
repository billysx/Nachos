#include "syscall.h"
void test(){
	Yield();
	Create("torch2.py");
	Exit(0);
}
int main(){
	int id = Exec("../test/sort");
	Fork(test);
	Join(id);
}
