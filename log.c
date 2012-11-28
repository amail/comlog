#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <mqueue.h>

char logg(mqd_t mqdes, char *mesg, int argc, ...)
{
	char s[256];
	int i = 0;

	va_list ap;
	va_start(ap, argc);
	s[0] = 0;

	while(i < argc) {
		s[i++] = (char)va_arg(ap, int);
	}

	s[i++] = '\0';
        va_end(ap);

	int len = strlen(mesg);
	//memcpy(s + i, mesg, len+1);

	i = mq_send(mqdes, s, 256, 1);
	if (i < 0) {
		perror("mq_send");
		exit(1);
	}	

	return 1;
}

void benchmark(int rounds)
{
	int i = 0;
	char *dummy = "message\0";
	const char *MQ_NAME = "/mq-comlog01";

	mqd_t mqdes;
	mqdes = mq_open(MQ_NAME, O_WRONLY);
	if (mqdes < 0) {
		perror("mq_open");
		exit(1);
	}
	while(i < rounds) {
		logg(mqdes, dummy, 4, (char)65, (char)65, (char)66, (char)89);
		i++;
	}

	return;
}

int main(void)
{
	benchmark((int)2048*2*2*2*2*2*2*2);

	return 0;
}
