#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "intrinsic.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
/* 사용자 프로그램에 의해 발생할 수 있는 인터럽트에 대한 핸들러를 등록합니다.

   실제 Unix와 유사한 OS에서는 이러한 대부분의 인터럽트가 [SV-386] 3-24 및 3-25에 설명된 대로
   신호(signals) 형태로 사용자 프로세스에 전달되지만, 우리는 신호를 구현하지 않습니다.
   대신, 이러한 인터럽트들은 단순히 사용자 프로세스를 종료시키는 역할을 합니다.

   페이지 폴트(page faults)는 예외입니다. 여기서는 다른 예외들과 같은 방식으로 처리되지만,
   가상 메모리를 구현하기 위해서는 이 부분을 변경해야 합니다.

   각각의 예외에 대한 설명은 [IA32-v3a] 섹션 5.15 "예외 및 인터럽트 참조(Exception and Interrupt Reference)"를 참조하십시오. */
void
exception_init (void) {
	/* These exceptions can be raised explicitly by a user program,
	   e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
	   we set DPL==3, meaning that user programs are allowed to
	   invoke them via these instructions. */
	/* 이 예외들은 사용자 프로그램에 의해 명시적으로 발생될 수 있습니다.
   예를 들어, INT, INT3, INTO, BOUND 명령어를 통해서 발생시킬 수 있습니다.
   따라서, 우리는 DPL(Descriptor Privilege Level)을 3으로 설정합니다.
   이는 사용자 프로그램이 이러한 명령어를 통해 이들을 호출할 수 있음을 의미합니다.
	 -> DPL==3으로 설정함으로써, 
	 사용자 모드에서 실행되는 프로그램이 이러한 예외를 발생시킬 수 있는 권한을 가지게 됩 */
	intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
	intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
	intr_register_int (5, 3, INTR_ON, kill,
			"#BR BOUND Range Exceeded Exception");

	/* These exceptions have DPL==0, preventing user processes from
	   invoking them via the INT instruction.  They can still be
	   caused indirectly, e.g. #DE can be caused by dividing by
	   0.  */
	/* 이 예외들은 DPL(Descriptor Privilege Level)을 0으로 가집니다.
    이는 사용자 프로세스가 INT 명령어를 통해 이들을 호출하는 것을 방지합니다.
    그러나, 이 예외들은 여전히 간접적으로 발생할 수 있습니다.
    예를 들어, #DE(0으로 나누기)는 0으로 나누어서 발생할 수 있습니다.
	  -> 이 설정은 사용자 모드에서 실행되는 프로세스가 INT 명령어를 사용하여 
	  이러한 예외를 직접적으로 호출하는 것을 방지 */
	intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
	intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
	intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
	intr_register_int (7, 0, INTR_ON, kill,
			"#NM Device Not Available Exception");
	intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
	intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
	intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
	intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
	intr_register_int (19, 0, INTR_ON, kill,
			"#XF SIMD Floating-Point Exception");

	/* Most exceptions can be handled with interrupts turned on.
	   We need to disable interrupts for page faults because the
	   fault address is stored in CR2 and needs to be preserved. */
	/* 대부분의 예외는 인터럽트가 켜진 상태에서 처리될 수 있습니다.
   하지만 페이지 폴트의 경우에는 인터럽트를 비활성화해야 합니다.
   이는 페이지 폴트 주소가 CR2 레지스터에 저장되며, 이 주소를 보존해야 하기 때문입니다. */
	intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) {
	printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) {
	/* This interrupt is one (probably) caused by a user process.
	   For example, the process might have tried to access unmapped
	   virtual memory (a page fault).  For now, we simply kill the
	   user process.  Later, we'll want to handle page faults in
	   the kernel.  Real Unix-like operating systems pass most
	   exceptions back to the process via signals, but we don't
	   implement them. */
	/* 이 인터럽트는 사용자 프로세스에 의해 발생한 것으로 추정됩니다.
	   예를 들어, 프로세스가 매핑되지 않은 가상 메모리에 접근을 시도했을 수 있습니다(페이지 폴트).
	   현재로서는, 우리는 단순히 사용자 프로세스를 종료시킵니다. 나중에는, 우리는 커널에서
	   페이지 폴트를 처리하고자 합니다. 실제 Unix와 유사한 운영 체제들은 대부분의 예외를
	   신호(signals)를 통해 프로세스에 다시 전달하지만, 우리는 이를 구현하지 않습니다. */

	/* The interrupt frame's code segment value tells us where the
	   exception originated. */
	/* 인터럽트 프레임의 코드 세그먼트 값은 예외가 발생한 원점을 알려줍니다. */
	switch (f->cs) {
		case SEL_UCSEG:
			/* User's code segment, so it's a user exception, as we
			   expected.  Kill the user process.  */
			printf ("%s: dying due to interrupt %#04llx (%s).\n",
					thread_name (), f->vec_no, intr_name (f->vec_no));
			intr_dump_frame (f);
			thread_exit ();

		case SEL_KCSEG:
			/* Kernel's code segment, which indicates a kernel bug.
			   Kernel code shouldn't throw exceptions.  (Page faults
			   may cause kernel exceptions--but they shouldn't arrive
			   here.)  Panic the kernel to make the point.  */
			intr_dump_frame (f);
			PANIC ("Kernel bug - unexpected interrupt in kernel");

		default:
			/* Some other code segment?  Shouldn't happen.  Panic the
			   kernel. */
			printf ("Interrupt %#04llx (%s) in unknown segment %04x\n",
					f->vec_no, intr_name (f->vec_no), f->cs);
			thread_exit ();
	}
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
/* 페이지 폴트 핸들러. 이것은 가상 메모리를 구현하기 위해 채워져야 할 스켈레톤입니다.
   프로젝트 2의 일부 해결책은 이 코드를 수정할 수도 있습니다.

   진입 시, 폴트가 발생한 주소는 CR2(제어 레지스터 2)에 있으며,
   폴트에 대한 정보는 exception.h의 PF_* 매크로로 설명된 형식의 F의 error_code 멤버에 있습니다.
   여기에 있는 예제 코드는 그 정보를 파싱하는 방법을 보여줍니다.
   이들에 대한 더 많은 정보는 [IA32-v3a] 섹션 5.15 "예외 및 인터럽트 참조"에서
   "인터럽트 14--페이지 폴트 예외 (#PF)"의 설명에서 찾을 수 있습니다.
*/
/*
 * page_fault 함수는 시스템이 메모리 접근 실패를 처리할 때 호출됩니다. 
 * 페이지 폴트는 프로그램이 유효하지 않거나,
 * 현재 메모리에 로드되지 않은 주소에 접근하려고 할 때 발생합니다.
*/
static void
page_fault (struct intr_frame *f) {
	bool not_present;  /* True: not-present page, false: writing r/o page. */
	bool write;        /* True: access was write, false: access was read. */
	bool user;         /* True: access by user, false: access by kernel. */
	void *fault_addr;  /* Fault address. */

	/* Obtain faulting address, the virtual address that was
	   accessed to cause the fault.  It may point to code or to
	   data.  It is not necessarily the address of the instruction
	   that caused the fault (that's f->rip). */

	fault_addr = (void *) rcr2(); // 폴트가 발생한 주소입니다.

	/* Turn interrupts back on (they were only off so that we could
	   be assured of reading CR2 before it changed). */

	intr_enable ();

	/* Determine cause. */
	not_present = (f->error_code & PF_P) == 0; // 페이지가 물리 메모리에 없을 때 true, 쓰기 금지 페이지에 쓰려고 할 때 false입니다.
	write = (f->error_code & PF_W) != 0; // 접근 시도가 쓰기 였을 때 true, 읽기 였을 때 false입니다.
	user = (f->error_code & PF_U) != 0; // 사용자 모드에서의 접근 시도일 때 true, 커널 모드에서의 접근 시도일 때 false입니다.

	// 구현하기: Call exit(-1) -> print the thread name and the exit status -1
	// sys_exit(-1); // 추가

#ifdef VM
	/* For project 3 and later. */
	if (vm_try_handle_fault (f, fault_addr, user, write, not_present))
		return;
#endif

	/* Count page faults. */
	page_fault_cnt++;

	/* If the fault is true fault, show info and exit. */
	printf ("Page fault at %p: %s error %s page in %s context.\n",
			fault_addr,
			not_present ? "not present" : "rights violation",
			write ? "writing" : "reading",
			user ? "user" : "kernel");
	kill (f);
}

