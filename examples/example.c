/*
 * example.c - Intentionally flawed file used to demonstrate llm-council.
 * Run: council -t "fix bugs and improve robustness" examples/example.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 64

/* Concatenates two strings into a fixed buffer - has bugs */
char *concat(const char *a, const char *b)
{
    char *buf = malloc(BUF_SIZE);
    strcpy(buf, a);          /* no bounds check */
    strcat(buf, b);          /* potential overflow */
    return buf;
}

/* Reads a line from stdin - has bugs */
void read_input(char *out)
{
    gets(out);               /* deprecated, unsafe */
}

/* Counts lines in a file - has bugs */
int count_lines(const char *path)
{
    FILE *f = fopen(path, "r");
    int   count = 0;
    char  line[256];

    while (fgets(line, sizeof(line), f))   /* no NULL check on f */
        count++;

    /* f is never closed */
    return count;
}

/* Finds an item in an array - has a logic error */
int find_item(int *arr, int len, int target)
{
    for (int i = 0; i <= len; i++) {      /* off-by-one: should be < */
        if (arr[i] == target)
            return i;
    }
    return -1;
}

int main(void)
{
    char input[BUF_SIZE];
    read_input(input);

    char *result = concat("Hello, ", input);
    printf("%s\n", result);
    /* result is never freed */

    int arr[] = {1, 2, 3, 4, 5};
    printf("Found at index: %d\n", find_item(arr, 5, 3));

    printf("Lines in this file: %d\n", count_lines("examples/example.c"));
    return 0;
}
