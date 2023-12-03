// 초당 100번 똑딱거리는 시스템 타이머
#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/fixed_point.h" // mlfqs 관련 고정 소수점 헤더 파일

/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;
// static int64_t wakeup_ticks;

/* Number of loops per timer tick.
	 Initialized by timer_calibrate(). */
static unsigned loops_per_tick; // 타이머 틱당 반복 루프의 수, 짧은 지연 시간을 구현하는 데 사용됨

static intr_handler_func timer_interrupt;
static bool too_many_loops(unsigned loops);
static void busy_wait(int64_t loops);
static void real_time_sleep(int64_t num, int32_t denom);

void thread_sleep(int64_t ticks);	 // 추가한 함수
void thread_wakeup(int64_t ticks); // 추가한 함수

/* Sets up the 8254 Programmable Interval Timer (PIT) to
	 interrupt PIT_FREQ times per second, and registers the
	 corresponding interrupt.
	 타이머 초기화, 초당 PIT_FREQ번 인터럽트가 발생하도록 함, 타이머 인터럽트 핸들러를 등록.*/
void timer_init(void)
{
	/* 8254 input frequency divided by TIMER_FREQ, rounded to
		 nearest. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

	outb(0x43, 0x34); /* CW: counter 0, LSB then MSB, mode 2, binary. */
	outb(0x40, count & 0xff);
	outb(0x40, count >> 8);

	intr_register_ext(0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays.
	시스템의 타이머 틱당 반복 루프 수를 캘리브레이션(보정)한다.*/
void timer_calibrate(void)
{
	unsigned high_bit, test_bit;

	ASSERT(intr_get_level() == INTR_ON);
	printf("Calibrating timer...  ");

	/* Approximate loops_per_tick as the largest power-of-two
		 still less than one timer tick. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops(loops_per_tick << 1))
	{
		loops_per_tick <<= 1;
		ASSERT(loops_per_tick != 0);
	}

	/* Refine the next 8 bits of loops_per_tick. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops(high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf("%'" PRIu64 " loops/s.\n", (uint64_t)loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted.
	운영 체제가 부팅된 이후 경과한 타이머 틱의 수를 반환 */
int64_t
timer_ticks(void)
{																							// os 부팅 후 타이머 틱 수를 반환
	enum intr_level old_level = intr_disable(); // 인터럽트를 비활성화하고 이전 인터럽트 상태를 반환
	int64_t t = ticks;
	intr_set_level(old_level); /*LEVEL에 지정된 대로 인터럽트를 활성화 또는 비활성화하고는 이전 인터럽트 상태를 반환*/
	barrier();
	return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
	 should be a value once returned by timer_ticks().
	 타이머 틱 경과 시간 계산 주어진 시간 then 이후로*/
int64_t
timer_elapsed(int64_t then)
{
	return timer_ticks() - then;
}

/* Suspends execution for approximately TICKS timer ticks. */
void timer_sleep(int64_t ticks)
{																 // 원하는 tick을 지정 해두고 해당 tick만큼 지나면 yield()를 멈춰라
	int64_t start = timer_ticks(); // start = 시작 시간

	ASSERT(intr_get_level() == INTR_ON);
	// busy wait
	// while (timer_elapsed (start) < ticks) //종료시간이 안되어도 계속 확인을 해 cpu 사용량이 많다. 이를 줄이는 방법을 생각해야할듯?!
	// 	thread_yield ();

	if (timer_elapsed(start) < ticks) // 아직 깨울시간이 안되었을때
		thread_sleep(start + ticks);
}

/* Suspends execution for approximately MS milliseconds. */
void timer_msleep(int64_t ms)
{
	real_time_sleep(ms, 1000);
}

/* Suspends execution for approximately US microseconds. */
void timer_usleep(int64_t us)
{
	real_time_sleep(us, 1000 * 1000);
}

/* Suspends execution for approximately NS nanoseconds. */
void timer_nsleep(int64_t ns)
{
	real_time_sleep(ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void timer_print_stats(void)
{
	printf("Timer: %" PRId64 " ticks\n", timer_ticks());
}

/* Timer interrupt handler.
	각 타이머 틱마다 실행 중인 스레드의 recent_cpu는 1씩 증가,
	매초마다, 모든 스레드의 recent_cpu는 다음과 같은 방식으로 업데이트:
	recent_cpu = (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice
	
	타이머 인터럽트가 발생할 때마다 호출되어 'ticks'를 중가시키고,
	스레드 스케줄링을 위한 thread_tick() 함수를 호출
	100ticks = 1sec */
static void timer_interrupt(struct intr_frame *args UNUSED)
{
	ticks++;
	thread_tick();
	thread_wakeup(ticks);

	if (thread_mlfqs) // mlfqs 스케줄러 일 경우
	{
		struct thread *cur = thread_current();
		cur->recent_cpu += FC; // 현재 쓰레드의 recent_cpu 값은 각 틱마다 1 증가(timer_interrupt가 발생할 때마다)

		if (timer_ticks() % 4 == 0) // 매 4틱마다 모든 쓰레드의 우선순위 업데이트
		{
			// update_thread_priority(); // 모든 쓰레드 어떻게 순회하지?
		}

		if (timer_ticks() % TIMER_FREQ == 0) // 매초마다 모든 쓰레드의 recent_cpu 업데이트
		{																		 // 1초마다 load_avg, recent_cpu, priority 계산?
			// update_recent_cpu(); // 모든 쓰레드 어떻게 순회하지?
		}
	}
}

// 쓰레드 t의 우선순위 계산 후 업데이트(mlfgs)
void update_thread_priority(struct thread *t)
{
	/* 해당 스레드가 idle_thread 가 아닌지 검사 */
	/* priority계산식을 구현 (fixed_point.h의 계산함수 이용) */
}

// 쓰레드 t의 recent_cpu 계산 후 업데이트(mlfgs)
void update_recent_cpu(struct thread *t)
{
	// 현재 쓰레드와 ready_list의 쓰레드 업데이트
	struct thread *cur = thread_current();
	/* 해당 스레드가 idle_thread 가 아닌지 검사 */
	/* recent_cpu계산식을 구현 (fixed_point.h의 계산함수 이용) */
}

/* Returns true if LOOPS iterations waits for more than one timer
	 tick, otherwise false. */
static bool
too_many_loops(unsigned loops)
{
	/* Wait for a timer tick. */
	int64_t start = ticks;
	while (ticks == start)
		barrier();

	/* Run LOOPS loops. */
	start = ticks;
	busy_wait(loops);

	/* If the tick count changed, we iterated too long. */
	barrier();
	return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
	 brief delays.

	 Marked NO_INLINE because code alignment can significantly
	 affect timings, so that if this function was inlined
	 differently in different places the results would be difficult
	 to predict. */
static void NO_INLINE
busy_wait(int64_t loops)
{
	while (loops-- > 0)
		barrier();
}

/* Sleep for approximately NUM/DENOM seconds.
	주어진 시간 동안 스레드의 실행을 중지하는데 사용된다.
	 */
static void
real_time_sleep(int64_t num, int32_t denom)
{
	/* Convert NUM/DENOM seconds into timer ticks, rounding down.

		 (NUM / DENOM) s
		 ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
		 1 s / TIMER_FREQ ticks
		 */
	int64_t ticks = num * TIMER_FREQ / denom;
	//											 인터럽트_온
	ASSERT(intr_get_level() == INTR_ON); // 이 함수가 인터럽트가 가능한 상태에서만 호출되어야 함을 보증
	if (ticks > 0)											 // 틱이 양수인 경우:
	{																		 //	timer_sleep 함수를 호출해서 CPU 양보하고 다른 프로세스가 실행될 수 있도록 함
		/* We're waiting for at least one full timer tick.  Use
			 timer_sleep() because it will yield the CPU to other
			 processes. */
		timer_sleep(ticks);
	}
	else // 틱이 0 또는 음수인 경우:
	{		 // 정확한 서브틱 타이밍을 위해 busy-wait 루프를 이용
		/* Otherwise, use a busy-wait loop for more accurate
			 sub-tick timing.  We scale the numerator and denominator
			 down by 1000 to avoid the possibility of overflow. */
		ASSERT(denom % 1000 == 0);
		busy_wait(loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}
