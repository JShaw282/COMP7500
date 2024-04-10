#Jackson Shaw 3/11/24 OS3
CC = gcc
CFLAGS = -Wall -pthread

all: aubatch batch_job

aubatch: aubatch.c
	$(CC) $(CFLAGS) -o aubatch aubatch.c

batch_job: batch_job.c
	$(CC) $(CFLAGS) -o batch_job batch_job.c

clean:
	rm -f aubatch batch_job
