#include "filesys/file.h"
#include <debug.h>
#include "filesys/inode.h"
#include "threads/malloc.h"

/* An open file. */
struct file 
{
	struct inode *inode;        /* File's inode. */
	off_t pos;                  /* Current position. */
	bool deny_write;            /* Has file_deny_write() been called? */
};

/* 주어진 INODE에 대한 파일을 열고, 이 INODE의 소유권을 가집니다.
 * 새로운 파일을 반환합니다. 할당이 실패하거나 INODE가 NULL이면,
 * NULL 포인터를 반환합니다.
 */
/* Opens a file for the given INODE, of which it takes ownership,
 * and returns the new file.  Returns a null pointer if an
 * allocation fails or if INODE is null. */
struct file *
file_open (struct inode *inode) 
{
	struct file *file = calloc (1, sizeof *file); // 파일 구조체를 위한 메모리를 할당하고 0으로 초기화합니다.
	if (inode != NULL && file != NULL) { // inode와 파일 구조체 할당이 성공했는지 확인합니다.
		file->inode = inode; // 파일 구조체에 inode를 설정합니다.
		file->pos = 0; // 파일 내 위치를 0으로 설정합니다 (파일의 시작점).
		file->deny_write = false; // 파일에 대한 쓰기 거부를 비활성화합니다.
		return file; // 초기화된 파일 구조체를 반환합니다.
	} else {
		inode_close (inode); // inode가 NULL이 아니면 inode를 닫습니다.
		free (file); // 파일 구조체에 대한 메모리 할당을 해제합니다.
		return NULL; // 실패 시 NULL을 반환합니다.
	}
}


/* Opens and returns a new file for the same inode as FILE.
 * Returns a null pointer if unsuccessful. */
struct file *
file_reopen (struct file *file) 
{
	return file_open (inode_reopen (file->inode));
}

/* Duplicate the file object including attributes and returns a new file for the
 * same inode as FILE. Returns a null pointer if unsuccessful. */
struct file *
file_duplicate (struct file *file) 
{
	struct file *nfile = file_open (inode_reopen (file->inode));
	if (nfile) {
		nfile->pos = file->pos;
		if (file->deny_write)
			file_deny_write (nfile);
	}
	return nfile;
}

/* Closes FILE. */
void
file_close (struct file *file) 
{
	if (file != NULL) {
		file_allow_write (file);
		inode_close (file->inode);
		free (file);
	}
}

/* Returns the inode encapsulated by FILE. */
struct inode *
file_get_inode (struct file *file) {
	return file->inode;
}

/* Reads SIZE bytes from FILE into BUFFER,
 * starting at the file's current position.
 * Returns the number of bytes actually read,
 * which may be less than SIZE if end of file is reached.
 * Advances FILE's position by the number of bytes read. */
off_t
file_read (struct file *file, void *buffer, off_t size) {
	off_t bytes_read = inode_read_at (file->inode, buffer, size, file->pos);
	file->pos += bytes_read;
	return bytes_read;
}

/* Reads SIZE bytes from FILE into BUFFER,
 * starting at offset FILE_OFS in the file.
 * Returns the number of bytes actually read,
 * which may be less than SIZE if end of file is reached.
 * The file's current position is unaffected. */
off_t
file_read_at (struct file *file, void *buffer, off_t size, off_t file_ofs) {
	return inode_read_at (file->inode, buffer, size, file_ofs);
}

/* Writes SIZE bytes from BUFFER into FILE,
 * starting at the file's current position.
 * Returns the number of bytes actually written,
 * which may be less than SIZE if end of file is reached.
 * (Normally we'd grow the file in that case, but file growth is
 * not yet implemented.)
 * Advances FILE's position by the number of bytes read. */
/* BUFFER에서 FILE로 SIZE 바이트를 씁니다.
 * 파일의 현재 위치에서 시작합니다.
 * 실제로 쓴 바이트 수를 반환하며,
 * 파일의 끝에 도달할 경우 SIZE보다 적을 수 있습니다.
 * (보통은 이 경우 파일을 확장시키겠지만, 파일 확장 기능은
 * 아직 구현되지 않았습니다.)
 * 파일의 위치는 읽은 바이트 수만큼 전진합니다. */
off_t
file_write (struct file *file, const void *buffer, off_t size) {
	/* inode_write_at 함수를 호출하여 파일의 현재 위치(file->pos)에서
	   size 바이트 만큼 buffer의 내용을 씁니다. */
	off_t bytes_written = inode_write_at (file->inode, buffer, size, file->pos);
	/* 쓰기 작업에 성공한 바이트 수만큼 파일 위치를 전진시킵니다. */
	file->pos += bytes_written;
	/* 실제로 쓴 바이트 수를 반환합니다. */
	return bytes_written;
}

/* Writes SIZE bytes from BUFFER into FILE,
 * starting at offset FILE_OFS in the file.
 * Returns the number of bytes actually written,
 * which may be less than SIZE if end of file is reached.
 * (Normally we'd grow the file in that case, but file growth is
 * not yet implemented.)
 * The file's current position is unaffected. */
off_t
file_write_at (struct file *file, const void *buffer, off_t size,
		off_t file_ofs) {
	return inode_write_at (file->inode, buffer, size, file_ofs);
}

/* Prevents write operations on FILE's underlying inode
 * until file_allow_write() is called or FILE is closed. */
void
file_deny_write (struct file *file) {
	ASSERT (file != NULL);
	if (!file->deny_write) {
		file->deny_write = true;
		inode_deny_write (file->inode);
	}
}

/* Re-enables write operations on FILE's underlying inode.
 * (Writes might still be denied by some other file that has the
 * same inode open.) */
void
file_allow_write (struct file *file) {
	ASSERT (file != NULL);
	if (file->deny_write) {
		file->deny_write = false;
		inode_allow_write (file->inode);
	}
}

/* Returns the size of FILE in bytes. */
off_t
file_length (struct file *file) {
	ASSERT (file != NULL);
	return inode_length (file->inode);
}

/* Sets the current position in FILE to NEW_POS bytes from the
 * start of the file. */
void
file_seek (struct file *file, off_t new_pos) {
	ASSERT (file != NULL);
	ASSERT (new_pos >= 0);
	file->pos = new_pos;
}

/* Returns the current position in FILE as a byte offset from the
 * start of the file. */
off_t
file_tell (struct file *file) {
	ASSERT (file != NULL);
	return file->pos;
}
