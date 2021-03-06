/*
 * process_helper.c
 * Implementation of process_helper.h
 */
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "sandbox_helper.h"
#include "file_toolkit.h"
#include "ptraceaux.h"
#include "binder_helper.h"

#define IS_WRITE(oflag) ((oflag & O_WRONLY) != 0 || (oflag & O_RDWR) != 0)
#define DEV_BINDER "/dev/binder"

//common file syscall handler
void syscall_common_handler(pid_t pid, char* syscall, int flag, uid_t uid, gid_t gid);

//open syscall handler
void syscall_open_handler(pid_t pid, int *binder_pid, int flag, uid_t uid, gid_t gid);

//ioctl syscall handler
void syscall_ioctl_handler(pid_t pid, pid_t binder_pid, int flag);

//setuid/gid syscall handler
//return:
//	0 -- do not need to trace this process
//	1 -- need to trace
//and the arg will be set to the argument of this syscall
int syscall_setuid_gid_handler(pid_t pid, pid_t target, int is_gid, long* arg);

//default syscall handler
void syscall_default_handler(pid_t pid);

pid_t ptrace_app_process(pid_t process_pid, int flag)
{
	/*
	if (ptrace_attach(pid)) {
		return -1;
	}
	*/
	pid_t pid = process_pid;
	printf("%d tracing process %d\n", getpid(), process_pid);

	ptrace_setopt(process_pid, PTRACE_O_TRACEFORK | PTRACE_O_TRACECLONE | PTRACE_O_TRACESYSGOOD);

	int binder_fd = -1;
	int status = 0;
	uid_t uid;
	gid_t gid;
	

	/* ptrace(PTRACE_SYSCALL, pid, NULL, NULL); */
	/* pid = waitpid(-1, &status, __WALL); */
	/* ptrace(PTRACE_SYSCALL, pid, NULL, NULL); */
	/* pid = waitpid(-1, &status, __WALL); */
	while(pid > 0){
		if(IS_FORK_EVENT(status) || IS_CLONE_EVENT(status)){
			pid_t newpid;
			ptrace(PTRACE_GETEVENTMSG, pid, NULL,  &newpid);
			/* ptrace(PTRACE_SYSCALL, newpid, NULL, NULL); */
			printf("new thread %d .\n", newpid);
			ptrace_setopt(newpid, PTRACE_O_TRACEFORK | PTRACE_O_TRACECLONE | PTRACE_O_TRACESYSGOOD);
			ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
		} else if(IS_SYSCALL_EVENT(status)){
			//syscall enter
			long syscall_no =  ptrace_get_syscall_nr(pid);
			switch(syscall_no) {
				case __NR_setuid32:
					if (syscall_setuid_gid_handler(pid, process_pid, 0, (long *)&uid) == 0) {
						printf("%d exit\n", getpid());
						return -1;
					} else {
						break;
					}
				case __NR_setgid32:
					if (syscall_setuid_gid_handler(pid, process_pid, 1, (long *)&gid) == 0) {
						printf("%d exit\n", getpid());
						return -1;
					} else {
						break;
					}
				case __NR_stat:
					syscall_common_handler(pid, "stat", flag, uid, gid);
					break;
				case __NR_stat64:
					syscall_common_handler(pid, "stat64", flag, uid, gid);
					break;
				/* case __NR_newstat: */
				/* 	syscall_common_handler(pid, "newstat", flag, uid, gid); */
				/* 	break; */
				case __NR_lstat:
					syscall_common_handler(pid, "lstat", flag, uid, gid);
					break;
				case __NR_lstat64:
					syscall_common_handler(pid, "lstat64", flag, uid, gid);
					break;
				/* case __NR_newlstat: */
				/* 	syscall_common_handler(pid, "newlstat", flag, uid, gid); */
				/* 	break; */
				case __NR_readlink:
					syscall_common_handler(pid, "readlink", flag, uid, gid);
					break;
				case __NR_statfs:
					syscall_common_handler(pid, "statfs", flag, uid, gid);
					break;
				case __NR_statfs64:
					syscall_common_handler(pid, "statfs64", flag, uid, gid);
					break;
				case __NR_truncate:
					syscall_common_handler(pid, "truncate", flag, uid, gid);
					break;
				case __NR_truncate64:
					syscall_common_handler(pid, "truncate64", flag, uid, gid);
					break;
				/* case __NR_utime: */
				/* 	syscall_common_handler(pid, "utime", flag, uid, gid); */
				/* 	break; */
				case __NR_utimes:
					syscall_common_handler(pid, "utimes", flag, uid, gid);
					break;
				case __NR_access:
					syscall_common_handler(pid, "access", flag, uid, gid);
					break;
				case __NR_chdir:
					syscall_common_handler(pid, "chdir", flag, uid, gid);
					break;
				case __NR_chroot:
					syscall_common_handler(pid, "chroot", flag, uid, gid);
					break;
				case __NR_chmod:
					syscall_common_handler(pid, "chmod", flag, uid, gid);
					break;
				case __NR_chown:
					syscall_common_handler(pid, "chown", flag, uid, gid);
					break;
				case __NR_chown32:
					syscall_common_handler(pid, "chown32", flag, uid, gid);
					break;
				case __NR_lchown:
					syscall_common_handler(pid, "lchown", flag, uid, gid);
					break;
				case __NR_lchown32:
					syscall_common_handler(pid, "lchown32", flag, uid, gid);
					break;
				case __NR_open:
					syscall_open_handler(pid, &binder_fd, flag, uid, gid);
					break;
				case __NR_creat:
					syscall_common_handler(pid, "creat", flag, uid, gid);
					break;
				case __NR_mknod:
					syscall_common_handler(pid, "mknod", flag, uid, gid);
					break;
				case __NR_mkdir:
					syscall_common_handler(pid, "mkdir", flag, uid, gid);
					break;
				case __NR_rmdir:
					syscall_common_handler(pid, "rmdir", flag, uid, gid);
					break;
				case __NR_acct:
					syscall_common_handler(pid, "acct", flag, uid, gid);
					break;
				case __NR_uselib:
					syscall_common_handler(pid, "uselib", flag, uid, gid);
					break;
				//TODO: link syscall handler
				case __NR_unlink:
					syscall_common_handler(pid, "unlink", flag, uid, gid);
				case __NR_ioctl:
					syscall_ioctl_handler(pid, binder_fd, flag);
					break;
				default:
					ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
					break;
			}

		} else {
			ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
		}
		pid = waitpid(-1, &status, __WALL);
	}
	/* ptrace_detach(pid); */
	printf("%d exit\n", getpid());
	return -1;
}


/**
 * Handle syscalls that take a path as the first parameter
 */
void syscall_common_handler(pid_t pid, char* syscall, int flag, uid_t uid, gid_t gid)
{
	tracee_ptr_t arg0 = (tracee_ptr_t) ptrace_get_syscall_arg(pid, 0);

	int len = ptrace_strlen(pid, arg0);
	char path[len + 1];
	//first arg of open is path addr
	ptrace_read_data(pid, path, arg0, len + 1);

	int nth_dir;

	if ((flag & SANDBOX_FLAG) && FILE_SANDBOX_ENABLED && (nth_dir = check_prefix_dir(path,SANDBOX_PATH_INTERNAL)) > 0) {
		//internal file storage sandbox
		char* sub_dir = get_nth_dir(path, nth_dir + 2);
		if (!check_prefix(sub_dir, SANDBOX_PATH_INTERNAL_EXCLUDE)) {
			char new_path[len + 1];
			//replace dir in path with LINK_PREFIX
			char* second_dir = get_nth_dir(path, nth_dir + 1);
			strcpy(new_path, SANDBOX_LINK);
			strcat(new_path, second_dir);
			ptrace_write_data(pid, new_path, arg0, len + 1);
			// create require folder
			create_nth_dir(new_path, 3, uid, gid, 0751);
			printf("pid %d %s: %s ==> new path: %s", pid, syscall, path, new_path);

			// return from open syscall, reset the path
			ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
			pid = waitpid(pid, NULL, __WALL);

			ptrace_write_data(pid, path, arg0, len + 1);
			long result = ptrace_get_syscall_arg(pid, 0);
			printf(" = %ld\n", result);

			ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
			return;
		}
	/* } else if ((flag & SANDBOX_FLAG) && FILE_SANDBOX_ENABLED && (nth_dir = check_prefix_dir(path,SANDBOX_PATH_EXTERNAL)) > 0) { */
	/* 	//external file storage sandbox */
	/* 	char new_path[len + 1]; */
	/* 	//replace dir in path with LINK_PREFIX */
	/* 	char* second_dir = get_nth_dir(path, nth_dir + 1); */
	/* 	strcpy(new_path, SANDBOX_LINK); */
	/* 	strcat(new_path, second_dir); */
	/* 	ptrace_write_data(pid, new_path, arg0, len + 1); */
	/* 	printf("pid %d %s: %s\n ==> new path: %s\n", pid, syscall, path, new_path); */

	/* 	// return from open syscall, reset the path */
	/* 	ptrace(PTRACE_SYSCALL, pid, NULL, NULL); */
	/* 	pid = waitpid(pid, NULL, __WALL); */

	/* 	ptrace_write_data(pid, path, arg0, len + 1); */

	/* 	ptrace(PTRACE_SYSCALL, pid, NULL, NULL); */
	/* 	return; */
	}
	//default
	printf("pid %d %s: %s\n", pid, syscall, path);
	syscall_default_handler(pid);
}


void syscall_open_handler(pid_t pid, int *binder_fd, int flag, uid_t uid, gid_t gid)
{
	tracee_ptr_t arg0 = (tracee_ptr_t) ptrace_get_syscall_arg(pid, 0);
	//arg1 is oflag
	long arg1 = ptrace_get_syscall_arg(pid, 1);

	int len = ptrace_strlen(pid, arg0);
	char path[len + 1];
	//first arg of open is path addr
	ptrace_read_data(pid, path, arg0, len + 1);

	if (strcmp(path, DEV_BINDER) == 0) {
		// return from open syscall, read the result fd
		ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
		pid = waitpid(pid, NULL, __WALL);

		// reg0 will be the result of open
		*binder_fd = (int) ptrace_get_syscall_arg(pid , 0);
		printf("pid %d open binder: %d\n", pid, *binder_fd);

		ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
		return;
		
	} else {
		syscall_common_handler(pid, "open", flag, uid, gid);
		return;
		
	}
}


void syscall_ioctl_handler(pid_t pid, int binder_fd, int flag)
{
	/* printf("ioctl from %d\n", pid); */
	if(binder_fd < 0) {
		ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
	}

	// arg0 will be the file description
	long arg0 = ptrace_get_syscall_arg(pid, 0);

	if((int)arg0 == binder_fd) {
		binder_ioctl_handler(pid);
		ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
	} else {
		ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
	}
}

int syscall_setuid_gid_handler(pid_t pid, pid_t target, int is_gid, long* arg)
{
	//arg0 will be the uid/gid
	long arg0 = ptrace_get_syscall_arg(pid, 0);
	*arg = arg0;
	if(!PROCESS_FILTER_ENABLED || pid != target){
		syscall_default_handler(pid);
		return 1;
	}
	FILE *filter_file;
	if((filter_file = fopen(PROCESS_FILTER_PATH,"r")) == NULL){
		printf("Error: failed to open process filter file");
		syscall_default_handler(pid);
		return 1;
	}
	long uid,gid;
	int result = 0;
	while(!feof(filter_file)){
		fscanf(filter_file, "%ld %ld", &uid, &gid);
		if(is_gid && gid == arg0){
			result = 1;
			break;
		} else if(!is_gid && uid == arg0){
			result = 1;
			break;
		}
	}
	fclose(filter_file);
	syscall_default_handler(pid);

	return result;
}

void syscall_default_handler(pid_t pid)
{
	ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
	pid = waitpid(pid, NULL, __WALL);
	//syscall return
	ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
}
