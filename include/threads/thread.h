#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status
{
	THREAD_RUNNING, /* Running thread. */
	THREAD_READY,		/* Not running but ready to run. */
	THREAD_BLOCKED, /* Waiting for an event to trigger. */
	THREAD_DYING		/* About to be destroyed. */
};

/* Thread identifier type.
	 You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0			 /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63		 /* Highest priority. */

/* macros for mlfqs
	n : integer, 
	x,y : fixed point numbers*/
#define FC 16384  // 2^14, 고정소수점에서의 소수부를 나타내는 비트 수
#define convert_n_to_fp(n) ((n) * (FC)) // 정수 n을 고정소수점으로 변환
#define convert_x_to_int_round_to_zero(x) ((x) / (FC)) // 고정소수점 수 x를 정수로 변환 (반내림)
#define convert_x_to_int_round_to_nearest(x) ((x) >= 0 ? (((x) + ((FC) / 2)) / FC) : ((((x) - ((FC) / 2)) / FC))) // 고정소수점 수 x를 정수로 변환 (반올림)
#define add_x_and_y(x,y) ((x) + (y)) // 두 고정소수점 수 x와 y의 합
#define sub_y_from_x(x,y) ((x) - (y)) // 두 고정소수점 수 x와 y의 차 (x - y)
#define add_x_and_n(x,n) ((x)+((n) * (FC))) // 고정소수점 수 x와 정수 n의 합
#define sub_n_from_x(x,n) ((x) - ((n) * (FC))) // 고정소수점 수 x에서 정수 n을 뺀 값 (x - n)
#define mul_x_by_y(x,y) (((int64_t) (x)) * (y) / (FC)) // 두 고정소수점 수 x와 y의 곱
#define mul_x_by_n(x,n) ((x) * (n)) // 고정소수점 수 x와 정수 n의 곱
#define div_x_by_y(x,y) (((int64_t) (x)) * (FC) / (y)) // 고정소수점 수 x를 y로 나눈 값
#define div_x_by_n(x,n) ((x) / (n)) // 고정소수점 수 x를 정수 n으로 나눈 값

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread
{
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */
	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */
	int64_t wakeup_ticks;

	//donate에 필요한 변수
	struct list donations; //donation 변수
	struct list_elem d_elem; // donation list elem 저장
	struct lock *wait_on_lock; //기다리는 lock이 무엇인지 저장해 줄 변수
	int org_priority; //원래 priority를 저장해둘 변수

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4; /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf; /* Information for switching */
	unsigned magic;				/* Detects stack overflow. */

	int nice;
	int recent_cpu;
};

// 쓰레드 a와 b의 우선순위 비교, a>b가 참이면 True 반환
bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
bool sema_cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

/* If false (default), use round-robin scheduler.
	 If true, use multi-level feedback queue scheduler.
	 Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

void thread_sleep(int64_t ticks);
void thread_wakeup(int64_t ticks);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void do_iret(struct intr_frame *tf);

#endif /* threads/thread.h */
