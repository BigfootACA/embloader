#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <assert.h>
#include "encode.h"

#define MAGIC 0xDEADBEEF
struct memblk {
	uint32_t magic;
	uint32_t size;
	char data[];
};

void _free_r(struct _reent *r, void *ptr){
	struct memblk *blk_hdr = BASE_CR(ptr, struct memblk, data);
	struct memblk *blk_end = ptr + blk_hdr->size;
	assert (blk_hdr->magic == MAGIC);
	assert (blk_end->magic == MAGIC);
	assert (blk_end->size == blk_hdr->size);
	FreePool(blk_hdr);
}

void* _malloc_r(struct _reent *r, size_t size) {
	assert(size < UINT32_MAX);
	struct memblk *blk_hdr = AllocatePool(sizeof(struct memblk) * 2 + size);
	struct memblk *blk_end = (void*)blk_hdr + sizeof(struct memblk) + size;
	if (!blk_hdr) return NULL;
	memset(blk_hdr, 0, sizeof(struct memblk) * 2 + size);
	blk_hdr->magic = MAGIC;
	blk_hdr->size = size;
	blk_end->magic = MAGIC;
	blk_end->size = size;
	return blk_hdr->data;
}

void* _calloc_r(struct _reent *r, size_t nmemb, size_t size) {
	ASSERT(size < UINT32_MAX);
	void *b = _malloc_r(r, nmemb * size);
	if (b) memset(b, 0, nmemb * size);
	return b;
}

void* _realloc_r(struct _reent *r, void *ptr, size_t size) {
	ASSERT(size < UINT32_MAX);
	if (!ptr) return _malloc_r(r, size);
	struct memblk *blk = BASE_CR(ptr, struct memblk, data);
	void *newptr = _malloc_r(r, size);
	if (!newptr) return NULL;
	memcpy(newptr, ptr, MIN(blk->size, size));
	_free_r(r, ptr);
	if (size > blk->size)
		memset((char*)newptr + blk->size, 0, size - blk->size);
	return newptr;
}

int _close_r(struct _reent *r, int fd) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _execve_r(struct _reent *r, const char *path, char *const *argv, char *const *envp) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _fcntl_r(struct _reent *r, int fd, int cmd, int arg) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _fork_r(struct _reent *r) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _fstat_r(struct _reent *r, int fd, struct stat *buf) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _getpid_r(struct _reent *r) {
	return 1;
}

__attribute__((weak)) int getpid() {
	return _getpid_r(_REENT);
}

int _isatty_r(struct _reent *r, int fd) {
	if (r) r->_errno = ENOSYS;
	return 0;
}

int _kill_r(struct _reent *r, int pid, int sig) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _link_r(struct _reent *r, const char *old, const char *new) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

_off_t _lseek_r(struct _reent *r, int fd, _off_t offset, int whence) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _mkdir_r(struct _reent *r, const char *path, int mode) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _open_r(struct _reent *r, const char *path, int flags, int mode) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

_ssize_t _read_r(struct _reent *r, int fd, void *buf, size_t count) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _rename_r(struct _reent *r, const char *old, const char *new) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

void *_sbrk_r(struct _reent *r, ptrdiff_t nbytes) {
	if (r) r->_errno = ENOSYS;
	return (void*)-1;
}

int _stat_r(struct _reent *r, const char *path, struct stat *buf) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _unlink_r(struct _reent *r, const char *path) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _wait_r(struct _reent *r, int *status) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

_ssize_t _write_r(struct _reent *r, int fd, const void *buf, size_t count) {
	if (fd == 1 || fd == 2) {
		EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out;
		switch (fd) {
			case 1: out = gST->ConOut; break;
			case 2: out = gST->StdErr; break;
		}
		if (!buf || count == 0) return 0;
		EFI_STATUS st = EFI_SUCCESS;
		CHAR16 buffer[128] = {};
		size_t len = count;
		const char *str = buf;
		convert_transfer lf_transfer = {
			.match = "\n",
			.match_size = 1,
			.replace = L"\r\n",
			.replace_size = 4
		};
		convert_transfer *transfers[] = { &lf_transfer, NULL };
		while (len > 0) {
			encode_convert_ctx ctx = {};
			ctx.in.src = ENC_UTF8;
			ctx.in.dst = ENC_UTF16;
			ctx.in.src_ptr = (void*) str;
			ctx.in.src_size = len;
			ctx.in.dst_ptr = buffer;
			ctx.in.dst_size = sizeof(buffer) - 2;
			ctx.in.allow_invalid = true;
			ctx.in.transfers = transfers;
			st = encode_convert(&ctx);
			if (EFI_ERROR(st)) break;
			if (ctx.out.dst_wrote == 0) break;
			buffer[sizeof(buffer) / sizeof(CHAR16) - 1]=0;
			st = out->OutputString(out, buffer);
			if (EFI_ERROR(st)) break;
			str = (const char*) ctx.out.src_end;
			len -= ctx.out.src_used;
		}
		return count - len;
	}
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _getentropy_r(struct _reent *r, void *buf, size_t len) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _gettimeofday_r(struct _reent *r, struct timeval *__tp, void *__tzp) {
	EFI_TIME time;
	EFI_STATUS status = gRT->GetTime(&time, NULL);
	if (EFI_ERROR(status)) {
		if (r) r->_errno = EIO;
		return -1;
	}
	if (__tp) {
		__tp->tv_sec = 
			time.Second +
			time.Minute * 60 +
			time.Hour * 3600 +
			(time.Day - 1) * 86400 +
			(time.Month - 1) * 2592000 +
			(time.Year - 1970) * 31536000;
		__tp->tv_usec = time.Nanosecond / 1000;
	}
	return 0;
}

__attribute__((weak)) int gettimeofday(struct timeval *__tp, void *__tzp) {
	return _gettimeofday_r(_REENT, __tp, __tzp);
}

_off64_t _lseek64_r(struct _reent *r, int fd, _off64_t offset, int whence) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

int _open64_r(struct _reent *r, const char *path, int flags, int mode) {
	if (r) r->_errno = ENOSYS;
	return -1;
}

void _exit(int __status) {
	EFI_STATUS st = __status == 0 ? EFI_SUCCESS : EFI_ABORTED;
	gBS->Exit(gImageHandle, st, 0, NULL);
}

int getentropy(void *ptr, size_t len) {
	return _getentropy_r(_REENT, ptr, len);
}

#if defined(__i386__)
struct _reent* __getreent(void) {
	return _GLOBAL_REENT;
}
#endif
