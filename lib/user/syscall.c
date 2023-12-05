#include <syscall.h>
#include <stdint.h>
#include "../syscall-nr.h"

__attribute__((always_inline)) static __inline int64_t syscall(uint64_t num_, uint64_t a1_, uint64_t a2_,
																															 uint64_t a3_, uint64_t a4_, uint64_t a5_, uint64_t a6_)
{
	int64_t ret;
	register uint64_t *num asm("rax") = (uint64_t *)num_;
	register uint64_t *a1 asm("rdi") = (uint64_t *)a1_;
	register uint64_t *a2 asm("rsi") = (uint64_t *)a2_;
	register uint64_t *a3 asm("rdx") = (uint64_t *)a3_;
	register uint64_t *a4 asm("r10") = (uint64_t *)a4_;
	register uint64_t *a5 asm("r8") = (uint64_t *)a5_;
	register uint64_t *a6 asm("r9") = (uint64_t *)a6_;

	__asm __volatile(
			"mov %1, %%rax\n"
			"mov %2, %%rdi\n"
			"mov %3, %%rsi\n"
			"mov %4, %%rdx\n"
			"mov %5, %%r10\n"
			"mov %6, %%r8\n"
			"mov %7, %%r9\n"
			"syscall\n"
			: "=a"(ret)
			: "g"(num), "g"(a1), "g"(a2), "g"(a3), "g"(a4), "g"(a5), "g"(a6)
			: "cc", "memory");
	return ret;
}

/* Invokes syscall NUMBER, passing no arguments, and returns the
	 return value as an `int'. */
#define syscall0(NUMBER) ( \
		syscall(((uint64_t)NUMBER), 0, 0, 0, 0, 0, 0))

/* Invokes syscall NUMBER, passing argument ARG0, and returns the
	 return value as an `int'. */
#define syscall1(NUMBER, ARG0) ( \
		syscall(((uint64_t)NUMBER),  \
						((uint64_t)ARG0), 0, 0, 0, 0, 0))
/* Invokes syscall NUMBER, passing arguments ARG0 and ARG1, and
	 returns the return value as an `int'. */
#define syscall2(NUMBER, ARG0, ARG1) ( \
		syscall(((uint64_t)NUMBER),        \
						((uint64_t)ARG0),          \
						((uint64_t)ARG1),          \
						0, 0, 0, 0))

#define syscall3(NUMBER, ARG0, ARG1, ARG2) ( \
		syscall(((uint64_t)NUMBER),              \
						((uint64_t)ARG0),                \
						((uint64_t)ARG1),                \
						((uint64_t)ARG2), 0, 0, 0))

#define syscall4(NUMBER, ARG0, ARG1, ARG2, ARG3) ( \
		syscall(((uint64_t *)NUMBER),                  \
						((uint64_t)ARG0),                      \
						((uint64_t)ARG1),                      \
						((uint64_t)ARG2),                      \
						((uint64_t)ARG3), 0, 0))

#define syscall5(NUMBER, ARG0, ARG1, ARG2, ARG3, ARG4) ( \
		syscall(((uint64_t)NUMBER),                          \
						((uint64_t)ARG0),                            \
						((uint64_t)ARG1),                            \
						((uint64_t)ARG2),                            \
						((uint64_t)ARG3),                            \
						((uint64_t)ARG4),                            \
						0))
/*
	power_off()를 호출해서 Pintos를 종료합니다. 
	(power_off()는 src/include/threads/init.h에 선언되어 있음.)
	
	이 함수는 웬만하면 사용되지 않아야 합니다. deadlock 상황에 대한 정보 등등 뭔가 조금 잃어 버릴지도 모릅니다
*/
void halt(void)
{
	syscall0(SYS_HALT);
	NOT_REACHED();
}

/*
	현재 동작중인 유저 프로그램을 종료합니다. 커널에 상태를 리턴하면서 종료합니다.

	만약 부모 프로세스가 현재 유저 프로그램의 종료를 기다리던 중이라면,
	그 말은 종료되면서 리턴될 그 상태를 기다린다는 것 입니다.
	관례적으로, 상태 = 0 은 성공을 뜻하고 0 이 아닌 값들은 에러를 뜻 합니다.
*/
void exit(int status)
{
	syscall1(SYS_EXIT, status);
	NOT_REACHED();
}

/*
	THREAD_NAME이라는 이름을 가진 현재 프로세스의 복제본인 새 프로세스를 만듭니다.

	피호출자(callee) 저장 레지스터인 %RBX, %RSP, %RBP와 %R12 ~ %R15를 제외한 레지스터 값을 복제할 필요가 없습니다.
	자식 프로세스의 pid를 반환해야 합니다. 그렇지 않으면 유효한 pid가 아닐 수 있습니다.
	자식 프로세스에서 반환 값은 0이어야 합니다.
	자식 프로세스에는 파일 식별자 및 가상 메모리 공간을 포함한 복제된 리소스가 있어야 합니다.
	부모 프로세스는 자식 프로세스가 성공적으로 복제되었는지 여부를 알 때까지 fork에서 반환해서는 안 됩니다.
	즉, 자식 프로세스가 리소스를 복제하지 못하면 부모의 fork() 호출이 TID_ERROR를 반환할 것입니다.
	템플릿은 `threads/mmu.c`의 `pml4_for_each`를 사용하여 해당되는 페이지 테이블 구조를 포함한 전체 사용자 메모리 공간을 복사하지만,
	전달된 `pte_for_each_func`의 누락된 부분을 채워야 합니다.
([가상 주소](https://casys-kaist.github.io/pintos-kaist/appendix/virtual_address.html)) 참조).
*/
pid_t fork(const char *thread_name)
{
	return (pid_t)syscall1(SYS_FORK, thread_name);
}

/*
	현재의 프로세스가 cmd_line에서 이름이 주어지는 실행가능한 프로세스로 변경됩니다.
	이때 주어진 인자들을 전달합니다. 성공적으로 진행된다면 어떤 것도 반환하지 않습니다.
	만약 프로그램이 이 프로세스를 로드하지 못하거나 다른 이유로 돌리지 못하게 되면 exit state -1을 반환하며 프로세스가 종료됩니다.
	이 함수는 exec 함수를 호출한 쓰레드의 이름은 바꾸지 않습니다.
	file descriptor는 exec 함수 호출 시에 열린 상태로 있다는 것을 알아두세요.
*/
int exec(const char *file)
{
	return (pid_t)syscall1(SYS_EXEC, file);
}

/*
	자식 프로세스 (pid) 를 기다려서 자식의 종료 상태(exit status)를 가져옵니다.

	만약 pid (자식 프로세스)가 아직 살아있으면, 종료 될 때 까지 기다립니다.
	종료가 되면 그 프로세스가 exit 함수로 전달해준 상태(exit status)를 반환합니다.
	만약 pid (자식 프로세스)가 exit() 함수를 호출하지 않고 커널에 의해서 종료된다면,
	(e.g exception에 의해서 죽는 경우), wait(pid) 는  -1을 반환해야 합니다.
	부모 프로세스가 wait 함수를 호출한 시점에서 이미 종료되어버린 자식 프로세스를 기다리도록 하는 것은 완전히 합당합니다만,
	커널은 부모 프로세스에게 자식의 종료 상태를 알려주든지, 커널에 의해 종료되었다는 사실을 알려주든지 해야 합니다.
	다음의 조건들 중 하나라도 참이면 wait 은 즉시 fail 하고 -1 을 반환합니다 :

	1. pid 는 호출하는 프로세스의 직속 자식을 참조하지 않습니다.
		오직 호출하는 프로세스가 fork() 호출 후 성공적으로 pid를 반환받은 경우에만,
		pid 는 호출하는 프로세스의 직속 자식입니다.

	2. 자식들은 상속되지 않는다:
		만약 A 가 자식 B를 낳고 B가 자식 프로세스 C를  낳는다면, A는 C를 기다릴 수 없습니다.
		심지어 B가 죽은 경우에도요. 프로세스 A가  wait(C) 호출하는 것은 실패해야 합니다.

	3. 마찬가지로, 부모 프로세스가 먼저 종료되버리는 고아 프로세스들도 새로운 부모에게 할당되지 않습니다.

	4. wait을 호출한 프로세스가 이미 pid에 대해 기다리는 wait을 호출한 상태 일 때,
		즉, 한 프로세스는 어떤 주어진 자식에 대해서 최대 한번만 wait 할 수 있습니다.


	프로세스들은 자식을 얼마든지 낳을 수 있고 그 자식들을 어떤 순서로도 기다릴 (wait) 수 있습니다.
	자식 몇 개로부터의 신호는 기다리지 않고도 종료될 수 있습니다. (전부를 기다리지 않기도 합니다.)

	여러분의 설계는 발생할 수 있는 기다림의 모든 경우를 고려해야합니다.
	한 프로세스의 (그 프로세스의 struct thread 를 포함한) 자원들은 꼭 할당 해제되어야 합니다.
	부모가 그 프로세스를 기다리든 아니든, 자식이 부모보다 먼저 종료되든 나중에 종료되든 상관없이 이뤄져야 합니다.

	최초의 process가 종료되기 전에 Pintos가 종료되지 않도록 하십시오.

	제공된 Pintos 코드는 main() (in threads/init.c)에서 process_wait() (in userprog/process.c ) 를 호출하여 Pintos가 최초의 process 보다 먼저 종료되는 것을 막으려고 시도합니다.

	여러분은 함수 설명의 제일 위의 코멘트를 따라서 process_wait() 를 구현하고
	process_wait() 의 방식으로 wait system call을 구현해야 할 겁니다.

	이 시스템 콜을 구현하는 것이 다른 어떤 시스템콜을 구현하는 것보다 더 많은 작업을 요구합니다.
*/
int wait(pid_t pid)
{
	return syscall1(SYS_WAIT, pid);
}

/*
	file(첫 번째 인자)를 이름으로 하고 크기가 initial_size(두 번째 인자)인 새로운 파일을 생성합니다.

	성공적으로 파일이 생성되었다면 true를 반환하고, 실패했다면 false를 반환합니다.
	새로운 파일을 생성하는 것이 그 파일을 여는 것을 의미하지는 않습니다:
		파일을 여는 것은 open 시스템콜의 역할로, ‘생성’과 개별적인 연산입니다.
*/
bool create(const char *file, unsigned initial_size)
{
	return syscall2(SYS_CREATE, file, initial_size);
}

/*
	file(첫 번째)라는 이름을 가진 파일을 삭제합니다.(Deletes the file called file.)

	성공적으로 삭제했다면 true를 반환하고, 그렇지 않으면 false를 반환합니다.
	파일은 열려있는지 닫혀있는지 여부와 관계없이 삭제될 수 있고, 파일을 삭제하는 것이 그 파일을 닫았다는 것을 의미하지는 않습니다.
	자세한 내용을 알고 싶다면 FAQ에 있는 Removing an Open File를 참고하세요.
*/
bool remove(const char *file)
{
	return syscall1(SYS_REMOVE, file);
}

/*
	file(첫 번째 인자)이라는 이름을 가진 파일을 엽니다.
	해당 파일이 성공적으로 열렸다면, 파일 식별자로 불리는 비음수 정수(0또는 양수)를 반환하고, 
	실패했다면 -1를 반환합니다.

	0번 파일식별자와 1번 파일식별자는 이미 역할이 지정되어 있습니다.
	0번은 표준 입력(STDIN_FILENO)을 의미하고 1번은 표준 출력(STDOUT_FILENO)을 의미합니다.

	open 시스템 콜은 아래에서 명시적으로 설명하는 것처럼, 
	시스템 콜 인자로서만 유효한 파일 식별자들을 반환하지 않습니다.

	각각의 프로세스는 독립적인 파일 식별자들을 갖습니다.
	파일 식별자는 자식 프로세스들에게 상속(전달)됩니다.

	하나의 프로세스에 의해서든 다른 여러개의 프로세스에 의해서든,
	하나의 파일이 두 번 이상 열리면 그때마다 open 시스템콜은 새로운 식별자를 반환합니다.
	하나의 파일을 위한 서로 다른 파일 식별자들은 개별적인 close 호출에 의해서 독립적으로 닫히고 그 한 파일의 위치를 공유하지 않습니다.
	당신이 추가적인 작업을 하기 위해서는 open 시스템 콜이 반환하는 정수(fd)가 0보다 크거나 같아야 한다는 리눅스 체계를 따라야 합니다.
*/
int open(const char *file)
{
	return syscall1(SYS_OPEN, file);
}

/*
	fd(첫 번째 인자)로서 열려 있는 파일의 크기가 몇 바이트인지 반환합니다.
*/
int filesize(int fd)
{
	return syscall1(SYS_FILESIZE, fd);
}

/*
	buffer 안에 fd 로 열려있는 파일로부터 size 바이트를 읽습니다.
	실제로 읽어낸 바이트의 수 를 반환합니다 (파일 끝에서 시도하면 0).
	파일이 읽어질 수 없었다면 -1을 반환합니다.(파일 끝이라서가 아닌 다른 조건에 때문에 못 읽은 경우)
*/
int read(int fd, void *buffer, unsigned size)
{
	return syscall3(SYS_READ, fd, buffer, size);
}

/*
	buffer로부터 open file fd로 size 바이트를 적어줍니다.
	
	실제로 적힌 바이트의 수를 반환해주고, 일부 바이트가 적히지 못했다면, 
	size보다 더 작은 바이트 수가 반환될 수 있습니다.

	파일의 끝을 넘어서 작성하는 것은 보통 파일을 확장하는 것이지만,
	파일 확장은 basic file system에 의해서는 불가능합니다.

	이로 인해 파일의 끝까지 최대한 많은 바이트를 적어주고 실제 적힌 수를 반환하거나,
	더 이상 바이트를 적을 수 없다면 0을 반환합니다.

	fd 1은 콘솔에 적어줍니다. 콘솔에 작성한 코드가 적어도 몇 백 바이트를 넘지 않는 사이즈라면,
	한 번의 호출에 있는 모든 버퍼를  putbuf()에 적어주는 것입니다.(더 큰 버퍼는 분해하는 것이 합리적입니다!!)
	그렇지 않다면, 다른 프로세스에 의해 텍스트 출력 라인들이 콘솔에 끼게 (interleaved)되고,
	읽는 사람과 우리 채점 스크립트가 헷갈릴 것입니다.
*/
int write(int fd, const void *buffer, unsigned size)
{
	return syscall3(SYS_WRITE, fd, buffer, size);
}

/*
	open file fd에서 읽거나 쓸 다음 바이트를 position으로 변경합니다.

	position은 파일 시작부터 바이트 단위로 표시됩니다.
	(따라서 position 0은 파일의 시작을 의미합니다).

	이후에 read를 실행하면 파일의 끝을 가리키는 0바이트를 얻습니다.
	이후에 write를 실행하면 파일이 확장되어 기록되지 않은 공백이 0으로 채워집니다.
	(하지만 Pintos에서 파일은 프로젝트 4가 끝나기 전까지 길이가 고정되어 있기 때문에
	파일의 끝을 넘어서 작성하려고 하면 오류를 반환할 것입니다.)

	이러한 의미론은 filesystem 안에서 구현되며 system call을 구현할 때에는 특별히 노력할 필요는 없습니다.
*/
void seek(int fd, unsigned position)
{
	syscall2(SYS_SEEK, fd, position);
}

/*
	열린 파일 fd에서 읽히거나 써질 다음 바이트의 위치를 반환합니다.

	파일의 시작지점부터 몇바이트인지로 표현됩니다.
*/
unsigned tell(int fd)
{
	return syscall1(SYS_TELL, fd);
}

/*
	파일 식별자 fd를 닫습니다.

	프로세스를 나가거나 종료하는 것은 묵시적으로 그 프로세스의 열려있는 파일 식별자들을 닫습니다.
	마치 각 파일 식별자에 대해 이 함수가 호출된 것과 같습니다.
*/
void close(int fd)
{
	syscall1(SYS_CLOSE, fd);
}

int dup2(int oldfd, int newfd)
{
	return syscall2(SYS_DUP2, oldfd, newfd);
}

void *
mmap(void *addr, size_t length, int writable, int fd, off_t offset)
{
	return (void *)syscall5(SYS_MMAP, addr, length, writable, fd, offset);
}

void munmap(void *addr)
{
	syscall1(SYS_MUNMAP, addr);
}

bool chdir(const char *dir)
{
	return syscall1(SYS_CHDIR, dir);
}

bool mkdir(const char *dir)
{
	return syscall1(SYS_MKDIR, dir);
}

bool readdir(int fd, char name[READDIR_MAX_LEN + 1])
{
	return syscall2(SYS_READDIR, fd, name);
}

bool isdir(int fd)
{
	return syscall1(SYS_ISDIR, fd);
}

int inumber(int fd)
{
	return syscall1(SYS_INUMBER, fd);
}

int symlink(const char *target, const char *linkpath)
{
	return syscall2(SYS_SYMLINK, target, linkpath);
}

int mount(const char *path, int chan_no, int dev_no)
{
	return syscall3(SYS_MOUNT, path, chan_no, dev_no);
}

int umount(const char *path)
{
	return syscall1(SYS_UMOUNT, path);
}
