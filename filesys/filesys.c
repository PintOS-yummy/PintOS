#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();

#ifdef EFILESYS
	fat_init ();

	if (format)
		do_format ();

	fat_open ();
#else
	/* Original FS */
	free_map_init ();

	if (format)
		do_format ();

	free_map_open ();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}

/* 주어진 이름(NAME)과 초기 크기(INITIAL_SIZE)로 파일을 생성합니다.
 * 성공하면 true를 반환하고, 그렇지 않으면 false를 반환합니다.
 * NAME으로 된 파일이 이미 존재하거나 내부 메모리 할당에 실패하면 실패합니다.
 */
/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. 
 */
bool
filesys_create (const char *name, off_t initial_size) {
	disk_sector_t inode_sector = 0; // inode를 위한 디스크 섹터 번호 초기화.
	struct dir *dir = dir_open_root (); // 루트 디렉토리를 연다.

	bool success = (dir != NULL
			&& free_map_allocate (1, &inode_sector) // 빈 디스크 섹터를 할당한다.
			&& inode_create (inode_sector, initial_size) // inode를 생성한다.
			&& dir_add (dir, name, inode_sector)); // 디렉토리에 파일을 추가한다.
			
	if (!success && inode_sector != 0) // 파일 생성이 실패하고, 할당된 섹터가 있다면
		free_map_release (inode_sector, 1); // 할당된 섹터를 해제한다.
	dir_close (dir); // 루트 디렉토리를 닫는다.

	return success; // 파일 생성 성공 여부를 반환한다.
}

/* 주어진 이름(NAME)의 파일을 엽니다.
 * 성공적으로 파일을 열면 새 파일 구조체를 반환하고,
 * 그렇지 않으면 NULL 포인터를 반환합니다.
 * NAME으로 된 파일이 없을 경우,
 * 또는 내부 메모리 할당에 실패한 경우에는 열기 작업이 실패합니다. */
/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name) {
    /* 루트   디렉토리를 연다. */
	struct dir *dir = dir_open_root ();
	struct inode *inode = NULL;

    /* 루트 디렉토리가 성공적으로 열렸다면 파일을 찾는다. */
	if (dir != NULL)
		dir_lookup (dir, name, &inode); // dir_lookup 함수를 이용하여 name에 해당하는 inode를 찾는다.
	dir_close (dir); // 디렉토리 검색이 끝났으므로 루트 디렉토리를 닫는다.

    /* inode를 이용하여 파일 구조체를 열고 그 포인터를 반환한다.
       파일이 없거나 다른 오류가 발생하면 inode는 NULL이 될 것이다. */
	return file_open (inode); // file_open 함수는 inode를 이용하여 파일 구조체를 열고, 그 포인터를 반환한다.
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
/* 주어진 이름(NAME)의 파일을 삭제합니다.
 * 성공적으로 파일을 삭제하면 true를 반환하고, 실패하면 false를 반환합니다.
 * NAME으로 된 파일이 없거나 내부 메모리 할당에 실패한 경우 삭제 작업이 실패합니다. */
bool
filesys_remove (const char *name) {
    /* 루트 디렉토리를 연다. */
	struct dir *dir = dir_open_root ();
    
    /* 루트 디렉토리가 성공적으로 열렸다면 파일을 찾아서 삭제한다. */
	bool success = dir != NULL && dir_remove (dir, name);
    
    /* 디렉토리 검색이 끝났으므로 루트 디렉토리를 닫는다. */
	dir_close (dir);

    /* 파일 삭제 작업의 성공 여부를 반환한다. */
	return success;
}


/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create ();
	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}
