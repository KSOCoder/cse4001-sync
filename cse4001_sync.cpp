#include "semaphore_class.h"
#include <stdint.h>

const int numReaders = 5;
const int numWriters = 5;
const int numPhilosophers = 5;
const int iterations = 3;

Semaphore PrintMutex(1);

void say(const char *who, int id, const char *what)
{
   PrintMutex.wait();
   printf("%s %d: %s\n", who, id, what);
   fflush(stdout);
   PrintMutex.signal();
}

void sayCount(const char *who, int id, const char *what, int n)
{
   PrintMutex.wait();
   printf("%s %d: %s (readers in room: %d)\n", who, id, what, n);
   fflush(stdout);
   PrintMutex.signal();
}

void randomSleep(unsigned int *seed, int minMs, int maxMs)
{
   *seed = *seed * 1103515245u + 12345u;
   int ms = minMs + (*seed/65536) % (maxMs - minMs + 1);
   usleep(ms * 1000);
}

class Lightswitch {
   public:
      Lightswitch() : counter(0), mutex(1) {}
      void lock(Semaphore &sem)
      {
         mutex.wait();
         counter++;
         if (counter == 1) sem.wait();
         mutex.signal();
      }
      void unlock(Semaphore &sem)
      {
         mutex.wait();
         counter--;
         if (counter == 0) sem.signal();
         mutex.signal();
      }
   private:
      int counter;
      Semaphore mutex;
};

int readersInside = 0;
Semaphore CountMutex(1);

int enterReading()
{
   CountMutex.wait();
   int n = ++readersInside;
   CountMutex.signal();
   return n;
}
void leaveReading()
{
   CountMutex.wait();
   --readersInside;
   CountMutex.signal();
}

/* Problem 1: No-starve readers-writers (turnstile) */
Semaphore roomEmpty(1);
Semaphore turnstile(1);
Lightswitch readSwitch1;

void *Reader1(void *threadID)
{
   int id = (long)(intptr_t)threadID;
   unsigned int seed = id * 7919;
   for (int i = 0; i < iterations; i++)
   {
      randomSleep(&seed, 10, 100);
      say("Reader", id, "waiting");
      turnstile.wait();
      turnstile.signal();

      readSwitch1.lock(roomEmpty);
      sayCount("Reader", id, "reading", enterReading());
      randomSleep(&seed, 50, 150);
      leaveReading();
      say("Reader", id, "done reading");
      readSwitch1.unlock(roomEmpty);
   }
   return NULL;
}

void *Writer1(void *threadID)
{
   int id = (long)(intptr_t)threadID;
   unsigned int seed = id * 104729;
   for (int i = 0; i < iterations; i++)
   {
      randomSleep(&seed, 10, 100);
      say("Writer", id, "waiting");
      turnstile.wait();
      roomEmpty.wait();
      say("Writer", id, "writing");
      randomSleep(&seed, 50, 150);
      say("Writer", id, "done writing");
      turnstile.signal();
      roomEmpty.signal();
   }
   return NULL;
}

/* Problem 2: Writer-priority readers-writers */
Semaphore noReaders(1);
Semaphore noWriters(1);
Lightswitch readSwitch2;
Lightswitch writeSwitch2;

void *Reader2(void *threadID)
{
   int id = (long)(intptr_t)threadID;
   unsigned int seed = id * 7919;
   for (int i = 0; i < iterations; i++)
   {
      randomSleep(&seed, 10, 100);
      say("Reader", id, "waiting");
      noReaders.wait();
         readSwitch2.lock(noWriters);
      noReaders.signal();

      sayCount("Reader", id, "reading", enterReading());
      randomSleep(&seed, 50, 150);
      leaveReading();
      say("Reader", id, "done reading");

      readSwitch2.unlock(noWriters);
   }
   return NULL;
}

void *Writer2(void *threadID)
{
   int id = (long)(intptr_t)threadID;
   unsigned int seed = id * 104729;
   for (int i = 0; i < iterations; i++)
   {
      randomSleep(&seed, 10, 100);
      say("Writer", id, "waiting");
      writeSwitch2.lock(noReaders);
         noWriters.wait();
         say("Writer", id, "writing");
         randomSleep(&seed, 50, 150);
         say("Writer", id, "done writing");
         noWriters.signal();
      writeSwitch2.unlock(noReaders);
   }
   return NULL;
}

/* Dining philosophers: shared pieces */

Semaphore *forks[numPhilosophers];
int leftFork(int i) { return i; }
int rightFork(int i) { return (i+1) % numPhilosophers; }

/* Problem 3: at most 4 philosophers at the table */
Semaphore footman(numPhilosophers - 1);

void *Philosopher1(void *threadID)
{
   int id = (long)(intptr_t)threadID;
   unsigned int seed = id * 7919 + 1;
   for (int i = 0; i < iterations; i++)
   {
      say("Philosopher", id, "thinking");
      randomSleep(&seed, 20, 100);

      say("Philosopher", id, "hungry");
      footman.wait();
      forks[rightFork(id)]->wait();
      forks[leftFork(id)]->wait();

      say("Philosopher", id, "eating");
      randomSleep(&seed, 50, 150);
      say("Philosopher", id, "done eating");

      forks[rightFork(id)]->signal();
      forks[leftFork(id)]->signal();
      footman.signal();
   }
   return NULL;
}

/* Problem 4: philosopher 0 is left-handed */
void *Philosopher2(void *threadID)
{
   int id = (long)(intptr_t)threadID;
   unsigned int seed = id * 7919 + 1;
   for (int i = 0; i < iterations; i++)
   {
      say("Philosopher", id, "thinking");
      randomSleep(&seed, 20, 100);

      say("Philosopher", id, "hungry");
      if (id == 0) {
         forks[leftFork(id)]->wait();
         forks[rightFork(id)]->wait();
      } else {
         forks[rightFork(id)]->wait();
         forks[leftFork(id)]->wait();
      }

      say("Philosopher", id, "eating");
      randomSleep(&seed, 50, 150);
      say("Philosopher", id, "done eating");

      forks[rightFork(id)]->signal();
      forks[leftFork(id)]->signal();
   }
   return NULL;
}

void create(pthread_t *t, void *(*fn)(void *), long id)
{
   int rc = pthread_create(t, NULL, fn, (void *)(intptr_t)id);
   if (rc) {
      printf("ERROR creating thread %ld; return code is %d\n", id, rc);
      exit(-1);
   }
}

int main(int argc, char **argv)
{
   if (argc != 2) {
      printf("Usage: %s <problem #>\n", argv[0]);
      printf("  1 = No-starve readers-writers\n");
      printf("  2 = Writer-priority readers-writers\n");
      printf("  3 = Dining philosophers #1 (footman)\n");
      printf("  4 = Dining philosophers #2 (left-handed philosopher)\n");
      return 1;
   }

   int problem = atoi(argv[1]);
   pthread_t threads[numReaders + numWriters];
   int count = 0;

   switch (problem)
   {
      case 1:
      case 2:
         printf(problem == 1 ? "=== No-starve readers-writers ===\n"
         : "=== Writer-priority readers-writers ===\n");
         for (long r = 1; r <= numReaders; r++)
            create(&threads[count++], problem == 1 ? Reader1 : Reader2, r);
         for (long w = 1; w <= numWriters; w++)
            create(&threads[count++], problem == 1 ? Writer1 : Writer2, w);
         break;
      case 3:
      case 4:
         printf(problem == 3 ? "=== Dining philosophers #1 (footman) ===\n"
         : "=== Dining philosophers #2 (left-handed philosopher) ===\n");
         for (int i = 0; i < numPhilosophers; i++)
            forks[i] = new Semaphore(1);
         for (long p = 0; p < numPhilosophers; p++)
            create(&threads[count++], problem == 3 ? Philosopher1 : Philosopher2, p);
         break;
      default:
         printf("Invalid problem number: %s (must be 1-4)\n", argv[1]);
         return 1;
   }

   for (int i = 0; i < count; i++)
      pthread_join(threads[i], NULL);

   printf("=== Finished ===\n");
   return 0;
}
