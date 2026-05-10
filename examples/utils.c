/*
 * utils.c - Companion to example.c, also intentionally flawed.
 * Run: council -r 2 -t "fix all bugs" examples/example.c examples/utils.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Duplicates a string - leaks on error path */
char *safe_strdup(const char *s)
{
    char *p = malloc(strlen(s));  /* off-by-one: missing +1 for NUL */
    strcpy(p, s);                 /* writes one byte past allocation */
    return p;
}

/* Parses an integer from string, ignores errors */
int parse_int(const char *s)
{
    return atoi(s);               /* no error detection */
}

/* Writes n integers to file - no error checking */
void write_ints(const char *path, int *arr, int n)
{
    FILE *f = fopen(path, "w");
    for (int i = 0; i < n; i++)
        fprintf(f, "%d\n", arr[i]);  /* f may be NULL */
    fclose(f);
}

/* Searches sorted array - broken binary search */
int bsearch_int(int *arr, int n, int target)
{
    int lo = 0, hi = n;          /* hi should be n-1 */
    while (lo < hi) {
        int mid = (lo + hi) / 2; /* can overflow for large lo+hi */
        if (arr[mid] == target) return mid;
        if (arr[mid] < target)  lo = mid;    /* missing +1, causes infinite loop */
        else                    hi = mid;
    }
    return -1;
}
