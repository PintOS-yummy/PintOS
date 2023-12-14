/* Forks and waits for a single child process. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void test_main(void)
{
  int pid;

  if ((pid = fork("child"))) // 자식은 0 반환 따라서 else로 간다
  {
    int status = wait(pid);
    msg("Parent: child exit status is %d", status);
  }
  else
  {
    msg("child run");
    exit(81); // 81이라는 종료 코드를 부모 프로세스에 전달
  }
}
