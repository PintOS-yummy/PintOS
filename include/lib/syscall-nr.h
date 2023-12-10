#ifndef __LIB_SYSCALL_NR_H
#define __LIB_SYSCALL_NR_H

/* System call numbers. */
enum {
	/* Projects 2 and later. */
	SYS_HALT,                   /* Halt the operating system, 0번 부터 시작*/
	SYS_EXIT,                   /* Terminate this process. 1번*/
	SYS_FORK,                   /* Clone current process. 2번*/
	SYS_EXEC,                   /* Switch current process. 3번*/
	SYS_WAIT,                   /* Wait for a child process to die. 4번*/
	SYS_CREATE,                 /* Create a file. 5번*/
	SYS_REMOVE,                 /* Delete a file. 6번*/
	SYS_OPEN,                   /* Open a file. 7번*/
	SYS_FILESIZE,               /* Obtain a file's size. 8번*/
	SYS_READ,                   /* Read from a file. 9번*/
	SYS_WRITE,                  /* Write to a file. 10번*/
	SYS_SEEK,                   /* Change position in a file. 11번*/
	SYS_TELL,                   /* Report current position in a file. 12번*/
	SYS_CLOSE,                  /* Close a file. 13번*/

	/* Project 3 and optionally project 4. */
	SYS_MMAP,                   /* Map a file into memory. */
	SYS_MUNMAP,                 /* Remove a memory mapping. */

	/* Project 4 only. */
	SYS_CHDIR,                  /* Change the current directory. */
	SYS_MKDIR,                  /* Create a directory. */
	SYS_READDIR,                /* Reads a directory entry. */
	SYS_ISDIR,                  /* Tests if a fd represents a directory. */
	SYS_INUMBER,                /* Returns the inode number for a fd. */
	SYS_SYMLINK,                /* Returns the inode number for a fd. */

	/* Extra for Project 2 */
	SYS_DUP2,                   /* Duplicate the file descriptor */

	SYS_MOUNT,
	SYS_UMOUNT,
};

#endif /* lib/syscall-nr.h */
