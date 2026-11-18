#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <unistd.h>

static const long MSG_TYPE_FINISH_RUN   = 1;  
static const long MSG_TYPE_REGISTER     = 2;
static const long MSG_TYPE_STICK        = MSG_TYPE_REGISTER + 1;

void judge(int qId, unsigned int nRunners);
void runner(int qId, unsigned int n, unsigned int nMax);

int main(int argc, char* argv[]) {
	if (argc < 2)
		return 0;

	int nRunnersRaw = atoi(argv[1]);
	if (nRunnersRaw <= 0)
		return 0;
	
	unsigned int nRunners = (unsigned int)nRunnersRaw;
	printf("Hello, world!\n");
	
	int msgId = msgget(IPC_PRIVATE, IPC_CREAT | 0700);
	pid_t judgePid = fork();
	if (judgePid == 0) {
		judge(msgId, nRunners);
		exit(0);
	}

	for (size_t i = 0; i < nRunners; i++) {
		pid_t runnerPid = fork();
		if (runnerPid == 0) {
			runner(msgId, i + 1, nRunners);
			exit(0);
		}
	}

	for (size_t i = 0; i < nRunners + 1; i++)
		wait(NULL);

	msgctl(msgId, IPC_RMID, NULL);	
	printf("Goodbye, world!\n");
	return 0;
}

void judge(int qId, unsigned int nRunners) {
	printf("I am the judge!\n");
	for (size_t i = 0; i < nRunners; i++) {
		msgrcv(qId, NULL, 0, 0, 0);
		printf("Registered a runner\n");
	}

	long msgType = MSG_TYPE_STICK;
	msgsnd(qId, &msgType, 0, 0);
	msgrcv(qId, NULL, 0, MSG_TYPE_FINISH_RUN, 0);
	printf("The judge has concluded the relay's end!\n");
}

void runner(int qId, unsigned int n, unsigned int nMax) {
	printf("I am a runner #%d/%d!\n", n, nMax);

	long msgType = MSG_TYPE_REGISTER;
	msgsnd(qId, &msgType, 0, 0);

	msgrcv(qId, NULL, 0, MSG_TYPE_STICK + n - 1, 0);
	printf("Runner #%d is running...\n", n);

	printf("Runner #%d has finished their run!\n", n);
	msgType = (n < nMax) ? MSG_TYPE_STICK + n : MSG_TYPE_FINISH_RUN;
	msgsnd(qId, &msgType, 0, 0);
}
