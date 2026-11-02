#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef PAUSE_FOR_PROC
#include <unistd.h>
#endif

#define NUM_RECORDS 100
#define MSG_SIZE    256

typedef struct {
    int   id;
    char *name;    /* Heap allocated - de bi quen free */
    int  *scores;  /* Heap allocated - de bi quen free */
} Record;

/* Leak 1: name khong duoc free khi record bi xoa */
Record *create_record(int id, const char *name, int num_scores) {
    Record *r = malloc(sizeof(Record));
    r->id = id;
    r->name = malloc(strlen(name) + 1);
    strcpy(r->name, name);
    r->scores = malloc(sizeof(int) * num_scores);
    for (int i = 0; i < num_scores; i++)
        r->scores[i] = id * 10 + i;
    return r;
}

/* Bug: free record nhung QUEN free r->name va r->scores */
void delete_record(Record *r) {
#ifdef FIXED
    free(r->name);
    free(r->scores);
#endif
    /* free(r->name);   <-- BI COMMENT = memory leak */
    /* free(r->scores); <-- BI COMMENT = memory leak */
    free(r);
}

/* Leak 2: Buffer trong vong lap khong duoc free */
void process_messages(int count) {
    for (int i = 0; i < count; i++) {
        char *buf = malloc(MSG_SIZE);
        snprintf(buf, MSG_SIZE, "Message #%d: data payload here", i);
        if (i % 10 == 0)
            printf("  Processed: %s\n", buf);
#ifdef FIXED
        free(buf);
#endif
        /* free(buf); <-- BI THIEU */
    }
}

int main(void) {
    printf("=== Leaky App - LAB-03 Memory Analysis ===\n");
#ifdef PAUSE_FOR_PROC
    /* Chi dung cho phan B.5: cho thoi gian doc /proc/PID/status truoc khi cap phat */
    sleep(3);
#endif
    printf("[1] Creating %d records...\n", NUM_RECORDS);
    Record **records = malloc(sizeof(Record *) * NUM_RECORDS);
    for (int i = 0; i < NUM_RECORDS; i++)
        records[i] = create_record(i, "EmbeddedLinuxStudent", 5);

    printf("[2] Deleting records (with leak)...\n");
    for (int i = 0; i < NUM_RECORDS; i++)
        delete_record(records[i]);
    free(records);

    printf("[3] Processing messages (with leak)...\n");
    process_messages(50);

    printf("Program finished - check valgrind output!\n");
#ifdef PAUSE_FOR_PROC
    /* Cho thoi gian doc /proc/PID/status sau khi leak */
    sleep(5);
#endif
    return 0;
}
