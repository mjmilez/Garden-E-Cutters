#define _GNU_SOURCE
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static atomic_int g_running = 1;

static void sleepMs(long ms)
{
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
}

static void handleSignal(int sig)
{
	(void)sig;
	atomic_store(&g_running, 0);
}

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	// Clean shutdown on Ctrl+C (SIGINT) and systemd stop (SIGTERM)
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handleSignal;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	printf("[base-fw-rpi] boot\n");
	fflush(stdout);

	unsigned long tick = 0;

	// Serial monitor heartbeat
	while (atomic_load(&g_running)) {
		printf("[base-fw-rpi] alive tick=%lu\n", tick++);
		fflush(stdout);
		sleepMs(1000);
	}

	printf("[base-fw-rpi] shutdown\n");
	fflush(stdout);
	return 0;
}