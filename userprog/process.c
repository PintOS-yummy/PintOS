#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup(void);
static bool load(const char *file_name, struct intr_frame *if_);
static void initd(void *f_name);
static void __do_fork(void *);
char *strtok_r(char *s, const char *delimiters, char **save_ptr);
// struct thread *get_child_process(int pid);

void hex_dump (uintptr_t ofs, const void *buf_, size_t size, bool ascii); // 추가

/* General process initializer for initd and other process. */
static void
process_init(void)
{
	struct thread *current = thread_current();
}

struct thread *
get_child_process2(int pid)
{
	// printf("\n2\n");
	struct thread *curr = thread_current();
	struct list_elem *e;
	struct thread *child;

	for (e = list_begin(&curr->child_list); e != list_end(&curr->child_list); e = list_next(e))
	{
		child = list_entry(e, struct thread, child_elem);
		// printf("\n3\n");	
		if (child->tid == pid)
			return child;
	}
	return NULL;
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE.
 * */
/*
 * 첫 번째 사용자 레벨 프로그램인 "initd"를 시작합니다. 이 프로그램은 FILE_NAME에서 로드됩니다.
 * 새로운 쓰레드는 process_create_initd()가 반환되기 전에
 * 스케줄 될 수 있으며 (심지어 종료될 수도 있습니다).
 * initd의 쓰레드 ID를 반환하거나,
 * 쓰레드를 생성할 수 없는 경우 TID_ERROR를 반환합니다.
 * 이 함수는 한 번만 호출되어야 합니다.
 */
tid_t 
process_create_initd(const char *file_name)
{
	char *fn_copy;
	tid_t tid;

	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	fn_copy = palloc_get_page(0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy(fn_copy, file_name, PGSIZE);

	char **save_ptr;
	strtok_r(file_name, " ", save_ptr); // 변경

	/* Create a new thread to execute FILE_NAME. */
	tid = thread_create(file_name, PRI_DEFAULT, initd, fn_copy);
	if (tid == TID_ERROR)
		palloc_free_page(fn_copy);
	return tid;
}

/* A thread function that launches first user process. */
static void
initd(void *f_name)
{
#ifdef VM
	supplemental_page_table_init(&thread_current()->spt);
#endif

	process_init();

	if (process_exec(f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
/* 현재 프로세스를 `name`이라는 이름으로 복제합니다. 새로운 프로세스의 스레드 ID를 반환하거나,
 * 스레드를 생성할 수 없는 경우 TID_ERROR를 반환합니다. */
tid_t 
process_fork(const char *name, struct intr_frame *if_)
{
	// thread_create가 호출되면 부모의 if_가 바뀜! 
	// 따라서 바뀌기전에 memcpy해서 현재 쓰레드(생성될 쓰레드의 부모)에 저장
	memcpy(&thread_current()->parent_if, if_, sizeof(struct intr_frame)); // 중요!!

  /* 자식 프로세스 생성 */
  return thread_create(name, PRI_DEFAULT, __do_fork, thread_current());
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
/* 부모 프로세스의 주소 공간을 복제하는 함수.
 * 이 함수는 프로젝트 2를 위해 pml4_for_each 함수에 전달되어 사용됩니다. */
static bool
duplicate_pte(uint64_t *pte, void *va, void *aux)
{
	struct thread *current = thread_current();
	struct thread *parent = (struct thread *)aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: If the parent_page is kernel page, then return immediately. */
	/* 1. TODO: 만약 부모 페이지가 커널 페이지라면, 즉시 반환합니다. */
	if (is_kern_pte(pte))
		return true;

	va = pg_round_down(va); // 페이지 단위로 복사 할 수 있게 페이지의 어디를 가리키든 페이지 시작주소로 지정
	/* 2. Resolve VA from the parent's page map level 4. */
	/* 2. 부모의 페이지 맵 레벨 4에서 가상 주소(VA)를 해결합니다. */
	parent_page = pml4_get_page(parent->pml4, va);
	
	if (!parent_page)
		return false;
	
	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */
	/* 3. TODO: 자식을 위해 새 PAL_USER 페이지를 할당하고 결과를 NEWPAGE에 설정합니다. */
	newpage = palloc_get_page(PAL_USER);

	if (!newpage)
		return false;

	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */
	/* 4. TODO: 부모의 페이지를 새 페이지로 복제하고
	 *    TODO: 부모 페이지가 쓰기 가능한지 확인하고 (WRITABLE을 결과에 따라 설정합니다). */
	memcpy(newpage, parent_page, PGSIZE);
	writable = (*pte & PTE_W) != 0;	//?? // 비트 연산

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
	/* 5. 쓰기 가능한 권한으로 자식의 페이지 테이블에 VA 주소로 새 페이지를 추가합니다. */
	if (!pml4_set_page(current->pml4, va, newpage, writable))
	{
		/* 6. TODO: if fail to insert page, do error handling. */
		/* 6. TODO: 페이지 삽입에 실패하면, 에러 처리를 합니다. */
		palloc_free_page(newpage);
		return false;
	}
	return true;
}
#endif

/* 부모의 실행 컨텍스트를 복사하는 스레드 함수입니다.
 * 힌트) parent->tf는 프로세스의 사용자 영역 컨텍스트를 보유하고 있지 않습니다.
 *       즉, process_fork의 두 번째 인자를 이 함수로 전달해야 합니다. */
/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
static void
__do_fork(void *aux)
{
	struct intr_frame if_;
	struct thread *parent = (struct thread *)aux;
	struct thread *current = thread_current();
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_)
		(즉, process_fork()'s if_를 전달해야 합니다.) */
	struct intr_frame *parent_if;
	bool succ = true;

	/* 1. Read the cpu context to local stack. */
	/* 1. 로컬 스택으로 CPU 컨텍스트를 읽어옵니다. */
	parent_if = &parent->parent_if;
	memcpy(&if_, parent_if, sizeof(struct intr_frame));

	/* 2. Duplicate PT */
	/* 2. 페이지 테이블을 복제합니다. */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate(current);
#ifdef VM
	supplemental_page_table_init(&current->spt);
	if (!supplemental_page_table_copy(&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each(parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/
	/* TODO: 여기에 코드를 작성합니다.
	 * TODO: 힌트) 파일 객체를 복제하기 위해 `include/filesys/file.h`에 있는
	 * TODO:       `file_duplicate`를 사용하세요. 부모 프로세스는 이 함수가
	 * TODO:       성공적으로 자원을 복제할 때까지 fork()에서 반환되어서는 안 됩니다.*/
	// 부모 fd가 존재하면, 현재 fd에 복사
	for (int i = 3; i < 64; i++)
	{
		if (parent->fdt[i] != NULL)
		{
			struct file *dup_file = file_duplicate(parent->fdt[i]);
			current->fdt[i] = dup_file;
		}
	}

	process_init(); // ASSERT (is_thread (t));

	/* Finally, switch to the newly created process. */
	/* 마지막으로, 새로 생성된 프로세스로 전환합니다. */
	if (succ)
	{
		sema_up(&current->fork_sema);
		if_.R.rax = 0; // 자식 생성이 잘 되면 0 반환
		do_iret(&if_); // if_ == 유저 컨텍스트 // 유저 컨텍스트로 컨텍스트 스위치!
	}
error:
	current->exit_status = -1;
	sema_up(&current->fork_sema);
	// thread_exit();
	sys_exit(-1);
}

void
argument_stack(int argc, const char **argv, struct intr_frame *_if)
{
	// 인자가 push된 스택의 주소값을 저장하는 배열
	char *addr[65];
	int cnt = argc;

	// 스택에 push 시작
	for (int i = cnt - 1; i >= 0; i--) // 역순으로 argv[] 요소를 스택에 push
	{
		int arg_length = strlen(argv[i]) + 1; // null 종료자('\n') 포함한 인수 길이
		
		_if->rsp -= arg_length; // 포인터 이동

		// rsp 저장
		addr[i] = (char *)_if->rsp; // 타입캐스팅 이유?

		// 인수 스택에 복사
		memcpy(_if->rsp, argv[i], arg_length);
	}

	// 주소값 8byte의 배수로 padding
	if (_if->rsp % 8)
	{
		_if->rsp -= (_if->rsp % 8); // 8의 배수로 만들고,
		// _if->rsp -= (8 - (_if->rsp % 8));
		// memset(_if->rsp, 0, 8 - (_if->rsp % 8)); 웨지감자?
	}

	// sentinel: 8byte 0 추가하기
	_if->rsp -= 8; // 8바이트만큼 감소
	memset(_if->rsp, 0, 8);

	// addr[]에 있는 argv 인자의 주소를 저장
	for (int i = argc - 1; i >= 0; i--)
	{
		_if->rsp -= 8;

		memcpy(_if->rsp, &addr[i], 8); // argv는 스택의 주소가 아님!! // 왜 &? addr에는 주소값이 데이터로 들어가있는데?
	}

	// argv[] 주소 값 가리키는 포인터의 주소 
	_if->R.rsi = _if->rsp;
	// printf("\n_if->R.rsi: %d\n", _if->R.rsi);
	
	// 인자 개수(argc) 저장
	_if->R.rdi = argc;
	// printf("\n_if->R.rdi: %d\n", _if->R.rdi);

	// fake address
	_if->rsp -= 8;
	memset(_if->rsp, 0, 8); // memcpy->memset으로 변경 후 page fault 해결
	// hex_dump(_if->rsp, _if->rsp, USER_STACK - _if->rsp, true);
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail.
 * 유저 프로그램을 위한 인자를 세팅 */
int 
process_exec(void *f_name)
{
	char *file_name = f_name;
	bool success;

	char *save_ptr;
	char *token = strtok_r(f_name, " ", &save_ptr);
	char *argv[65];
	int argc = 0;

	// 파싱
	while (token != NULL) // f_name의 모든 토큰 파싱해서 argv에 push
	{											// argc는 인자 개수
		argv[argc] = token;
		token = strtok_r(NULL, " ", &save_ptr);
		argc++;
	}

	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. */
	/* 
	 * 쓰레드 구조체에서 intr_frame을 사용할 수 없습니다.
	 * 이는 현재 쓰레드가 재스케줄링될 때,
	 * 실행 정보를 해당 멤버에 저장하기 때문입니다.
	 */
	struct intr_frame _if;
	// printf("\nprocess_exec, rax: %d\n", _if.R.rax);

	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS; // 인터럽트 활성화 플래그와 모든 인터럽트를 무시하는 플래그를 설정

	/* We first kill the current context 
	* 현재 프로세스의 실행 환경을 정리하고, 자원을 해제합니다.
	*/
	process_cleanup();

	/* And then load the binary */
	success = load(file_name, &_if);

	argument_stack(argc, argv, &_if); // 인자 스택에 push

	/* If load failed, quit. */
	palloc_free_page(file_name);
	if (!success)
		return -1;

	/* Start switched process. */
	do_iret(&_if);
	NOT_REACHED();
}

/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
/*
 * 스레드 TID가 종료될 때까지 기다린 후 그 종료 상태를 반환합니다. 만약
 * 커널에 의해 종료되었다면 (예: 예외로 인해 종료됨), -1을 반환합니다.
 * 만약 TID가 유효하지 않거나, 호출한 프로세스의 자식이 아니거나,
 * process_wait()이 이미 해당 TID에 대해 성공적으로 호출되었다면,
 * 기다리지 않고 즉시 -1을 반환합니다.
 *
 * 이 함수는 문제 2-2에서 구현될 예정입니다. 현재는 아무것도 하지 않습니다.
 */
int process_wait(tid_t child_tid)
{
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */
	/*
	 * XXX: 힌트) Pintos가 process_wait(initd)에서 종료됩니다. 우리는
	 * XXX:       process_wait을 구현하기 전에 여기에 무한 루프를
	 * XXX:       추가할 것을 권장합니다.
	 */

	// int cnt = 9999999; // process_wait이 너무 일찍 끝나서 로깅이 안될수도 있음
	// while (cnt-- > 0)
	// {
	// }

	struct thread *curr = thread_current();
	curr->waiting_child = child_tid;

	struct thread *target = get_child_process2(child_tid);

	if (!target) // 추가
		return -1; 
	
	sema_down(&target->wait_sema);

	int exit_status = target->exit_status; // 추가

	list_remove(&target->child_elem); // 추가
	target->parent = NULL;
	
	return exit_status; // 변경
}

/* Exit the process. This function is called by thread_exit (). */
void 
process_exit(void)
{
	// printf("\n1\n");
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */
	struct thread *curr = thread_current();

	//curr->parent->child_exit_status = curr->exit_status;
	
	// printf("\n2\n");
	sema_up(&curr->wait_sema); // 문제발생
	// list_remove(&curr->child_elem);

	process_cleanup();
}

/* Free the current process's resources. */
static void
process_cleanup(void)
{
	struct thread *curr = thread_current();

#ifdef VM
	supplemental_page_table_kill(&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;
	if (pml4 != NULL)
	{
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
		curr->pml4 = NULL;
		pml4_activate(NULL);
		pml4_destroy(pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void process_activate(struct thread *next)
{
	/* Activate thread's page tables. */
	pml4_activate(next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update(next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL 0						/* Ignore. */
#define PT_LOAD 1						/* Loadable segment. */
#define PT_DYNAMIC 2				/* Dynamic linking info. */
#define PT_INTERP 3					/* Name of dynamic loader. */
#define PT_NOTE 4						/* Auxiliary info. */
#define PT_SHLIB 5					/* Reserved. */
#define PT_PHDR 6						/* Program header table. */
#define PT_STACK 0x6474e551 /* Stack segment. */

#define PF_X 1 /* Executable. */
#define PF_W 2 /* Writable. */
#define PF_R 4 /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr
{
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR
{
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack(struct intr_frame *if_);
static bool validate_segment(const struct Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
												 uint32_t read_bytes, uint32_t zero_bytes,
												 bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
static bool
load(const char *file_name, struct intr_frame *if_)
{
	struct thread *t = thread_current();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create();
	if (t->pml4 == NULL)
		goto done;
	process_activate(thread_current());

	/* Open executable file. */
	file = filesys_open(file_name);
	if (file == NULL)
	{
		printf("load: %s: open failed\n", file_name);
		goto done;
	}

	/* Read and verify executable header. */
	if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr || memcmp(ehdr.e_ident, "\177ELF\2\1\1", 7) || ehdr.e_type != 2 || ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1 || ehdr.e_phentsize != sizeof(struct Phdr) || ehdr.e_phnum > 1024)
	{
		printf("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++)
	{
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length(file))
			goto done;
		file_seek(file, file_ofs);

		if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type)
		{
		case PT_NULL:
		case PT_NOTE:
		case PT_PHDR:
		case PT_STACK:
		default:
			/* Ignore this segment. */
			break;
		case PT_DYNAMIC:
		case PT_INTERP:
		case PT_SHLIB:
			goto done;
		case PT_LOAD:
			if (validate_segment(&phdr, file))
			{
				bool writable = (phdr.p_flags & PF_W) != 0;
				uint64_t file_page = phdr.p_offset & ~PGMASK;
				uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
				uint64_t page_offset = phdr.p_vaddr & PGMASK;
				uint32_t read_bytes, zero_bytes;
				if (phdr.p_filesz > 0)
				{
					/* Normal segment.
					 * Read initial part from disk and zero the rest. */
					read_bytes = page_offset + phdr.p_filesz;
					zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes);
				}
				else
				{
					/* Entirely zero.
					 * Don't read anything from disk. */
					read_bytes = 0;
					zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
				}
				if (!load_segment(file, file_page, (void *)mem_page,
													read_bytes, zero_bytes, writable))
					goto done;
			}
			else
				goto done;
			break;
		}
	}

	/* Set up stack. */
	if (!setup_stack(if_))
		goto done;

	/* Start address. */
	if_->rip = ehdr.e_entry;

	/* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */

	success = true;

done:
	/* We arrive here whether the load is successful or not. */
	file_close(file);
	return success;
}

/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment(const struct Phdr *phdr, struct file *file)
{
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t)file_length(file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
		 user address space range. */
	if (!is_user_vaddr((void *)phdr->p_vaddr))
		return false;
	if (!is_user_vaddr((void *)(phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
		 address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
		 Not only is it a bad idea to map page 0, but if we allowed
		 it then user code that passed a null pointer to system calls
		 could quite likely panic the kernel by way of null pointer
		 assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page(void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment(struct file *file, off_t ofs, uint8_t *upage,
						 uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT(pg_ofs(upage) == 0);
	ASSERT(ofs % PGSIZE == 0);

	file_seek(file, ofs);
	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page(PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read(file, kpage, page_read_bytes) != (int)page_read_bytes)
		{
			palloc_free_page(kpage);
			return false;
		}
		memset(kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page(upage, kpage, writable))
		{
			printf("fail\n");
			palloc_free_page(kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack(struct intr_frame *if_)
{
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page(PAL_USER | PAL_ZERO);
	if (kpage != NULL)
	{
		success = install_page(((uint8_t *)USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page(kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page(void *upage, void *kpage, bool writable)
{
	struct thread *t = thread_current();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page(t->pml4, upage) == NULL && pml4_set_page(t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool
lazy_load_segment(struct page *page, void *aux)
{
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment(struct file *file, off_t ofs, uint8_t *upage,
						 uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT(pg_ofs(upage) == 0);
	ASSERT(ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer(VM_ANON, upage,
																				writable, lazy_load_segment, aux))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack(struct intr_frame *if_)
{
	bool success = false;
	void *stack_bottom = (void *)(((uint8_t *)USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */

	return success;
}
#endif /* VM */
