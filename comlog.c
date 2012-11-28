#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <mqueue.h>
#include <signal.h>
#include <sys/mman.h>
#include <sched.h>

#define MSG_SIZE 256
#define DEFAULT_BLOCK_SIZE 4096
#define DUMP_FILE "/tmp/comlog.dump"

unsigned long int get_block_size(char*);
void sig_quit(int);

typedef struct {
	unsigned long tail, head, size;
	void *address;
} ring_buffer;

ring_buffer buffer;
int dump_fd, fd;
int block_size, q_mult, max_msgs, num_msgs;
mqd_t mqdes;

int main(void)
{
	int len, res;
	struct mq_attr attr;
	const char *MQ_NAME = "/mq-comlog01";

	printf("opening dump file...\n");
	/* open dump file */
	dump_fd = fopen(DUMP_FILE, "a");
	if (dump_fd < 0) {
		perror("fopen (dump-file)");
		exit(1);
	}

	printf("setting up queue...\n");
	attr.mq_maxmsg = 1000;
	attr.mq_msgsize = MSG_SIZE;

	mqdes = mq_open(MQ_NAME, O_RDONLY|O_CREAT, 0777, &attr);
	if (mqdes < 0) {
		perror("mq_open");
		exit(1);
	}

	printf("allocating buffer...\n");

	/* allocate buffer */
	block_size = get_block_size(DUMP_FILE);
	if (block_size < 0) {
		perror("could not get block size, defaulting to 4096 bytes");
		block_size = DEFAULT_BLOCK_SIZE;
	}
	printf("using block size of %d bytes\n", block_size);
	q_mult = 512; //256;
	buffer.size = block_size * q_mult;

	/* valloc /
	buffer.address = valloc(buffer.size);
	if (buffer.address >= 0) {
		printf("using page size aligned memory\n");
	} else {
		perror("valloc buffer");
		buffer.address = malloc(buffer.size);
		if (buffer.address < 0) {
			perror("malloc buffer");
			exit(1);
		}
	}*/

	/* mmap */
	char devfile[] = "/tmp/comlog-shm-XXXXXX";
	void *address;
	fd = mkstemp(devfile);
	if (fd < 0) {
		perror("mkstemp");
	}
	printf("using shm file %s\n", devfile);
	if (unlink(devfile) < 0) {
		perror("unlink");
	}
	if (ftruncate(fd, buffer.size) < 0) {
		perror("ftruncate");
	}
	fd = fopen(DUMP_FILE, "rw");
	buffer.address = mmap(NULL, buffer.size, PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
	if (buffer.address < 0) {
		perror("mmap");
		exit(1);
	}
	close(fd);

	/* madvise */
	if (0 < madvise(buffer.address, buffer.size, MADV_SEQUENTIAL)) {
		perror("madvice");
	}

	/* lock memory to avoid swapping*/
	if (0 < mlockall(MCL_CURRENT | MCL_FUTURE)) {
		perror("mlockall");
	}

	/* set scheduling */
	struct sched_param schedp;
	int sched_prio = sched_get_priority_max(SCHED_FIFO);
	if (sched_prio < 0) {
		perror("sched_get_priority_max - defaulting to 32");
		sched_prio = 32;
	}
	schedp.sched_priority = sched_prio;
	if (sched_setscheduler(0, SCHED_FIFO, &schedp) < 0) {
		perror("sched_setscheduler");
	}

	/* set tail and head pointers */
	buffer.tail = 0;
	buffer.head = 0;

	printf("using buffer size of %d bytes\n", buffer.size);

	num_msgs = 0;
	max_msgs = buffer.size / MSG_SIZE;
	printf("max number of messages in buffer %d\n", max_msgs);

	/* register signal handlers */
	struct sigaction kill_act;
     
	kill_act.sa_handler = sig_quit;
	sigemptyset(&kill_act.sa_mask);
	kill_act.sa_flags = 0;
	if(sigaction(SIGINT, &kill_act, NULL)) {
		perror("sigaction");
	}

	printf("ready to accept messages...\n");
	while(1) {
		len = 0;
		while(len < MSG_SIZE) {
			res = mq_receive(mqdes, buffer.head + len, MSG_SIZE - len, NULL);
			if (res < 0) {
				perror("mq_receive");
				continue;
			} else if (res == 0) {
				break;
			}
			len += res;
		}
		buffer.head = '\0';

		/* advance head pointer */
		buffer.head += MSG_SIZE;
		num_msgs++;

		if (num_msgs == max_msgs) {
			// append to file
			//fwrite(buffer.address, block_size, q_mult, dump_fd);
			if(msync(buffer.address, buffer.size, MS_SYNC) < 0) {
				perror("msync");
			}
			printf("dumped buffer to file (%d messages)\n", num_msgs);
			num_msgs = 0;
		}
	}
	
	munlockall();
	munmap(buffer.address, buffer.size);

	return 0;
}

unsigned long int get_block_size(char *path)
{
	struct statvfs st;
	if (statvfs(path, &st) < 0) {
		perror("statvfs");	
		return -1;
	}
	
	return (unsigned long int)st.f_bsize;
}

void sig_quit(int signum)
{
	/* stop receiving msgs */

	/* close queue */
	printf("closing queue...\n");
	if(mq_close(mqdes)) {
		perror("mq_close");
	}

	/* dump buffer to file */
	printf("dumping buffer to file...\n");

	/* free memory */
	printf("freeing memory...\n");
	munlockall();
	munmap(buffer.address, buffer.size);

	printf("quiting...");
	exit(0);
}
