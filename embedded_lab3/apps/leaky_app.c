#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_RECORDS 100
#define MSG_SIZE 256

typedef struct {
    int id;
    char *name;
    int *scores;
} Record;

Record *create_record(int id, const char *name, int num_scores)
{
    Record *r = malloc(sizeof(Record));

    r->id = id;

    r->name = malloc(strlen(name) + 1);
    strcpy(r->name, name);

    r->scores = malloc(sizeof(int) * num_scores);

    for (int i = 0; i < num_scores; i++)
        r->scores[i] = id * 10 + i;

    return r;
}

void delete_record(Record *r)
{
    /* Memory leak:
       free(r->name);
       free(r->scores);
    */

    free(r);
}

void process_messages(int count)
{
    for (int i = 0; i < count; i++) {
        char *buf = malloc(MSG_SIZE);

        snprintf(buf, MSG_SIZE,
                 "Message #%d: data payload here", i);

        if (i % 10 == 0)
            printf("Processed: %s\n", buf);

        /* Memory leak: free(buf) is missing */
    }
}

int main(void)
{
    printf("=== Leaky App - LAB-03 Memory Analysis ===\n");

    printf("[1] Creating %d records...\n", NUM_RECORDS);

    Record **records =
        malloc(sizeof(Record *) * NUM_RECORDS);

    for (int i = 0; i < NUM_RECORDS; i++)
        records[i] =
            create_record(i, "EmbeddedLinuxStudent", 5);

    printf("[2] Deleting records (with leak)...\n");

    for (int i = 0; i < NUM_RECORDS; i++)
        delete_record(records[i]);

    free(records);

    printf("[3] Processing messages (with leak)...\n");

    process_messages(50);

    printf("Program finished - check valgrind output!\n");

    return 0;
}
