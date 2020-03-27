// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "elevatortest.h"
#include "interrupt.h"
#include "synch.h"

// testnum is set in main.cc
int testnum = 1;

#define CARGO_MAX 2
#define BARRIER_MAX 5


Semaphore* rcnt_s = new Semaphore("rcnt",1);
Semaphore* writable = new Semaphore("writable",1); 
int rcnt = 0;

void myReader(){
    int iter=1;
    while(iter--) {
        rcnt_s->P();
        rcnt++;
        if(rcnt == 1){ 
            writable->P();
        }
        //rcnt++;
        printf("reader %d is reading ---- %d is reading\n",currentThread->get_threadID(),rcnt);

        rcnt_s->V();
        currentThread->Yield();
        rcnt_s->P();

        rcnt--;
        printf("reader %d finishes reading ---- %d is reading\n",currentThread->get_threadID(),rcnt);
        if(rcnt == 0){
            writable->V();
        }
        rcnt_s->V();
    }
}

void myWriter(){
    printf("writer %d tries to write\n",currentThread->get_threadID());
    writable->P();
    printf("writer %d is writing",currentThread->get_threadID());
    printf(" *** %d reader working *** ",rcnt);
    printf("writer %d finishes writing\n",currentThread->get_threadID());
    writable->V();
}

void ReadWriteTest()
{
    for(int i=0;i<3;++i){

        Thread* r1 = new Thread("reader",3);
        r1->Fork(myReader, 0);

        Thread* w1 = new Thread("writer",1);
        w1->Fork(myWriter, 0);

        Thread* r2 = new Thread("reader",1);
        r2->Fork(myReader, 0);


    }

}








Lock* barrierLock = new Lock("barrierLock");
Condition* group_cond = new Condition("barrierCond");
int allnumber = 0;

void barrier(){

    barrierLock->Acquire();
    allnumber++;
    if (allnumber < BARRIER_MAX) {
        printf("allnumber:%d\n",allnumber);
        group_cond->Wait(barrierLock);

        // The step to release the lock is very important
        // because after a thread is waken up by Signal()
        // It would re-Acquire the lock.
        // If it doesn't release the lock, other threads
        // would be stuck.

        barrierLock->Release();
    }
    else{
        printf("%d break the barrier\n",currentThread->get_threadID());
        group_cond->Broadcast(barrierLock);
        barrierLock->Release();
    }
    printf("%d is Going out with %d people arrived at the barrier.\n",currentThread->get_threadID(),allnumber);

}
void barrierTest(){
    for(int i=0;i<5;++i){
        Thread* t = new Thread("t",2);
        t->Fork(barrier, i);
    }
}


int cargo_number = 0;
Condition* c_cond = new Condition("consumer");
Condition* p_cond = new Condition("producer");
Lock* testLock = new Lock("testLock");

void producer_c(){
    int iter = 6;
    while(iter--){
        // Equals to how we enter the monitor exclusively
        testLock->Acquire();
        if(cargo_number>=CARGO_MAX){
            printf("producer waiting\n");
            p_cond->Wait(testLock);
        }
        printf("In producer: %d ---- ", cargo_number);
        cargo_number++;
        printf("In producer: %d\n", cargo_number);
        c_cond->Signal(testLock);
        testLock->Release();
    }
}

void consumer_c(){
    int iter=6;
    while(iter--){
        // Equals to how we enter the monitor exclusively
        testLock->Acquire();
        if(cargo_number <= 0) {
            printf("customer ");
            c_cond->Wait(testLock);
        }
        printf("In consumer: %d ---- ", cargo_number);
        cargo_number--;
        printf("In consumer: %d\n", cargo_number);
        p_cond->Signal(testLock);
        testLock->Release();
    }
}
void CondTest(){
    printf("----------\n");
    Thread* p = new Thread("producer2");
    p->Fork(producer_c, 0);

    Thread* c = new Thread("consumer2");
    c->Fork(consumer_c, 0);
}



Semaphore* customer_S = new Semaphore("customer",0);
Semaphore* mutex = new Semaphore("mutex",1);
Semaphore* barber_S = new Semaphore("barber",0);

int num_wait = 0;
int num_chair = 5;
int flag = 0;
void barber(){
    while(1){
        customer_S->P();
        mutex->P();
        if (flag==0){
            printf("barber get to work\n");
            flag=1;
        }
        printf("Waiting customer number before:%d --- ",num_wait);
        num_wait--;
        printf("Waiting customer number after:%d\n",num_wait);
        mutex->V();
        barber_S->V();
    }
}
void customer(int number){
    mutex->P();
    if(num_wait < num_chair){
        printf("%d is waiting\n",number);
        num_wait++;
        customer_S->V();
        mutex->V();
        barber_S->P();
        printf("%d is getting hair cut\n",number);
    }
    else{
        printf("%d goes away, %d is waiting\n",number,num_wait);
        mutex->V();
    }
}


void SemaphoreTest(){

    for(int i=1;i<=7;++i){
        Thread* cust = new Thread("test",2);
        cust->Fork(customer, i);
    }

    Thread* barb = new Thread("test",5);
    barb->Fork(barber, 0);

    for(int i=11;i<=18;++i){
        Thread* cust = new Thread("test",2);
        cust->Fork(customer, i);
    }
}



void
TestPrio(){

    for(int i=0;i<1;++i){
        printf("---Thread %d is running with priority %d---\n",
            currentThread->get_threadID(),
            currentThread->get_StaticPro());
        currentThread->Yield();
    }
}

void 
ThreadTest5()
{
    DEBUG('t', "Entering ThreadTest5");

    for (int i=9;i>=0;--i){
        Thread *t = new Thread("test",i%10);
        t->Fork(TestPrio,0);
    }
}


void 
TimerTest(){

    for (int i=0; i<5; ++i){
        interrupt->SetLevel(IntOff);
        printf("-------------------------------\n");
        printf("Thread %d has used %d time slice\n",
            currentThread->get_threadID(),
            currentThread->get_UsedTime());
        printf("-------------------------------\n");
        interrupt->SetLevel(IntOn);
    }
}
void 
ThreadTest4()
{
    DEBUG('t', "Entering ThreadTest4");

    Thread *t1 = new Thread("test1");
    Thread *t2 = new Thread("test2");
    Thread *t3 = new Thread("test3");
    Thread *t4 = new Thread("test4");
    t1->Fork(TimerTest, 0);
    t2->Fork(TimerTest, 0);
    t3->Fork(TimerTest, 0);
    t4->Fork(TimerTest, 0);
}


void
ThreadTest3()
{
    DEBUG('t', "Entering ThreadTest3");
    for (int i=0;i<200;++i){
    	Thread *t = new Thread("test");
    }

}


//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
	printf("*** thread %d looped %d times\n", which, num);
        currentThread->Yield();
    }
}


void TS(){
	printf("---------------------\n");
	currentThread->Print();
	scheduler->Print();
	currentThread->Yield();
	printf("---------------------\n");
}

void
ThreadTest2()
{
    DEBUG('t', "Entering ThreadTest2");

    Thread *t1 = new Thread("test1");
    Thread *t2 = new Thread("test2");
    t1->Fork(TS, 0);
    t2->Fork(TS, 0);
}


//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, (void*)1);
    SimpleThread(0);
}


//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 1:
	ThreadTest1();
	break;
	case 2:
	ThreadTest2();
	break;
	case 3:
	ThreadTest3();
    break;
    case 4:
    ThreadTest4();
	break;
    case 5:
    ThreadTest5();
    break;
    case 6:
    SemaphoreTest();
    break;
    case 7:
    CondTest();
    break;
    case 8:
    barrierTest();
    break;
    case 9:
    ReadWriteTest();
    break;
    default:
	printf("No test specified.\n");
	break;
    }
}

