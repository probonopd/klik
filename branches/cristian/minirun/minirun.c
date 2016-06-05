/*
 * Klik2 Minirun - Proof of Concept
 * Copyright (C) 2008 Cristian Greco
 * GPLv2 only
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/wait.h>

#define TARCMD "/bin/tar"
#define RUNTIME_TGZ "runtime.tgz"
#define IMAGE_TGZ "image.tgz"
#define UMVIEW "umview"

// this is just a tricky way to launch our app
// it would be better to provide a /start link inside our iso image
#define MOUNT_CMD \
	"export PATH=$PATH:/usr/games " \
	"&& mount -t umfuseiso9660 -o merge,except=/usr/lib/libglib-2.0.so.0 image.cmg / " \
	"&& APP=$(grep \"group main\" /recipe.xml | cut -d \"\\\"\" -f 2) " \
	"&& $APP "

extern const char *_binary_runtime_tgz_start[];
extern const char *_binary_runtime_tgz_size[];
extern const char *_binary_image_tgz_start[];
extern const char *_binary_image_tgz_size[];

static int write_to_disk(char *path, void *buf, size_t count)
{
	int fd;

	if ((fd=open(path,O_CREAT|O_RDWR,0600))<0) {
		perror("open");
		return -1;
	}

	if (write(fd,buf,count)<0) {
		perror("write");
		return -1;
	}

	if (close(fd)<0) {
		perror("close");
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int fd, status;
	pid_t pid;
	char klikdir[PATH_MAX];

	snprintf(klikdir,PATH_MAX,"/tmp/.klik_%ld",(long int)getpid());
	if (mkdir(klikdir,0700)<0) {
		perror("mkdir");
		exit(EXIT_FAILURE);
	}

	if (chdir(klikdir)<0) {
		perror("chdir");
		exit(EXIT_FAILURE);
	}

	if (write_to_disk(RUNTIME_TGZ,(void *)_binary_runtime_tgz_start,(size_t)_binary_runtime_tgz_size)<0)
		exit(EXIT_FAILURE);

	if (write_to_disk(IMAGE_TGZ,(void *)_binary_image_tgz_start,(size_t)_binary_image_tgz_size)<0)
		exit(EXIT_FAILURE);


	if ((pid=fork())<0) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid==0) {
		/* child #1 */
		if ((pid=fork())<0) {
			perror("fork");
			exit(EXIT_FAILURE);
		}

		if (pid==0) {
			/* child #2 - unpack image archive */
			char *newargv[]={"tar","zxf",IMAGE_TGZ,NULL};
			execv(TARCMD,newargv);
		} else {
			/* continue child #1 - unpack runtime archive */
			char *newargv[]={"tar","zxf",RUNTIME_TGZ,NULL};

			if (wait(&status)<0) {
				perror("wait");
				exit(EXIT_FAILURE);
			}

			if (!WIFEXITED(status)) {
				fprintf(stderr,"child error\n");
				exit(EXIT_FAILURE);
			}

			execv(TARCMD,newargv);
		}

		/* never reach here */
		perror("execv");
		exit(EXIT_FAILURE);
	}

	/* parent - wait child #1 and launch umview*/
	char *newargv[]={"umview","-q","-p","umfuse","/bin/sh","-c",MOUNT_CMD,NULL};

	setenv("LD_LIBRARY_PATH",klikdir,1);

	if (wait(&status)<0) {
		perror("wait");
		exit(EXIT_FAILURE);
	}

	if (!WIFEXITED(status)) {
		fprintf(stderr,"child error\n");
		exit(EXIT_FAILURE);
	}

	if (unlink(RUNTIME_TGZ)<0) {
		perror("unlink");
		exit(EXIT_FAILURE);
	}

	if (unlink(IMAGE_TGZ)<0) {
		perror("unlink");
		exit(EXIT_FAILURE);
	}

	execv(UMVIEW,newargv);

	/* never reach here */
	perror("execv");
	exit(EXIT_FAILURE);
}
