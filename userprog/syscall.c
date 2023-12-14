#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

// 추가
#include "threads/init.h"
#include <string.h>				
// #include "threads/mmu.h" 
#include <console.h>			
#include "filesys/file.h"
#include "threads/palloc.h"	
#include "userprog/process.h"
#include "user/syscall.h" 

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void putbuf(const char *buffer, size_t n);															 // 추가
void hex_dump(uintptr_t ofs, const void *buf_, size_t size, bool ascii); // 추가
bool filesys_create(const char *name, off_t initial_size);							 // 추가

// 추가
void sys_halt(void);
void sys_exit(int status);
int sys_write(int fd, const void *buffer, unsigned size);
bool sys_create(const char *file, unsigned initial_size);
struct file *filesys_open(const char *name);
int sys_open(const char *name);
void file_close(struct file *file);
uint8_t input_getc(void);
off_t file_read(struct file *file, void *buffer, off_t size);
static struct file *get_file_fd(int fd);
off_t file_length(struct file *file);
off_t file_tell (struct file *file);
void file_seek (struct file *file, off_t new_pos);
off_t file_tell (struct file *file);
unsigned sys_tell(int fd);
void sys_seek(int fd, unsigned position);
bool filesys_remove (const char *name);
bool sys_remove(const char *file);
int process_exec(void *f_name);
tid_t process_fork(const char *name, struct intr_frame *if_ UNUSED);
pid_t sys_fork(const char *thread_name, struct intr_frame *if_);
int sys_wait(pid_t pid);
int process_wait(tid_t child_tid UNUSED);
void remove_child_process(pid_t);
struct thread *get_child_process(int pid);


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

#define MSR_STAR 0xc0000081					/* Segment selector msr: 시스템 콜을 실행할 때 코드 세그먼트의 기준을 설정합니다. 여기서 사용자 모드와 커널 모드 간의 세그먼트 전환이 정의됩니다.*/
#define MSR_LSTAR 0xc0000082				/* Long mode SYSCALL target: 시스템 콜이 발생했을 때 실행될 함수(시스템 콜 핸들러)의 주소를 설정 */
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
void syscall_init(void)
{
	/* 시스템 콜에 대한 MSR 레지스터를 설정합니다.
	 * 이 때 SEL_UCSEG(사용자 코드 세그먼트 셀렉터)에서 0x10을 빼고 48비트를 왼쪽으로 시프트하고,
	 * SEL_KCSEG(커널 코드 세그먼트 셀렉터)를 32비트 왼쪽으로 시프트합니다. */
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
													((uint64_t)SEL_KCSEG) << 32);

	/* syscall_entry 함수의 주소를 MSR_LSTAR 레지스터에 씁니다. */
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	/* 인터럽트 서비스 루틴은 syscall_entry가 사용자 모드 스택을 커널 모드 스택으로 교환할 때까지
	 * 어떠한 인터럽트도 서비스해서는 안 됩니다. 그래서 FLAG_FL을 마스킹합니다.
	 */
	write_msr(MSR_SYSCALL_MASK,
						FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	// 락 초기화
	// 글로벌 락 사용해서 file간 경쟁 조건 피하기->filesystem과 연관된 코드에서 글로벌 락 사용하기
	lock_init(&filesys_lock);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f)
{
	
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
	case SYS_HALT:  							 sys_halt();  														 break; // 0번
	case SYS_EXIT:  							 sys_exit(f->R.rdi);  										 break;	// 1번
	case SYS_FORK:  		f->R.rax = sys_fork(f->R.rdi, f);  									 break;	// 2번
	case SYS_EXEC:  		f->R.rax = sys_exec(f->R.rdi);  										 break;	// 3번
	case SYS_WAIT:  		f->R.rax = sys_wait(f->R.rdi);  									   break;	// 4번
	case SYS_CREATE:  	f->R.rax = sys_create(f->R.rdi, f->R.rsi);  				 break; // 5번
	case SYS_REMOVE:  	f->R.rax = sys_remove(f->R.rdi);  									 break;
	case SYS_OPEN:  		f->R.rax = sys_open(f->R.rdi);  										 break;
	case SYS_FILESIZE:  f->R.rax = sys_filesize(f->R.rdi);  								 break;
	case SYS_READ:  		f->R.rax = sys_read(f->R.rdi, f->R.rsi, f->R.rdx);   break; // sys_filesize() 구현해야 테스트 통과함
	case SYS_WRITE:  		f->R.rax = sys_write(f->R.rdi, f->R.rsi, f->R.rdx);  break; // 10번
	case SYS_SEEK: 								 sys_seek(f->R.rdi, f->R.rsi);  					 break;
	case SYS_TELL:  		f->R.rax = sys_tell(f->R.rdi);  										 break;
	case SYS_CLOSE:  							 sys_close(f->R.rdi);  										 break;
	default:
		printf("존재하지 않는 case\n");
	}

	// thread_exit ();
}

void 
sys_halt(void)
{
	power_off();
	NOT_REACHED();
}

void 
sys_exit(int status)
{
	struct thread *curr = thread_current();
	curr->exit_status = status;
	printf("%s: exit(%d)\n", curr->name, curr->exit_status); // thread_exit 안에서 프린트하면 안됨!!!
	thread_exit();
}

pid_t
sys_fork(const char *thread_name, struct intr_frame *if_) // fork가 된 시점의 프로세스의 if_ 가져옴
{
	// printf("\n1\n");
	/* 현재 프로세스의 if_를 thread_name이라는 프로세스로 복제
	복제된 프로세스의 pid 반환 */
	int pid = process_fork(thread_name, if_);
	
	struct thread *child = get_child_process(pid);
	if (child == NULL) // child 프로세스를 찾지 못하면,
		return TID_ERROR;

	sema_down(&child->fork_sema); // child가 load 될 때까지 down하고, load되면 up해줌
	// printf("\n2\n");
	
	if (child->exit_status == -1)
		return -1;
	
	return pid; // return pid가 실행되도록 세마포어를 사용하는 것
}

// pid인 자식 프로세스의 thread 구조체 반환
struct thread *
get_child_process(int pid)
{
	// printf("\n3\n");
	struct thread *curr = thread_current();
	struct list_elem *e;
	struct thread *child;

	for (e = list_begin(&curr->child_list); e != list_end(&curr->child_list); e = list_next(e))
	{
		child = list_entry(e, struct thread, child_elem);
		// printf("\nchild->tid: %d\n", child->tid);
		if (child->tid == pid)
			// printf("\n4\n");		
			return child;
	}

	return NULL; // 자식 프로세스 존재하지 않음
}

void
remove_child_process(pid_t pid)
{
	struct thread *curr = thread_current();
	struct list_elem *e;
	struct thread *child;

	for (e = list_begin(&curr->child_list); e != list_end(&curr->child_list); e = list_next(e))
	{
		child = list_entry(e, struct thread, child_elem);
		if (child->tid == pid)
			list_remove(e);
			return;
	}

	return;
}

int 
sys_exec (const char *cmd_line)
{
	check_page_fault(cmd_line);
	int size = strlen(cmd_line) + 1;
	if (cmd_line[0] == '\n') // 추가
		sys_exit(-1);
	
	// page와 frame은 malloc, frame->kva에는 palloc_get_page
	// 메모리 풀에서 4kb만큼 물리 메모리 공간을 잡고 물리 메모리 시작주소를 return 해줌.
	char *fn_copy = palloc_get_page(PAL_ZERO); 
	
	if ((fn_copy) == NULL)
		sys_exit(-1);
		
	// strlcpy(fn_copy, cmd_line, size);
	memcpy(fn_copy, cmd_line, size);

	if (process_exec(fn_copy) == -1)
		sys_exit(-1);
}

int 
sys_wait(pid_t pid)
{
	struct thread *curr = thread_current();
	
	if (curr->waiting_child == pid) // 이미 기다리는 자식이 또 wait 호출하면 안됨
		return -1;

	curr->waiting_child = pid; // 부모 프로세스가 기다리는 자식 프로세스 pid 값 저장

	return process_wait(pid);
}

bool 
sys_create(const char *file, unsigned initial_size)
{
	// 포인터 검사하는 함수 추가하기
	check_page_fault(file);

	return filesys_create(file, initial_size);
}

int 
sys_open(const char *name)
{
	check_page_fault(name);

	struct thread *curr = thread_current();
	// struct file *get_file = filesys_open(name);

	// for (int i = 3; i < (sizeof(curr->fdt) / sizeof(curr->fdt[0])); i++)
	for (int i = 3; i < 64; i++)
	{
		if (curr->fdt[i] == NULL
		)
		{
			struct file *get_file = filesys_open(name); // 추가
			if (!get_file)
				return -1;
			else
			{
				curr->fdt[i] = get_file;
				return i;
			}
		}
	}

	return -1;
}

int 
sys_filesize(int fd)
{	
	return file_length(get_file_fd(fd));
}

int 
sys_read(int fd, void *buffer, unsigned size)
{
	check_page_fault(buffer);

	if (!(get_file_fd(fd) == NULL))
	{

		if (fd == 0) // 사용자가 입력
		{
			return strlen(input_getc());
		}
		else
		{
			return file_read(get_file_fd(fd), buffer, size);
		}
	}

	return -1;
}

static struct file 
*get_file_fd(int fd)
{
	struct thread *curr = thread_current();

	if (fd < 0 || fd >= 64)
		return NULL;

	return curr->fdt[fd];
}

int 
sys_write(int fd, const void *buffer, unsigned size)
{
	check_page_fault(buffer);

	// if (!((fd < 0) || (fd > 64))) // fd가 유효한 값이면,
	if (fd == 1)
	{
		/* 콘솔에 putbuf() 이용해서 write
		 * putbuf(): 정확히 size_t n 만큼만 출력함 */
		putbuf(buffer, size);
		return size;
	}
	// fd가 1이 아니고, 유효한 값이면,
	// fdt[fd]에 buffer 값 작성
	// "Writes size bytes from buffer to the open file fd." <- 구현하기
	if (get_file_fd(fd) == NULL) // 추가
		return 0;
	
	// file_allow_write(thread_current()->fdt[fd]); // 추가
	return file_write(get_file_fd(fd), buffer, size); // return 몇 바이트 write 했는지 반환

	// return -1; // fd가 유효하지 않은 값인 경우,
}

// page_fault 확인
void 
check_page_fault(void *uadder)
{
	struct thread *curr = thread_current(); // va가 pa에 매핑이 되어있는지 확인
	if (uadder == NULL || is_kernel_vaddr(uadder) || pml4_get_page(curr->pml4, uadder) == NULL)
	{
		sys_exit(-1);
	}
}

bool 
sys_remove(const char *file)
{
	check_page_fault(file);

	return filesys_remove (file);
}

void 
sys_seek(int fd, unsigned position)
{
	return file_seek(get_file_fd(fd), position);
}

unsigned
sys_tell(int fd)
{
	return file_tell(get_file_fd(fd));
}

void 
sys_close(int fd)
{
	// struct thread *curr = thread_current();
	// // fdt의 존재하는 모든 fd를 닫음
	// for (int i = 3; i < (sizeof(curr->fdt) / sizeof(curr->fdt[0])); i++)
	// 	if (!curr->fdt[i])
	// 	{
	// 		file_close(curr->fdt[i]);
	// 	}
	
	struct file *get_file = get_file_fd(fd);

	if (get_file == NULL)
		return;
	
	file_close(get_file);
	thread_current()->fdt[fd] = NULL;

	return;
}