#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h" // 추가 power_off()
#include <string.h> // 추가
// #include "threads/mmu.h" // 추가
#include <console.h> // 추가
#include "filesys/file.h" // 추가

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void putbuf (const char *buffer, size_t n); // 추가
void hex_dump (uintptr_t ofs, const void *buf_, size_t size, bool ascii); // 추가
bool filesys_create (const char *name, off_t initial_size); // 추가
bool create(const char *file, unsigned initial_size);
/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */
/* 
 * 시스템 콜.
 *
 * 이전에는 시스템 콜 서비스가 인터럽트 핸들러에 의해 처리되었습니다
 * (예: 리눅스에서의 int 0x80). 그러나 x86-64에서는 제조사가
 * 시스템 콜을 요청하기 위한 효율적인 경로를 제공합니다, `syscall` 명령어입니다.
 *
 * syscall 명령어는 모델 특정 레지스터(MSR)에서 값을 읽어서 작동합니다.
 * 자세한 내용은 매뉴얼을 참조하세요.
 */

#define MSR_STAR 0xc0000081         /* Segment selector msr: 시스템 콜을 실행할 때 코드 세그먼트의 기준을 설정합니다. 여기서 사용자 모드와 커널 모드 간의 세그먼트 전환이 정의됩니다.*/
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target: 시스템 콜이 발생했을 때 실행될 함수(시스템 콜 핸들러)의 주소를 설정 */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags: 시스템 콜이 진행될 때 마스킹될 플래그를 정의합니다. 이 마스킹은 시스템 콜 처리 중에 발생할 수 있는 인터럽트를 방지하기 위해 사용됩니다. */

/*
 * syscall_init 함수는 운영체제의 시스템 콜 인터페이스를 초기화합니다.
 * 이 함수는 시스템 콜 관련 MSR(Model Specific Registers)을 설정하여, 
 * 운영체제가 시스템 콜을 처리할 준비를 합니다. 
 * MSR은 CPU의 특정 동작을 제어하기 위해 사용되는 레지스터입니다. 
 * write_msr 함수는 특정 MSR에 값을 쓰는 데 사용되며, 
 * 이를 통해 해당 MSR이 가리키는 레지스터에 직접 값을 설정할 수 있습니다. 
 * 이 초기화 과정은 시스템이 안전하게 시스템 콜을 수행할 수 있도록 구성하는 중요한 단계입니다.
 */
void
syscall_init (void) 
{
	/* 시스템 콜에 대한 MSR 레지스터를 설정합니다.
	 * 이 때 SEL_UCSEG(사용자 코드 세그먼트 셀렉터)에서 0x10을 빼고 48비트를 왼쪽으로 시프트하고,
   * SEL_KCSEG(커널 코드 세그먼트 셀렉터)를 32비트 왼쪽으로 시프트합니다. */
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);

	/* syscall_entry 함수의 주소를 MSR_LSTAR 레지스터에 씁니다. */
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	/* 인터럽트 서비스 루틴은 syscall_entry가 사용자 모드 스택을 커널 모드 스택으로 교환할 때까지
   * 어떠한 인터럽트도 서비스해서는 안 됩니다. 그래서 FLAG_FL을 마스킹합니다. 
	 */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) 
{
	// 락 초기화
	// 글로벌 락 사용해서 file간 경쟁 조건 피하기->filesystem과 연관된 코드에서 글로벌 락 사용하기 
	lock_init(&filesys_lock); 

	// printf ("\nsystem call!\n");
	// printf("\nf->R.rax: %d\n", f->R.rax);

	// 매핑 중
	/* 인자가 전달되는 순서
	 * -> rdi, rsi, rdx, rcx, r8, r9
	 * 전체적인 흐름 - 뇌피셜 가득
	 * 1. /user/syscall.c에서 레지스터에 정보를 넣고 커널(/userprog/syscall.c)에 전달->문맥전환????
	 * 2. 커널은 syscall_handler를 호출해서 이게 어떤 시스템 콜인지 파악하고 처리
	 * 3. 
	 */
	switch (f->R.rax)
	{
	case SYS_HALT:  
		halt();  
		break; 
	case SYS_EXIT:  
		exit(f->R.rdi);  
		break; 
	case SYS_FORK:  /*fork_(f->R.rdi);*/  break; 
	case SYS_EXEC:  /*exec_(f->R.rdi);*/  break; 
	case SYS_WAIT:
		// wait_(f->R.rdi);
		break;
	case SYS_CREATE:  
		f->R.rax = create(f->R.rdi, f->R.rsi);  
		break; 
	case SYS_REMOVE:
		// remove_(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		// filesize_(f->R.rdi);
		break;
	case SYS_READ:
		// read_(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:  f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);  break; // 10번
	case SYS_SEEK:
		// seek_(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		// tell_(f->R.rdi);
		break;
	case SYS_CLOSE:
		// close_(f->R.rdi);
		break;
	default:
    printf("존재하지 않는 case\n");
	}
	
	// thread_exit ();
}

void 
check_page_fault(void* uaddr){
	struct thread *curr = thread_current();
	if(uaddr == NULL || is_kernel_vaddr(uaddr) || pml4_get_page(curr->pml4, uaddr) == NULL){
		exit(-1);
	}
}
static struct file *get_file_fd(int fd){
	struct thread *curr = thread_current();
	if(fd<0 || fd>=64){
		return NULL;
	}
	return curr->fdt[fd];
}
void 
halt(void)
{
	power_off();
	NOT_REACHED();
}

void
exit(int status)
{
	struct thread *curr = thread_current();
	curr->exit_status = status;
	printf("%s: exit(%d)\n", curr->name, curr->exit_status); // thread_exit 안에서 프린트하면 안됨!!!
	thread_exit();
}

bool 
create(const char *file, unsigned initial_size){
	check_page_fault(file);
	bool v = filesys_create(file, initial_size);
	return v;
}


int
write(int fd, const void *buffer, unsigned size)
{	
	int result;

	check_page_fault(buffer);
	if (fd == 1)
	{
		putbuf(buffer, size);
		return size;
	}
	else 
	{
		if(get_file_fd(fd) == NULL){
			result = -1;
		}else{
			result = file_write(get_file_fd(fd), buffer, size);
		}
		return result;
	}
	
}

int 
open(const char *file){

	check_page_fault(file);
	struct thread *curr = thread_current();
	struct file *get_file = filesys_open(file);

	if(get_file == NULL){
		return -1;
	}

	// for(int i=2; i<(sizeof(curr->fdt)/sizeof(curr->fdt[0]));i++){
	// 	if(curr->fdt[i] == get_file){
	// 		curr->fdt[i] = file;
	// 		return i;
	// 	}
	// 	else
	// 		return -1;
	// }
}