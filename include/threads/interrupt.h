#ifndef THREADS_INTERRUPT_H
#define THREADS_INTERRUPT_H

#include <stdbool.h>
#include <stdint.h>

/* Interrupts on or off? */
enum intr_level {
	INTR_OFF,             /* Interrupts disabled. */
	INTR_ON               /* Interrupts enabled. */
};

enum intr_level intr_get_level (void);
enum intr_level intr_set_level (enum intr_level);
enum intr_level intr_enable (void);
enum intr_level intr_disable (void);

/* Interrupt stack frame. */
struct gp_registers {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;  // 인자 전달되는 순서: 6번
	uint64_t r8;  // 인자 전달되는 순서: 5번
	uint64_t rsi; // 인자 전달되는 순서: 2번
	uint64_t rdi; // 인자 전달되는 순서: 1번
	uint64_t rbp;
	uint64_t rdx; // 인자 전달되는 순서: 3번 
	uint64_t rcx; // 인자 전달되는 순서: 4번
	uint64_t rbx;
	uint64_t rax; // 함수의 반환 값
} __attribute__((packed));

struct intr_frame {
	/* Pushed by intr_entry in intr-stubs.S.
	  These are the interrupted task's saved registers. */
	/* intr_entry에 의해 intr-stubs.S에서 푸시됩니다.
		이것들은 중단된 태스크의 저장된 레지스터들입니다. */

	struct gp_registers R;
	uint16_t es;
	uint16_t __pad1;
	uint32_t __pad2;
	uint16_t ds;
	uint16_t __pad3;
	uint32_t __pad4;
	/* Pushed by intrNN_stub in intr-stubs.S. */
	/* intrNN_stub에 의해 intr-stubs.S에서 푸시됩니다. */
	uint64_t vec_no; /* Interrupt vector number(인터럽트 벡터 번호): 어떤 종류의 인터럽트가 발생했는지 */
/* Sometimes pushed by the CPU,
   otherwise for consistency pushed as 0 by intrNN_stub.
   The CPU puts it just under `eip', but we move it here. */
/* CPU에 의해 때때로 푸시됩니다,
	  그렇지 않으면 일관성을 위해 intrNN_stub에 의해 0으로 푸시됩니다.
	  CPU는 `rip` 바로 아래에 이것을 놓지만, 우리는 여기로 옮깁니다. */
	uint64_t error_code;
/* Pushed by the CPU.
  These are the interrupted task's saved registers. */
/* CPU에 의해 푸시됩니다.
	이것들은 중단된 태스크의 저장된 레지스터들입니다. */
	uintptr_t rip;		// 5
	uint16_t cs;			// 4
	uint16_t __pad5;
	uint32_t __pad6;
	uint64_t eflags;	// 3
	uintptr_t rsp; 		// 2
	uint16_t ss; 			// 1
	uint16_t __pad7;
	uint32_t __pad8;
} __attribute__((packed));

typedef void intr_handler_func (struct intr_frame *);

void intr_init (void);
void intr_register_ext (uint8_t vec, intr_handler_func *, const char *name);
void intr_register_int (uint8_t vec, int dpl, enum intr_level,
                        intr_handler_func *, const char *name);
bool intr_context (void);
void intr_yield_on_return (void);

void intr_dump_frame (const struct intr_frame *);
const char *intr_name (uint8_t vec);

#endif /* threads/interrupt.h */
