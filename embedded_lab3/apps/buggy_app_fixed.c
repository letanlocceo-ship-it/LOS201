#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void process_data(int *data, int len)
{
    int sum = 0;

    for (int i = 0; i < len; i++) {
        sum += data[i];
    }

    printf("Sum = %d\n", sum);
}

char *create_message(const char *text)
{
    char *buf = malloc(strlen(text) + 1);

    strcpy(buf, text);

    return buf;
}

long long calculate(int a, int b)
{
    long long result = (long long)a * (long long)b;

    printf("calculate(%d, %d) = %lld\n", a, b, result);

    return result;
}

int main(int argc, char *argv[])
{
    printf("=== Buggy App v1.0 - LAB-03 Debug Target ===\n");

    int arr[5] = {10, 20, 30, 40, 50};

    printf("[TEST1] Processing array of 5 elements\n");
    process_data(arr, 5);

    printf("[TEST2] Creating message\n");

    char *msg = create_message("Hello Embedded Linux");
    printf("Message: %s\n", msg);
    free(msg);

    printf("[TEST3] Integer calculation\n");
    calculate(100000, 100000);

    printf("Done.\n");

    return 0;
}
