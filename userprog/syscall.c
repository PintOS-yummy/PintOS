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
#include <string.h>				// 추가
#include <console.h>			// 추가
#include "filesys/file.h" // 추가
#include "threads/palloc.h"

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
// void open_(const char *name);
void file_close(struct file *file);
void sys_seek (int fd, unsigned position);
unsigned sys_tell (int fd);
bool sys_remove (const char *file);

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
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f)
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
		sys_halt();
		break; // 0번
	case SYS_EXIT:
		sys_exit(f->R.rdi);
		break;			 // 1번
	case SYS_FORK: /*fork_(f->R.rdi);*/
		break;			 // 2번
	case SYS_EXEC: 
		f->R.rax = sys_exec(f->R.rdi);
		break;			 
	case SYS_WAIT:
		// wait_(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = sys_create(f->R.rdi, f->R.rsi);
		break; // 5번
	case SYS_REMOVE:
		f->R.rax = sys_remove(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = sys_open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = sys_filesize(f->R.rdi);
		break;
	case SYS_READ:
		f->R.rax = sys_read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		f->R.rax = sys_write(f->R.rdi, f->R.rsi, f->R.rdx);
		break; // 10번
	case SYS_SEEK:
		sys_seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		sys_tell(f->R.rdi);
		break;
	case SYS_CLOSE:  sys_close(f->R.rdi);  
		break;
	default:
		printf("존재하지 않는 case\n");
	}

	// thread_exit ();
}

void sys_halt(void)
{
	power_off();
	NOT_REACHED();
}

void sys_exit(int status)
{
	struct thread *curr = thread_current();
	curr->exit_status = status;
	printf("%s: exit(%d)\n", curr->name, curr->exit_status); 
	thread_exit();
}

bool sys_create(const char *file, unsigned initial_size)
{

	check_page_fault(file);

	return filesys_create(file, initial_size);
}

int
sys_open(const char *name)
{
	check_page_fault(name);

	struct thread *curr = thread_current();
	struct file *get_file = filesys_open(name);

	if (get_file == NULL)
		return -1;


	for (int i = 2; i < (sizeof(curr->fdt) / sizeof(curr->fdt[0])); i++)
	{
		if (!curr->fdt[i])
		{
			curr->fdt[i] = get_file;

			return i;
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

	if (fd == 1)
	{
		putbuf(buffer, size);
		return size;
	}
	else 
	{
		if (get_file_fd(fd) == NULL) // 추가
			return -1;
		else
			return file_write(get_file_fd(fd), buffer, size);
		
	}

}

void 
check_page_fault(void *uadder)
{
	struct thread *curr = thread_current(); // va가 pa에 매핑이 되어있는지 확인
	if (uadder == NULL || is_kernel_vaddr(uadder) || pml4_get_page(curr->pml4, uadder) == NULL)
	{
		sys_exit(-1);
	}
}

void
sys_close(int fd){

	if (fd < 0 || fd > 63) return;
	if (thread_current()->fdt[fd] == NULL) return;
	
	file_close(thread_current()->fdt[fd]);
	thread_current()->fdt[fd] = NULL;
	return;
}

int
sys_read(int fd, void *buffer, unsigned size){
	check_page_fault(buffer);
	
	if(!pml4_get_page(thread_current()->pml4,buffer)) { 
		sys_exit(-1);
	}

	int file_size;	
	if( fd == 0){
		file_size = input_getc();
		return file_size;
	}
	
	if( fd < 0 || fd > 63) return -1;
	// lock 잡아주기
	if( thread_current()->fdt[fd] == NULL) return -1;
	// printf("\nread : fd = %d\n",fd);
	return file_read(thread_current()->fdt[fd],buffer,size);
}

int 
sys_filesize(int fd){
	return file_length(get_file_fd(fd));
}

bool sys_remove (const char *file){ //file 이라는 이름을 가진 파일을 삭제
	check_page_fault(file);
	return filesys_remove(file);
}

void sys_seek (int fd, unsigned position){ //open file fd에서 읽거나 쓸 다음 바이트를 position(파일 시작부터 파이트 단위로 표시)으로 변경. 
	
	if(get_file_fd(fd)<=2)
		return;
	
	if(position<0 || position>sys_filesize(get_file_fd(fd)))
		return;

	file_seek(get_file_fd(fd), position);
}

unsigned sys_tell (int fd){ //열려진 파일 fd에서 읽히거나 써질 다음 바이트의 위치를 반환. 파일의 시작지점부터 몇바이트인지로 표현됩니다.
	if(check_page_fault <=2)
		return;
	return file_tell(get_file_fd(fd));
}

int sys_exec (const char *cmd_line){
	check_page_fault(cmd_line);
	
	int size = strlen(cmd_line)+1;
	char *fn_copy = palloc_get_page(PAL_ZERO); 
	//page와 frame은 malloc, frame->kva에는 palloc_get_page
	//메모리 풀에서 4kb만큼 물리 메모리 공간을 잡고 물리 메모리 시작주소를 return 해줌.
	if((fn_copy) == NULL)
		sys_exit(-1);
	strlcpy(fn_copy, cmd_line, size);

	if(process_exec(fn_copy) == -1)
		return -1;
}

//pid_t fork (const char *thread_name); 
//THREAD_NAME이라는 이름을 가진 현재 프로세스의 복제본인 새 프로세스를 만듬. 자식 프로세스 pid 반환
//자식 프로세스에서 반환값은 0이어야 함.
//자식 프로세스를 성공적으로 복제되었는지 확인하고 fork를 반환해야함. 실패하면 TID_ERROR
//threads/mmu.c의 pml4_for_each를 사용하여 해당되는 페이지 테이블 구조를 포함한 전체 사용자 메모리 공간을 복사하지만,
//pte_for_each_func의 누락된 부분을 채워줘야 함.

//int wait (pid_t pid);
//자식 프로세스 (pid) 를 기다려서 자식의 종료 상태(exit status)를 가져옴.
//만약 pid(자식 프로세스)가 살아있으면, 종료될 때 까지 기다려 종료가 되면 exit함수로 전달해준 상태(exit status) 반환
//exit를 호출하지 않고 커널에 의해 종료된다면(exception), wait(pid)는 -1 반환
//커널은 부모 프로세스에게 자식의 종료 상태를 알려주던지, 커널에 의해 종료되었다는 사실을 알려주어야 함.

//수정 필요한거
//자료구조 struct thread 수정
//static void init_thread(struct thread *, const char *name, int priority) //프로세스 디스크립터 초기화
// tid_t thread_create(const char *name, int priority, thread_func *function, void *aux) //function을 수행하는 스레드 생성
// void thread_exit(void) //현재 실행중인 스레드 종료
//int process_wait(tid_t child_tid UNUSED) // 자식 프로세스가 종료될 때까지 부모 프로세스 대기
//void exit(it status) //프로그램을 종료하는 시스템 콜
//struct thread *get_child_process(int pid)//자식 리스트를 검색하여 프로세스 디스크립터의 주소 리턴
//void remove_child_process(struct thread *cp) //프로세스 디스크립터를 자식 리스트에서 제거 후 메모리 해제

//추가 필요한거
//void thread_schedule_tail(struct thread *prev) //프로세스 스케줄링 하는 함수
//pid_t exec(const *cmd_line) //자식프로세스 생성 및 프로그램 실행
//int wait(tid_t tid) //자식 프로세스 종료될때 까지 대기(sleep)