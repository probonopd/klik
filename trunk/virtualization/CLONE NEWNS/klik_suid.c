/* 
 * suid wrapper for klik seamless application overlay
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sched.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

/* This is run suid root! */

int do_mount(char* command, int klik_nr, int uid, int fuse_fd)
{
	char* args[255];
	char* envp[255];
	
	char s_klik_nr[256];
	char s_uid[256];
	char s_fuse_fd[256];

	int i=0;
	
	snprintf(s_klik_nr, 255, "%d", klik_nr);
	snprintf(s_uid, 255, "%d", uid);
	snprintf(s_fuse_fd, 255, "%d", fuse_fd);

	args[i++]="/usr/bin/klik-overlay";
	args[i++]=command;
	args[i++]=s_klik_nr;
	args[i++]=s_uid;
	args[i++]=s_fuse_fd;
	args[i++]=NULL;

	/* Clear environment to be secure */
	envp[0]=NULL;

	execve(args[0], args, envp);
	
	perror("execv");
	exit(255);
}

void do_command(char *command, int uid, int fuse_fd, int argc, char* argv[])
{
	pid_t pid;
	int status = 255;
	int klik_nr;
	
	/* FIXME: Safely get klik_nr */

	if (!argv || !argv[0] || !argv[1])
		exit(255);

	klik_nr=(int)(argv[1][0]-'0');

	if (klik_nr < 0 || klik_nr > 9)
		exit(255);

	/* Spawn the mount/prepare command and wait for it */

	pid=fork();

	if (pid < 0)
	{
		perror("fork");
		exit(1);
	}

	if (pid == 0)
	{
		do_mount(command, klik_nr, uid, fuse_fd);
		exit(255);
	}

	/* Wait for our child */

	wait(&status);

	/* Mount failed? */
	if (status != 0)
	{
		fprintf(stderr, "%s failed. Exiting ...\n", command);
		exit(255);
	}
}

/* This is also run suid root */

int newns_main(int uid, int fuse_fd, int argc, char* argv[])
{
	do_command("mount", uid, fuse_fd, argc, argv);

	/* Drop privileges */
	setuid(uid);
	seteuid(getuid());

/* Now we are the calling user again ------------- SECURE SECTION */
		
	/* Drop argv[0] we don't need it */
	argv++;
	argc--;
	
	/* Skip klik_nr */
	argv++;
	argc--;

	/* Exec program */
	execv(argv[0], argv);

	/* Should never reach here */
	perror("execv");
	exit(255);

/* END OF ----------------- SECURE SECTION */
}

/* This part is run suid root, so be careful about security */

int main(int argc, char* argv[])
{
	pid_t pid;
	int status;
	int uid;
	int fds[2];
	
	if (argc < 3)
	{
		/* Do not use argv[0] here, because we cannot trust it! */
		fprintf(stderr, "Usage: klik_suid <appdir nr> <program> [args]\n");
		exit(1);
	}

	if (geteuid() != 0)
	{
		fprintf(stderr, "klik_suid needs to be suid root\n");
		exit(1);
	}

	/* FIXME: Close all filedescriptors and reopen stdin, stdout, stderr from 
	 *        /dev/tty. Else this program can be used to break out of a chroot jail.
	 */
	
	/* Gain root */
	uid=getuid();
	setuid(0);

	/* Prepare fuse_fd */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
	{
		perror("socketpair");
		exit(255);
	}

	/* Prepare filesystem */
	do_command("prepare", uid, fds[0], argc, argv);

	/* Change namespace */
	
	pid=(pid_t)syscall(SYS_clone, SIGCHLD | CLONE_NEWNS, NULL);

	if (pid < 0)
	{
		perror("sys_clone");
		exit(1);
	}
	if (pid == 0)
	{
		/* child */
		newns_main(uid, fds[1], argc, argv);
		
		/* Should never be reached */
		exit(255);
	}

	/* Parent waits for its child */
	
	wait(&status);

	return status;
}
