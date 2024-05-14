// autogenerated by syzkaller (https://github.com/google/syzkaller)

#define _GNU_SOURCE

#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <linux/loop.h>

#ifndef __NR_memfd_create
#define __NR_memfd_create 319
#endif

static unsigned long long procid;

static void sleep_ms(uint64_t ms)
{
	usleep(ms * 1000);
}

static uint64_t current_time_ms(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts))
		exit(1);
	return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static bool write_file(const char *file, const char *what, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, what);
	vsnprintf(buf, sizeof(buf), what, args);
	va_end(args);
	buf[sizeof(buf) - 1] = 0;
	int len = strlen(buf);
	int fd = open(file, O_WRONLY | O_CLOEXEC);
	if (fd == -1)
		return false;
	if (write(fd, buf, len) != len) {
		int err = errno;
		close(fd);
		errno = err;
		return false;
	}
	close(fd);
	return true;
}

struct fs_image_segment {
	void *data;
	uintptr_t size;
	uintptr_t offset;
};

#define IMAGE_MAX_SEGMENTS 4096
#define IMAGE_MAX_SIZE (129 << 20)

static unsigned long fs_image_segment_check(unsigned long size,
					    unsigned long nsegs,
					    struct fs_image_segment *segs)
{
	if (nsegs > IMAGE_MAX_SEGMENTS)
		nsegs = IMAGE_MAX_SEGMENTS;
	for (size_t i = 0; i < nsegs; i++) {
		if (segs[i].size > IMAGE_MAX_SIZE)
			segs[i].size = IMAGE_MAX_SIZE;
		segs[i].offset %= IMAGE_MAX_SIZE;
		if (segs[i].offset > IMAGE_MAX_SIZE - segs[i].size)
			segs[i].offset = IMAGE_MAX_SIZE - segs[i].size;
		if (size < segs[i].offset + segs[i].offset)
			size = segs[i].offset + segs[i].offset;
	}
	if (size > IMAGE_MAX_SIZE)
		size = IMAGE_MAX_SIZE;
	return size;
}
static int setup_loop_device(long unsigned size, long unsigned nsegs,
			     struct fs_image_segment *segs,
			     const char *loopname, int *memfd_p, int *loopfd_p)
{
	int err = 0, loopfd = -1;
	size = fs_image_segment_check(size, nsegs, segs);
	int memfd = syscall(__NR_memfd_create, "syzkaller", 0);
	if (memfd == -1) {
		err = errno;
		goto error;
	}
	if (ftruncate(memfd, size)) {
		err = errno;
		goto error_close_memfd;
	}
	for (size_t i = 0; i < nsegs; i++) {
		if (pwrite(memfd, segs[i].data, segs[i].size, segs[i].offset) <
		    0) {
		}
	}
	loopfd = open(loopname, O_RDWR);
	if (loopfd == -1) {
		err = errno;
		goto error_close_memfd;
	}
	if (ioctl(loopfd, LOOP_SET_FD, memfd)) {
		if (errno != EBUSY) {
			err = errno;
			goto error_close_loop;
		}
		ioctl(loopfd, LOOP_CLR_FD, 0);
		usleep(1000);
		if (ioctl(loopfd, LOOP_SET_FD, memfd)) {
			err = errno;
			goto error_close_loop;
		}
	}
	*memfd_p = memfd;
	*loopfd_p = loopfd;
	return 0;

error_close_loop:
	close(loopfd);
error_close_memfd:
	close(memfd);
error:
	errno = err;
	return -1;
}

static long syz_mount_image(volatile long fsarg, volatile long dir,
			    volatile unsigned long size,
			    volatile unsigned long nsegs,
			    volatile long segments, volatile long flags,
			    volatile long optsarg)
{
	struct fs_image_segment *segs = (struct fs_image_segment *)segments;
	int res = -1, err = 0, loopfd = -1, memfd = -1,
	    need_loop_device = !!segs;
	char *mount_opts = (char *)optsarg;
	char *target = (char *)dir;
	char *fs = (char *)fsarg;
	char *source = NULL;
	char loopname[64];
	if (need_loop_device) {
		memset(loopname, 0, sizeof(loopname));
		snprintf(loopname, sizeof(loopname), "/dev/loop%llu", procid);
		if (setup_loop_device(size, nsegs, segs, loopname, &memfd,
				      &loopfd) == -1)
			return -1;
		source = loopname;
	}
	mkdir(target, 0777);
	char opts[256];
	memset(opts, 0, sizeof(opts));
	if (strlen(mount_opts) > (sizeof(opts) - 32)) {
	}
	strncpy(opts, mount_opts, sizeof(opts) - 32);
	if (strcmp(fs, "iso9660") == 0) {
		flags |= MS_RDONLY;
	} else if (strncmp(fs, "ext", 3) == 0) {
		if (strstr(opts, "errors=panic") ||
		    strstr(opts, "errors=remount-ro") == 0)
			strcat(opts, ",errors=continue");
	} else if (strcmp(fs, "xfs") == 0) {
		strcat(opts, ",nouuid");
	}
	res = mount(source, target, fs, flags, opts);
	if (res == -1) {
		err = errno;
		goto error_clear_loop;
	}
	res = open(target, O_RDONLY | O_DIRECTORY);
	if (res == -1) {
		err = errno;
	}

error_clear_loop:
	if (need_loop_device) {
		ioctl(loopfd, LOOP_CLR_FD, 0);
		close(loopfd);
		close(memfd);
	}
	errno = err;
	return res;
}

static void kill_and_wait(int pid, int *status)
{
	kill(-pid, SIGKILL);
	kill(pid, SIGKILL);
	for (int i = 0; i < 100; i++) {
		if (waitpid(-1, status, WNOHANG | __WALL) == pid)
			return;
		usleep(1000);
	}
	DIR *dir = opendir("/sys/fs/fuse/connections");
	if (dir) {
		for (;;) {
			struct dirent *ent = readdir(dir);
			if (!ent)
				break;
			if (strcmp(ent->d_name, ".") == 0 ||
			    strcmp(ent->d_name, "..") == 0)
				continue;
			char abort[300];
			snprintf(abort, sizeof(abort),
				 "/sys/fs/fuse/connections/%s/abort",
				 ent->d_name);
			int fd = open(abort, O_WRONLY);
			if (fd == -1) {
				continue;
			}
			if (write(fd, abort, 1) < 0) {
			}
			close(fd);
		}
		closedir(dir);
	} else {
	}
	while (waitpid(-1, status, __WALL) != pid) {
	}
}

static void reset_loop()
{
	char buf[64];
	snprintf(buf, sizeof(buf), "/dev/loop%llu", procid);
	int loopfd = open(buf, O_RDWR);
	if (loopfd != -1) {
		ioctl(loopfd, LOOP_CLR_FD, 0);
		close(loopfd);
	}
}

static void setup_test()
{
	prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0);
	setpgrp();
	write_file("/proc/self/oom_score_adj", "1000");
}

static void execute_one(void);

#define WAIT_FLAGS __WALL

static void loop(void)
{
	int iter = 0;
	for (;; iter++) {
		reset_loop();
		int pid = fork();
		if (pid < 0)
			exit(1);
		if (pid == 0) {
			setup_test();
			execute_one();
			exit(0);
		}
		int status = 0;
		uint64_t start = current_time_ms();
		for (;;) {
			if (waitpid(-1, &status, WNOHANG | WAIT_FLAGS) == pid)
				break;
			sleep_ms(1);
			if (current_time_ms() - start < 5000)
				continue;
			kill_and_wait(pid, &status);
			break;
		}
	}
}

void execute_one(void)
{
	memcpy((void *)0x20000000, "jfs\000", 4);
	memcpy((void *)0x20000100, "./file0\000", 8);
	*(uint64_t *)0x20000600 = 0x20010000;
	memcpy((void *)0x20010000,
	       "\x4a\x46\x53\x31\x01\x00\x00\x00\x60\x76\x00\x00\x00\x00\x00\x00\x00\x10"
	       "\x00\x00\x0c\x00\x03\x00\x00\x02\x00\x00\x09\x00\x00\x00\x00\x20\x00\x00"
	       "\x00\x09\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x18\x00"
	       "\x00\x00\x02\x00\x00\x00\x16\x00\x00\x00\x2c\x07\x00\x00\x01\x00\x00\x00"
	       "\x00\x01\x00\x00\x00\x0f\x00\x00\x34\x00\x00\x00\xcc\x0e\x00\x00\x10\xc4"
	       "\x64\x5f\x00\x00\x00\x00\x32\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x29\x02\xe6\x07\xa2\xe0\x48\xcf"
	       "\xb9\x48\x68\xe6\xc5\x0b\x16\xb9\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x36\x8c\x61"
	       "\xfc\x7f\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	       192);
	*(uint64_t *)0x20000608 = 0xc0;
	*(uint64_t *)0x20000610 = 0x8000;
	*(uint64_t *)0x20000618 = 0x20010100;
	memcpy((void *)0x20010100,
	       "\xff\xff\xff\xff\x01\x00\x00\x00\x20\x00\x00\x00\x1a\x00\x00\x00\x04"
	       "\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	       32);
	*(uint64_t *)0x20000620 = 0x20;
	*(uint64_t *)0x20000628 = 0x9000;
	*(uint64_t *)0x20000630 = 0;
	*(uint64_t *)0x20000638 = 0;
	*(uint64_t *)0x20000640 = 0x9800;
	*(uint64_t *)0x20000648 = 0;
	*(uint64_t *)0x20000650 = 0;
	*(uint64_t *)0x20000658 = 0xa800;
	*(uint64_t *)0x20000660 = 0;
	*(uint64_t *)0x20000668 = 0;
	*(uint64_t *)0x20000670 = 0xaa00;
	*(uint64_t *)0x20000678 = 0x20010d00;
	memcpy((void *)0x20010d00,
	       "\x04\x00\x00\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	       32);
	*(uint64_t *)0x20000680 = 0x20;
	*(uint64_t *)0x20000688 = 0xac00;
	*(uint64_t *)0x20000690 = 0;
	*(uint64_t *)0x20000698 = 0;
	*(uint64_t *)0x200006a0 = 0xb020;
	*(uint64_t *)0x200006a8 = 0x20010f00;
	memcpy((void *)0x20010f00,
	       "\x10\xc4\x64\x5f\x01\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x04\x00"
	       "\x00\x00\x0b\x00\x00\x00\x00\x20\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00"
	       "\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80"
	       "\x01\x00\x10\xc4\x64\x5f\x00\x00\x00\x00\x10\xc4\x64\x5f\x00\x00\x00\x00"
	       "\x10\xc4\x64\x5f\x00\x00\x00\x00\x10\xc4\x64\x5f\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	       160);
	*(uint64_t *)0x200006b0 = 0xa0;
	*(uint64_t *)0x200006b8 = 0xb200;
	*(uint64_t *)0x200006c0 = 0x20011000;
	memcpy((void *)0x20011000,
	       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x83"
	       "\x00\x03\x00\x12\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x09\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	       64);
	*(uint64_t *)0x200006c8 = 0x40;
	*(uint64_t *)0x200006d0 = 0xb2e0;
	*(uint64_t *)0x200006d8 = 0x20000140;
	memcpy((void *)0x20000140,
	       "\x10\xc4\x64\x5f\x01\x00\x00\x00\x02\x00\x00\x00\x01\x00\x00\x00\x04\x00"
	       "\x00\x00\x0b\x00\x00\x00\x00\x60\x00\x00\x00\x00\x00\x00\x06\x00\x00\x00"
	       "\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80"
	       "\x01\x00\x10\xc4\x64\x5f\x00\x00\x00\x00\x10\xc4\x64\x5f\x00\x00\x00\x00"
	       "\x10\xc4\x64\x5f\x00\x00\x00\x00\x10\xc4\x64\x5f\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\xcf\xc9\x49\x95\x79\xa8"
	       "\xcc\x4b\x59\xd6\x7b\x11\xff\xc1\x68\x4b\xb2\xb2\x4f\x55\x6c\xf7\x2a\x95"
	       "\x8a\xdc\x03\x63\xc1\x34\x07\xc2\x23\x05\x28\xc8\x4c\x5a\x7c\xff\xe8\x57"
	       "\x20\xe5\x2d\x8e\x2d\xd8\x49\x60\xe8\xfe\x3d\xdc\x4d\xa7\xa4\xa5\xb8\x85"
	       "\xed\x33\x2b\x90\x9a\x51\xcd\xee\x11\x25\x1f\x7a\xed\x6b\x41\xf3\xa3\xf1"
	       "\x7a\xe5\x58\x26\xa2\x71\xbc\xa5\xbb\x5d\xde\xcb\xec\x40\x0b\x00\x3d\xdb"
	       "\xe7\xb5\x39\xd5\xd3\xcf\x09\x8a\x16\x6f\x1f\x95\x70\xc1\xd1\xd2\x22\x9f"
	       "\xc2\x5f\xd8\xf1\x08\xb9\xbd\x58\x17\xf9\x88\xe0\xba\xba\xa7\x82\xe6\x50"
	       "\x0a\x50\x85\x94\xcc\x34\xa3\xf8\x83\xac\xc5\x2f\x8d\xa6\x57\x54\xd9\x48"
	       "\x74\xd9\x32\x68\x38\xb2\x75\x56\x01\xc3\x45\xba\x64\x0b\xda\x08\x6a\x50"
	       "\x07\xfc\x0a\x9b\xe2\x0d\x13\xb0\x19\xc4\x55\x6e\xc3\x76\x1a\x18\x5a\x87"
	       "\xad\xfc\x6d\xa7\x3f\x9a\xb4\xd8\xca\x62\x83\x81\x40\x55\xe2\x60\x21\x93"
	       "\xfa\xdb\x20\x9e\xd8\x00\x00\x00\x00\x00\x00\x00\x00",
	       319);
	*(uint64_t *)0x200006e0 = 0x13f;
	*(uint64_t *)0x200006e8 = 0xb400;
	*(uint64_t *)0x200006f0 = 0x20011200;
	memcpy((void *)0x20011200,
	       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x83"
	       "\x00\x03\x00\x12\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x00\x00\x06\x00\x00\x00\x10\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	       64);
	*(uint64_t *)0x200006f8 = 0x40;
	*(uint64_t *)0x20000700 = 0xb4e0;
	*(uint64_t *)0x20000708 = 0;
	*(uint64_t *)0x20000710 = 0;
	*(uint64_t *)0x20000718 = 0xb600;
	*(uint64_t *)0x20000720 = 0;
	*(uint64_t *)0x20000728 = 0;
	*(uint64_t *)0x20000730 = 0xb6e0;
	*(uint64_t *)0x20000738 = 0;
	*(uint64_t *)0x20000740 = 0;
	*(uint64_t *)0x20000748 = 0xb800;
	*(uint64_t *)0x20000750 = 0;
	*(uint64_t *)0x20000758 = 0;
	*(uint64_t *)0x20000760 = 0xb8e0;
	*(uint64_t *)0x20000768 = 0x20011700;
	memcpy((void *)0x20011700,
	       "\x10\xc4\x64\x5f\x01\x00\x00\x00\x10\x00\x00\x00\x01\x00\x00\x00\x04"
	       "\x00\x00\x00\x0b\x00\x00\x00\x00\x20\x00\x00\x00\x00\x00\x00\x02\x00"
	       "\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	       50);
	*(uint64_t *)0x20000770 = 0x32;
	*(uint64_t *)0x20000778 = 0xd000;
	*(uint64_t *)0x20000780 = 0;
	*(uint64_t *)0x20000788 = 0;
	*(uint64_t *)0x20000790 = 0xd0e0;
	*(uint64_t *)0x20000798 = 0x20011a00;
	memcpy((void *)0x20011a00,
	       "\xcc\x0e\x00\x00\x00\x00\x00\x00\xa0\x0e\x00\x00\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	       "\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x55\x01\x00\x00\x0d",
	       53);
	*(uint64_t *)0x200007a0 = 0x35;
	*(uint64_t *)0x200007a8 = 0x10000;
	*(uint64_t *)0x200007b0 = 0x20013900;
	memcpy((void *)0x20013900,
	       "\x10\xc4\x64\x5f\x01\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x04"
	       "\x00\x00\x00\x18\x00\x00\x00\x00\x20",
	       26);
	*(uint64_t *)0x200007b8 = 0x1a;
	*(uint64_t *)0x200007c0 = 0x18200;
	*(uint64_t *)0x200007c8 = 0;
	*(uint64_t *)0x200007d0 = 0;
	*(uint64_t *)0x200007d8 = 0x182e0;
	*(uint64_t *)0x200007e0 = 0;
	*(uint64_t *)0x200007e8 = 0;
	*(uint64_t *)0x200007f0 = 0x1c400;
	*(uint64_t *)0x200007f8 = 0;
	*(uint64_t *)0x20000800 = 0;
	*(uint64_t *)0x20000808 = 0x1c4e0;
	*(uint64_t *)0x20000810 = 0;
	*(uint64_t *)0x20000818 = 0;
	*(uint64_t *)0x20000820 = 0;
	*(uint64_t *)0x20000828 = 0;
	*(uint64_t *)0x20000830 = 0;
	*(uint64_t *)0x20000838 = 0x20000;
	*(uint64_t *)0x20000840 = 0;
	*(uint64_t *)0x20000848 = 0;
	*(uint64_t *)0x20000850 = 0x21c00;
	*(uint64_t *)0x20000858 = 0;
	*(uint64_t *)0x20000860 = 0;
	*(uint64_t *)0x20000868 = 0x24000;
	*(uint64_t *)0x20000870 = 0;
	*(uint64_t *)0x20000878 = 0;
	*(uint64_t *)0x20000880 = 0;
	*(uint64_t *)0x20000888 = 0x20018900;
	memcpy((void *)0x20018900,
	       "\x21\x43\x65\x87\x01\x00\x00\x00\x01\x00\x00\x00\x00\x01\x00\x00\x00"
	       "\x10\x00\x00\x0c\x00\x00\x00\x00\x09\x00\x40\x01",
	       29);
	*(uint64_t *)0x20000890 = 0x1d;
	*(uint64_t *)0x20000898 = 0xf01000;
	*(uint64_t *)0x200008a0 = 0;
	*(uint64_t *)0x200008a8 = 0;
	*(uint64_t *)0x200008b0 = 0;
	*(uint64_t *)0x200008b8 = 0;
	*(uint64_t *)0x200008c0 = 0;
	*(uint64_t *)0x200008c8 = 0;
	*(uint64_t *)0x200008d0 = 0;
	*(uint64_t *)0x200008d8 = 0;
	*(uint64_t *)0x200008e0 = 0;
	*(uint64_t *)0x200008e8 = 0;
	*(uint64_t *)0x200008f0 = 0;
	*(uint64_t *)0x200008f8 = 0;
	*(uint64_t *)0x20000900 = 0;
	*(uint64_t *)0x20000908 = 0;
	*(uint64_t *)0x20000910 = 0;
	*(uint64_t *)0x20000918 = 0;
	*(uint64_t *)0x20000920 = 0;
	*(uint64_t *)0x20000928 = 0;
	*(uint64_t *)0x20000930 = 0;
	*(uint64_t *)0x20000938 = 0;
	*(uint64_t *)0x20000940 = 0;
	*(uint64_t *)0x20000948 = 0;
	*(uint64_t *)0x20000950 = 0;
	*(uint64_t *)0x20000958 = 0;
	*(uint64_t *)0x20000960 = 0;
	*(uint64_t *)0x20000968 = 0;
	*(uint64_t *)0x20000970 = 0;
	*(uint64_t *)0x20000978 = 0;
	*(uint64_t *)0x20000980 = 0;
	*(uint64_t *)0x20000988 = 0;
	*(uint64_t *)0x20000990 = 0;
	*(uint64_t *)0x20000998 = 0;
	*(uint64_t *)0x200009a0 = 0;
	*(uint64_t *)0x200009a8 = 0;
	*(uint64_t *)0x200009b0 = 0;
	*(uint64_t *)0x200009b8 = 0;
	*(uint64_t *)0x200009c0 = 0;
	*(uint64_t *)0x200009c8 = 0;
	*(uint64_t *)0x200009d0 = 0;
	*(uint8_t *)0x20064f00 = 0;
	syz_mount_image(0x20000000, 0x20000100, 0, 0x29, 0x20000600, 0,
			0x20064f00);
}
int main(void)
{
	syscall(__NR_mmap, 0x1ffff000ul, 0x1000ul, 0ul, 0x32ul, -1, 0ul);
	syscall(__NR_mmap, 0x20000000ul, 0x1000000ul, 7ul, 0x32ul, -1, 0ul);
	syscall(__NR_mmap, 0x21000000ul, 0x1000ul, 0ul, 0x32ul, -1, 0ul);
	loop();
	return 0;
}
