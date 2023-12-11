/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
/* 세마포어 SEMA를 Value로 초기화합니다.
세마포어는 음이 아닌 정수와 두 개의 원자 연산자
조작하기:

- down or "P": 값이 양수가 될 때까지 기다렸다가
줄이세요.

- up or "V": 값을 증가시키고 대기 중인 한 명을 깨웁니다
나사산(thread, 있는 경우). */

// 원래 thread에서 구현했던 cmp_priority
bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

void sema_init(struct semaphore *sema, unsigned value)
{
   ASSERT(sema != NULL);

   sema->value = value;
   list_init(&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
/* 세마포어에서 down 또는 "P" 작업. SEMA의 값이 양성이 될때까지 기다렸다가 감소

이 기능은 절전 모드일 수 있으므로 다음 시간 내에 호출하면 안 됩니다
인터럽트 핸들러. 이 함수는 다음과 같이 호출될 수 있습니다
인터럽트가 비활성화되어 있지만, 그것이 절전 모드일 경우 다음으로 예약됩니다
스레드는 아마도 인터럽트를 다시 켜줄 것입니다. 이것은
sema_down 함수. */
void sema_down(struct semaphore *sema)
{ // sema 수정해야 할 것! // waiters list의 우선순위를 정렬하여 thread를 삽입하는 부분을 수정해야함.
   enum intr_level old_level;

   ASSERT(sema != NULL);
   ASSERT(!intr_context());

   old_level = intr_disable();
   while (sema->value == 0)
   {
      // list_push_back (&sema->waiters, &thread_current ()->elem);
      list_insert_ordered(&sema->waiters, &thread_current()->elem, cmp_priority, NULL); // p_s2 : 비교해서 정렬하는거 추가
      thread_block();
   }
   sema->value--;
   intr_set_level(old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool sema_try_down(struct semaphore *sema)
{
   enum intr_level old_level;
   bool success;

   ASSERT(sema != NULL);

   old_level = intr_disable();
   if (sema->value > 0)
   {
      sema->value--;
      success = true;
   }
   else
      success = false;
   intr_set_level(old_level);

   return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
/* 세마포어에서 업 또는 "V" 작업. SEMA 값을 증가시킵니다
그리고 SEMA를 기다리는 사람들 중 하나의 실을 깨웁니다.

이 함수는 인터럽트 핸들러에서 호출될 수 있습니다.*/
void sema_up(struct semaphore *sema)
{ // waters list안의 thread를 우선순위에 맞춰 정렬하는 것 추가해야 함.
   // 특정 lock을 기다리던 thread에게 잠금이 해제되었으니 기다리던 애를 unblock해주면서 sema를 사용할 수 있도록 해줌
   enum intr_level old_level;

   ASSERT(sema != NULL);

   old_level = intr_disable();
   if (!list_empty(&sema->waiters))
      thread_unblock(list_entry(list_pop_front(&sema->waiters),
                                struct thread, elem));
   sema->value++;
   intr_set_level(old_level);
}

static void sema_test_helper(void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void sema_self_test(void)
{
   struct semaphore sema[2];
   int i;

   printf("Testing semaphores...");
   sema_init(&sema[0], 0);
   sema_init(&sema[1], 0);
   thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
   for (i = 0; i < 10; i++)
   {
      sema_up(&sema[0]);
      sema_down(&sema[1]);
   }
   printf("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper(void *sema_)
{
   struct semaphore *sema = sema_;
   int i;

   for (i = 0; i < 10; i++)
   {
      sema_down(&sema[0]);
      sema_up(&sema[1]);
   }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void lock_init(struct lock *lock)
{
   ASSERT(lock != NULL);

   lock->holder = NULL;
   sema_init(&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void lock_acquire(struct lock *lock)
{
   if (!thread_mlfqs) // mlfqs 일 때 donation 비활성화
   {
      ASSERT(lock != NULL);
      ASSERT(!intr_context());
      ASSERT(!lock_held_by_current_thread(lock));

      sema_down(&lock->semaphore);
      lock->holder = thread_current();
   }
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool lock_try_acquire(struct lock *lock)
{
   bool success;

   ASSERT(lock != NULL);
   ASSERT(!lock_held_by_current_thread(lock));

   success = sema_try_down(&lock->semaphore);
   if (success)
      lock->holder = thread_current();
   return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void lock_release(struct lock *lock)
{
   ASSERT(lock != NULL);
   ASSERT(lock_held_by_current_thread(lock));

   lock->holder = NULL;
   sema_up(&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool lock_held_by_current_thread(const struct lock *lock)
{
   ASSERT(lock != NULL);

   return lock->holder == thread_current();
}

/* One semaphore in a list. */
struct semaphore_elem
{
   struct list_elem elem;      /* List element. */
   struct semaphore semaphore; /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void cond_init(struct condition *cond)
{
   ASSERT(cond != NULL);

   list_init(&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
/* 자동으로 LOCK을 해제하고 COND 신호가 전송될 때까지 기다립니다
또 다른 코드입니다. COND가 신호를 보낸 후, LOCK은
돌아가기 전에 다시 획득했습니다. 전화하기 전에 LOCK을(를) 보유해야 합니다
이 기능.

이 기능으로 구현된 모니터는 "메사" 스타일이 아니라
"Hoare" 스타일, 즉 신호를 주고 받는 것이 아닙니다
원자 연산. 따라서, 일반적으로 발신자는 다시 확인해야 합니다
대기가 완료되고 필요한 경우 대기 후의 상태
다시.

주어진 조건 변수는 단일 변수와만 연관되어 있습니다
잠금, 그러나 하나의 잠금은 임의의 수의 잠금과 연관될 수 있습니다
조건 변수. 즉, 일대일 매핑이 있습니다
잠금에서 상태 변수까지.

이 기능은 절전 모드일 수 있으므로 다음 시간 내에 호출하면 안 됩니다
인터럽트 핸들러. 이 함수는 다음과 같이 호출될 수 있습니다
인터럽트는 비활성화되어 있지만, 인터럽트는 다시 켜집니다
우리는 자야합니다.*/
void cond_wait(struct condition *cond, struct lock *lock)
{ // thead가 스스로를 잠재우기 위해 호출
   //<-> cond_signal() 조건이 만족되기를 대기하면서 잠자고 있던 thread를 깨울 때 호출
   struct semaphore_elem waiter;

   ASSERT(cond != NULL);
   ASSERT(lock != NULL);
   ASSERT(!intr_context());
   ASSERT(lock_held_by_current_thread(lock));

   sema_init(&waiter.semaphore, 0); // semaphore 초기화
   // list_push_back (&cond->waiters, &waiter.elem);
   // codition waiters리스트에 현재 대기자 목록을 정렬해서 넣기
   list_insert_ordered(&cond->waiters, &waiter.elem, cmp_priority, NULL); // p_s2 : 비교해서 정렬하는거 추가
   // 현재 lock 해제
   lock_release(lock);
   // semaphore가 양수가 될때까지 기다렸다가 -1
   sema_down(&waiter.semaphore);
   // lock 요구
   lock_acquire(lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_signal(struct condition *cond, struct lock *lock UNUSED)
{
   ASSERT(cond != NULL);
   ASSERT(lock != NULL);
   ASSERT(!intr_context());
   ASSERT(lock_held_by_current_thread(lock));

   if (!list_empty(&cond->waiters))
      sema_up(&list_entry(list_pop_front(&cond->waiters),
                          struct semaphore_elem, elem)
                   ->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_broadcast(struct condition *cond, struct lock *lock)
{
   ASSERT(cond != NULL);
   ASSERT(lock != NULL);

   while (!list_empty(&cond->waiters))
      cond_signal(cond, lock);
}