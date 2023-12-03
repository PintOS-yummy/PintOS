#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed_point.h" // mlfqs 관련 고정 소수점 헤더 파일
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
	 Used to detect stack overflow.  See the big comment at the top
	 of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
	 Do not modify this value. */
#define THREAD_BASIC 0xd42df210

bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
	struct thread *a_t = list_entry(a, struct thread, elem);
	struct thread *b_t = list_entry(b, struct thread, elem);
	return a_t->priority > b_t->priority; // a->priority > b->priority 이면 true
}

bool thread_cmp_donate_priority(const struct list_elem *l, const struct list_elem *s, void *aux UNUSED)
{
	return list_entry(l, struct thread, d_elem)->priority > list_entry(s, struct thread, d_elem)->priority;
}

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
static struct list destruction_req; // 쓰레드 삭제 요청

/* Statistics. */
static long long idle_ticks;	 /* # of timer ticks spent idle. */
static long long kernel_ticks; /* # of timer ticks in kernel threads. */
static long long user_ticks;	 /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4					/* # of timer ticks to give each thread. */
static unsigned thread_ticks; /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
	 If true, use multi-level feedback queue scheduler.
	 Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule(void);
static tid_t allocate_tid(void);
void thread_sleep(int64_t ticks);
void thread_wakeup(int64_t ticks);

/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
#define running_thread() ((struct thread *)(pg_round_down(rrsp())))

// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = {0, 0x00af9a000000ffff, 0x00cf92000000ffff};

/* Initializes the threading system by transforming the code
	 that's currently running into a thread.  This can't work in
	 general and it is possible in this case only because loader.S
	 was careful to put the bottom of the stack at a page boundary.

	 Also initializes the run queue and the tid lock.

	 After calling this function, be sure to initialize the page
	 allocator before trying to create any threads with
	 thread_create().

	 It is not safe to call thread_current() until this function
	 finishes. */
void thread_init(void)
{
	ASSERT(intr_get_level() == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	struct desc_ptr gdt_ds = {
			.size = sizeof(gdt) - 1,
			.address = (uint64_t)gdt};
	lgdt(&gdt_ds);

	/* Init the globla thread context */
	lock_init(&tid_lock);
	list_init(&ready_list);
	list_init(&sleep_list);
	list_init(&destruction_req);

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread();
	init_thread(initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid();
}

/* Starts preemptive thread scheduling by enabling interrupts.
	 Also creates the idle thread. */
void thread_start(void)
{
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init(&idle_started, 0);
	thread_create("idle", PRI_MIN, idle, &idle_started);
	load_avg = LOAD_AVG_DEFAULT; // load_avg 0으로 초기화

	/* Start preemptive thread scheduling. */
	intr_enable();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down(&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
	 Thus, this function runs in an external interrupt context.
	 타이머 인터럽트 핸들러에 의해 각 타이머 틱(tick)마다 호출 */
void thread_tick(void)
{
	struct thread *t = thread_current();

	/* Update statistics. */
	if (t == idle_thread) // 쓰레드가 idle_thread인 경우,
		idle_ticks++;				// idle_ticks 증가시켜서 시스템이 얼마나 유휴 상태였는지 추적
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption.
		사전 선점 실행(TIME_SLICE=한 쓰레드가 연속해서 실행할 수 있는 최대 시간)*/
	if (++thread_ticks >= TIME_SLICE) // thread_ticks 증가시키고, TIME_SLICE보다 크거나 같아지면,
		intr_yield_on_return();					// 선점 실행, 현재 쓰레드가 타임 슬라이스를 소진했을 때,
																		// 인터럽트 서비스 루틴이 반환될 때 다른 스레드로 전환하도록 요청.
																		// 다음에 가능한 시점에 컨텍스트 스위치를 수행하도록 스케줄러에 신호를 보냅니다.
}

void thread_sleep(int64_t ticks)
{
	struct thread *cur;
	enum intr_level old_level;

	old_level = intr_disable(); // 인터럽트 off  //인터럽트를 비활성화하고 이전 인터럽트 상태를 반환
	cur = thread_current();

	ASSERT(cur != idle_thread);

	cur->wakeup_ticks = ticks;							 // 일어날 시간을 저장
	list_push_back(&sleep_list, &cur->elem); // sleep_list 에 추가
	thread_block();													 // block 상태로 변경 // 여기서 현재 thread->status = THREAD_BLOCKE, schedule()이 동작함

	intr_set_level(old_level); // old_level에 지정된 대로 인터럽트를 활성화 또는 비활성화하고는 이전 인터럽트 상태를 반환
														 // return level == INTR_ON ? intr_enable () : intr_disable ();
														 // INTR_ON = True : 인터럽트를 받을 수 있는 상황 intr_enable () <-> 받을 수 없는 상황 intr_disable()
}

void thread_wakeup(int64_t ticks)
{ // list_begin을 통해서 스레드가 일어날 시간이 되었는지 확인 후 되었으면 unblock. list_remove() 안되었으면 list_next()
	// sleep list를 다 확인해주는 방법밖에 없나?!
	struct list_elem *element = list_begin(&sleep_list); // list_begin, list_next
	while (element != list_end(&sleep_list))
	{
		struct thread *t = list_entry(element, struct thread, elem);

		if (t->wakeup_ticks <= ticks)
		{ // 깰 시간이야~
			element = list_remove(&t->elem);
			thread_unblock(t); // 여기서 현재 thread->status = THREAD_READY, list_push_back (&ready_list)를 실행함
		}
		else
		{ // 아직 깰시간 아니야 다음 리스트 값 확인
			element = list_next(element);
		}
	}
}

/* Prints thread statistics. */
void thread_print_stats(void)
{
	printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
				 idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
	 PRIORITY, which executes FUNCTION passing AUX as the argument,
	 and adds it to the ready queue.  Returns the thread identifier
	 for the new thread, or TID_ERROR if creation fails.

	 If thread_start() has been called, then the new thread may be
	 scheduled before thread_create() returns.  It could even exit
	 before thread_create() returns.  Contrariwise, the original
	 thread may run for any amount of time before the new thread is
	 scheduled.  Use a semaphore or some other form of
	 synchronization if you need to ensure ordering.

	 The code provided sets the new thread's `priority' member to
	 PRIORITY, but no actual priority scheduling is implemented.
	 Priority scheduling is the goal of Problem 1-3. */

/* 주어진 이니셜을 가진 NAME이라는 이름의 새 커널 스레드를 생성합니다.
우선순위를 가진 새로운 커널 스레드를 생성하고, 이 스레드는 AUX를 인수로 전달하는 FUNCTION을 실행합니다,
를 실행하여 준비 큐에 추가합니다.  새 스레드의 스레드 식별자
를 반환하고, 생성에 실패하면 TID_ERROR를 반환합니다.

thread_start()가 호출된 경우, 새 스레드가
새 스레드가 스케줄링될 수 있습니다.  심지어 thread_create()가 반환되기 전에
종료할 수도 있습니다.  반대로, 원래의
스레드는 새 스레드가 스케줄되기 전까지 얼마든지 실행될 수 있습니다.
스케줄링할 수 있습니다.  순서를 보장해야 하는 경우 세마포어 또는 다른 형태의
동기화를 사용하세요.

제공된 코드는 새 스레드의 '우선순위' 멤버를
PRIORITY로 설정하지만 실제 우선순위 스케줄링은 구현되지 않습니다.
우선순위 스케줄링은 문제 1-3의 목표입니다. */

tid_t thread_create(const char *name, int priority,
										thread_func *function, void *aux)
{ // priority scheduling 수정해야 할것!
	struct thread *t;
	struct thread *curr;
	tid_t tid;

	ASSERT(function != NULL);

	/* Allocate thread. */
	t = palloc_get_page(PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread(t, name, priority);
	tid = t->tid = allocate_tid();

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	t->tf.rip = (uintptr_t)kernel_thread;
	t->tf.R.rdi = (uint64_t)function;
	t->tf.R.rsi = (uint64_t)aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock(t);

	// 선점 방식 구현(priority-FIFO FAIL->PASS)
	/* 현재 실행중인 thread의 우선순위와 비교하고, 새로운 thread를 삽입해야한다.
	만약 새로운 thread가 더 높은 우선순위를 가졌다면 yield를 해야한다 */
	resort_priority();

	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
	 again until awoken by thread_unblock().

	 This function must be called with interrupts turned off.  It
	 is usually a better idea to use one of the synchronization
	 primitives in synch.h. */
void thread_block(void)
{
	ASSERT(!intr_context());
	ASSERT(intr_get_level() == INTR_OFF);
	thread_current()->status = THREAD_BLOCKED;
	schedule();
}

/* Transitions a blocked thread T to the ready-to-run state.
	 This is an error if T is not blocked.  (Use thread_yield() to
	 make the running thread ready.)

	 This function does not preempt the running thread.  This can
	 be important: if the caller had disabled interrupts itself,
	 it may expect that it can atomically unblock a thread and
	 update other data. */

void thread_unblock(struct thread *t)
{
	enum intr_level old_level;
	ASSERT(is_thread(t));
	old_level = intr_disable();
	ASSERT(t->status == THREAD_BLOCKED);
	// list_push_back (&ready_list, &t->elem); //이걸 list_insert_ordered 로 교체
	list_insert_ordered(&ready_list, &t->elem, cmp_priority, 0);
	t->status = THREAD_READY;
	intr_set_level(old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name(void)
{
	return thread_current()->name;
}

/* Returns the running thread.
	 This is running_thread() plus a couple of sanity checks.
	 See the big comment at the top of thread.h for details. */
struct thread *
thread_current(void)
{ // 현재 실행중인 쓰레드 반환, running_thread에 검사 추가한것
	struct thread *t = running_thread();

	/* Make sure T is really a thread.
		 If either of these assertions fire, then your thread may
		 have overflowed its stack.  Each thread has less than 4 kB
		 of stack, so a few big automatic arrays or moderate
		 recursion can cause stack overflow. */
	ASSERT(is_thread(t));
	ASSERT(t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t thread_tid(void)
{
	return thread_current()->tid;
}

/* Deschedules the current thread and destroys it.  Never
	 returns to the caller. */
void thread_exit(void)
{
	ASSERT(!intr_context());

#ifdef USERPROG
	process_exit();
#endif

	/* Just set our status to dying and schedule another process.
		 We will be destroyed during the call to schedule_tail(). */
	intr_disable();
	do_schedule(THREAD_DYING);
	NOT_REACHED();
}

/* Yields the CPU.  The current thread is not put to sleep and
	 may be scheduled again immediately at the scheduler's whim. */
void thread_yield(void)
{ // 다른 쓰레드에게 양보하는 것 //priority scheduling 수정해야 할것!
	struct thread *cur = thread_current();
	enum intr_level old_level;

	ASSERT(!intr_context());
	old_level = intr_disable();

	// list_push_back (&ready_list, &cur->elem); //이걸 list_insert_ordered로 교체
	if (cur != idle_thread)
		list_insert_ordered(&ready_list, &cur->elem, cmp_priority, 0);

	do_schedule(THREAD_READY);
	intr_set_level(old_level);
}

void resort_priority(void)
{ // priority scheduling 추가 한것! //현재 실행중인 thread가 ready_list 맨 앞의 값보다 우선순위가 낮은 경우
	// thread_yield()를 해주면 yield 안에서 다시 list 재정렬을 해주고 다시 schedule()

	struct thread *curr = thread_current();
	struct thread *list_begin_thread = list_entry(list_begin(&ready_list), struct thread, elem);

	if (!list_empty(&ready_list) && curr->priority < list_begin_thread->priority)
		thread_yield();
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void thread_set_priority(int new_priority)
{ // priority scheduling 수정해야 할 것! // 현재 thread의 우선순위가 삽입하는 thread 우선순위보다 낮은 경우 수정 필요

	if (!thread_mlfqs) // 수정
	{
		thread_current()->org_priority = new_priority;
		refresh_priority();
	}
	else
	{
		/* mlfqs 스케줄러 일때 우선순위를 임의로 변경할수 없도록 한다. */
	}
	
	resort_priority();
}

/* Returns the current thread's priority.
	이 함수를 호출한 쓰레드의 우선순위 반환 */
int thread_get_priority(void)
{
	// if (!thread_mlfqs) // mlfqs 테스트 케이스가 아닐때 실행하는 부분
	// {
	// }
	return thread_current()->priority;
}

/* Sets the current thread's nice value to NICE. */
// 현재 thread의 nice값을 nice로 설정
void thread_set_nice(int nice)
{
	/* 쓰레드를 수정하므로 해당 작업중에 인터럽트는 비활성화 해야 한다. */
	enum intr_level old_level;
	old_level = intr_disable(); // 인터럽트 비활성화

	struct thread *cur = thread_current();
	cur->nice = nice; // 현재 쓰레드의 나이스 값을 바꿔줌

	int new_priority = convert_x_to_int_round_to_zero(convert_n_to_fp(PRI_MAX) - convert_n_to_fp((cur->recent_cpu / 4)) - convert_n_to_fp((cur->nice * 2)));
	thread_set_priority(new_priority);

	resort_priority(); // 우선순위에 의해 스케줄?

	intr_set_level(old_level); // 인터럽트 활성화
}

/* Returns the current thread's nice value.
	해당 작업중에 인터럽트는 비활성되어야 한다. */
int thread_get_nice(void) // 수정
{
	enum intr_level old_level;
	old_level = intr_disable(); // 인터럽트 비활성화

	struct thread *cur = thread_current();

	return cur->nice;

	intr_set_level(old_level); // 인터럽트 활성화
}

/* Returns 100 times the system load average.
해당 작업중에 인터럽트는 비활성되어야 한다. */
int thread_get_load_avg(void) // 수정
{
	enum intr_level old_level;
	old_level = intr_disable(); // 인터럽트 비활성화

	return convert_x_to_int_round_to_nearest(load_avg * 100);

	intr_set_level(old_level); // 인터럽트 활성화
}

/* Returns 100 times the current thread's recent_cpu value.
	현재 쓰레드의 recent_cpu 값 얻기
	해당 과정중에 인터럽트는 비활성되어야 한다. */
int thread_get_recent_cpu(void) // 수정
{
	enum intr_level old_level;
	old_level = intr_disable(); // 인터럽트 비활성화

	struct thread *cur = thread_current();

	return convert_x_to_int_round_to_nearest(cur->recent_cpu * 100);

	intr_set_level(old_level); // 인터럽트 활성화
}

/* Idle thread.  Executes when no other thread is ready to run.

	 The idle thread is initially put on the ready list by
	 thread_start().  It will be scheduled once initially, at which
	 point it initializes idle_thread, "up"s the semaphore passed
	 to it to enable thread_start() to continue, and immediately
	 blocks.  After that, the idle thread never appears in the
	 ready list.  It is returned by next_thread_to_run() as a
	 special case when the ready list is empty. */
static void idle(void *idle_started_ UNUSED)
{
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current();
	sema_up(idle_started);

	for (;;)
	{
		/* Let someone else run. */
		intr_disable();
		thread_block();

		/* Re-enable interrupts and wait for the next one.

			 The `sti' instruction disables interrupts until the
			 completion of the next instruction, so these two
			 instructions are executed atomically.  This atomicity is
			 important; otherwise, an interrupt could be handled
			 between re-enabling interrupts and waiting for the next
			 one to occur, wasting as much as one clock tick worth of
			 time.

			 See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
			 7.11.1 "HLT Instruction". */
		asm volatile("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread(thread_func *function, void *aux)
{
	ASSERT(function != NULL);

	intr_enable(); /* The scheduler runs with interrupts off. */
	function(aux); /* Execute the thread function. */
	thread_exit(); /* If function() returns, kill the thread. */
}

/* Does basic initialization of T as a blocked thread named
	 NAME. */
static void init_thread(struct thread *t, const char *name, int priority)
{
	ASSERT(t != NULL);
	ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT(name != NULL);

	memset(t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy(t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t)t + PGSIZE - sizeof(void *);
	t->priority = priority;
	t->magic = THREAD_MAGIC;

	// struct list donations; //donation 변수
	// struct list_elem d_elem; // donation list elem 저장
	// struct lock wait_on_lock; //기다리는 lock이 무엇인지 저장해 줄 변수
	// int org_priority; //원래 priority를 저장해둘 변수
	t->org_priority = priority;
	t->wait_on_lock = NULL;
	list_init(&t->donations);

	t->nice = NICE_DEFAULT;							// nice 값 초기화: 0
	t->recent_cpu = RECENT_CPU_DEFAULT; // recent_cpu 값 초기화: 0
}

/* Chooses and returns the next thread to be scheduled.  Should
	 return a thread from the run queue, unless the run queue is
	 empty.  (If the running thread can continue running, then it
	 will be in the run queue.)  If the run queue is empty, return
	 idle_thread. */
static struct thread *next_thread_to_run(void)
{
	if (list_empty(&ready_list))
		return idle_thread;
	else
		return list_entry(list_pop_front(&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
void do_iret(struct intr_frame *tf)
{
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g"((uint64_t)tf) : "memory");
}

/* Switching the thread by activating the new thread's page
	 tables, and, if the previous thread is dying, destroying it.

	 At this function's invocation, we just switched from thread
	 PREV, the new thread is already running, and interrupts are
	 still disabled.

	 It's not safe to call printf() until the thread switch is
	 complete.  In practice that means that printf()s should be
	 added at the end of the function. */
static void thread_launch(struct thread *th)
{
	uint64_t tf_cur = (uint64_t)&running_thread()->tf;
	uint64_t tf = (uint64_t)&th->tf;
	ASSERT(intr_get_level() == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile(
			/* Store registers that will be used. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* Fetch input once */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n" // Saved rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n" // Saved rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n" // Saved rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n" // read the current rip.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"	 // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g"(tf) : "memory");
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
static void
do_schedule(int status)
{
	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(thread_current()->status == THREAD_RUNNING);
	while (!list_empty(&destruction_req))
	{													// 리스트가 비어있지 않다면
		struct thread *victim = // ready_list의 앞에껄 pop해옴
				list_entry(list_pop_front(&destruction_req), struct thread, elem);
		palloc_free_page(victim); // 해당 쓰레드를 ready_list에서 pop해주고 free해줌
	}
	thread_current()->status = status;
	schedule(); // 다음 쓰레드를 실행해줌
}

// 현재 실행 중인 스레드에서 다음 실행할 스레드로 문맥전환을 수행
static void schedule(void)
{
	struct thread *curr = running_thread();
	struct thread *next = next_thread_to_run();

	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(curr->status != THREAD_RUNNING);
	ASSERT(is_thread(next));
	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate(next);
#endif

	if (curr != next)
	{ // 현재 실행할 쓰레드와 다음 실행할 쓰레드가 다른 쓰레드이면
		/* If the thread we switched from is dying, destroy its struct
			 thread. This must happen late so that thread_exit() doesn't
			 pull out the rug under itself.
			 We just queuing the page free reqeust here because the page is
			 currently used by the stack.
			 The real destruction logic will be called at the beginning of the
			 schedule(). */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread)
		{ // 현재 쓰레드가 파괴되지 않았고, 제일 처음 쓰레드가 아니라면
			ASSERT(curr != next);
			list_push_back(&destruction_req, &curr->elem); // 쓰레드를 삭제 리스트(destruction_req)에 넣어줌
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		thread_launch(next); // 현재 쓰레드를 파괴하고 다음 쓰레드 실행
	}
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid(void)
{
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire(&tid_lock);
	tid = next_tid++;
	lock_release(&tid_lock);

	return tid;
}

/* donate */
void donate_priority(void)
{
	int depth;
	struct thread *c_t = thread_current();

	for (depth = 0; depth < 8; depth++)
	{
		if (!c_t->wait_on_lock)
			break;
		struct thread *holder = c_t->wait_on_lock->holder;
		holder->priority = c_t->priority;
		c_t = holder;
	}
}

void remove_with_lock(struct lock *lock) // donations list에서 thread를 지움
{
	struct list_elem *e;
	struct thread *cur = thread_current();

	for (e = list_begin(&cur->donations); e != list_end(&cur->donations); e = list_next(e))
	{
		struct thread *t = list_entry(e, struct thread, d_elem);
		if (t->wait_on_lock == lock) // thread가 기다리는 lock이 release 하는 lock이라면 해당 thread에서 지워줌
			list_remove(&t->d_elem);
	}
}

void refresh_priority(void) // priority를 재설정하는 함수
{
	struct thread *cur = thread_current();

	cur->priority = cur->org_priority;

	if (!list_empty(&cur->donations))
	{																														 // donations 리스트가 비어있지 않다면
		list_sort(&cur->donations, thread_cmp_donate_priority, 0); // 남은 thread를 sort해서

		struct thread *front = list_entry(list_front(&cur->donations), struct thread, d_elem); // 가장 높은 priority를 가진 thread를 가져와서
		if (front->priority > cur->priority)																									 // 리스트 안의 가장 높은 우선순위를 가진 thread가 현재 thread우선순위보다 높다면
			cur->priority = front->priority;																										 // 현재 thread의 우선순위에 더 높은 우선순위를 넣어준다.
	}
}