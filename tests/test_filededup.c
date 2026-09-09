#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#define _POSIX_C_SOURCE 200809L

#include "../src/filededup.h"

typedef struct
{
    const char *name;
    void (*func)(void);
} TestCase;


/* Write a file with the given content. */
static void write_file(const char *name, const char *content)
{
    FILE *f = fopen(name, "wb");
    assert(f != NULL);
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

/* Write a file of the given size with a repeating pattern. */
static void write_pattern_file(const char *name, int size, const char *pattern)
{
    size_t pattern_len = strlen(pattern);
    assert(pattern_len > 0);

    FILE *f = fopen(name, "wb");
    assert(f != NULL);

    for (int i = 0; i < size; i++)
    {
        unsigned char c = (unsigned char)pattern[i % pattern_len];
        fwrite(&c, 1, 1, f);
    }

    fclose(f);
}

/* Free memory returned by FDDump. */
static void free_dump(char **dump, int length)
{
    if (dump == NULL)
        return;

    for (int i = 0; i < length; i++)
    {
        free(dump[i]); /* free(NULL) is allowed */
    }
    free(dump);
}

/* Check whether a path is present in the dump. */
static int contains_path(char **dump, int length, const char *path)
{
    for (int i = 0; i < length; i++)
    {
        if (dump[i] != NULL && strcmp(dump[i], path) == 0)
            return 1;
    }
    return 0;
}

/* Count NULL separators in the dump. */
static int count_nulls(char **dump, int length)
{
    int count = 0;
    for (int i = 0; i < length; i++)
    {
        if (dump[i] == NULL)
            count++;
    }
    return count;
}

/* Check whether a group matches exactly two expected paths, regardless of order. */
static int group_matches_two(char **dump, int start, int end, const char *a, const char *b)
{
    if (end - start != 2)
        return 0;

    return ((strcmp(dump[start], a) == 0 && strcmp(dump[start + 1], b) == 0) ||
            (strcmp(dump[start], b) == 0 && strcmp(dump[start + 1], a) == 0));
}

/* Expect no duplicate groups. */
static void expect_no_duplicates(FILEDEDUP fd)
{
    int len = -1;
    char **dump = FDDump(fd, &len);

    assert(dump == NULL);
    assert(len == 0);
}

/* Expect exactly one group with two files. */
static void expect_one_group(FILEDEDUP fd, const char *a, const char *b)
{
    int len = 0;
    char **dump = FDDump(fd, &len);

    assert(dump != NULL);
    assert(len == 3); /* a, b, NULL */
    assert(contains_path(dump, len, a));
    assert(contains_path(dump, len, b));
    assert(count_nulls(dump, len) == 1);

    free_dump(dump, len);
}

/* Expect exactly one group with three files. */
static void expect_one_group_of_three(FILEDEDUP fd, const char *a, const char *b, const char *c)
{
    int len = 0;
    char **dump = FDDump(fd, &len);

    assert(dump != NULL);
    assert(len == 4); /* a, b, c, NULL */
    assert(contains_path(dump, len, a));
    assert(contains_path(dump, len, b));
    assert(contains_path(dump, len, c));
    assert(count_nulls(dump, len) == 1);

    free_dump(dump, len);
}

/* Expect exactly two duplicate groups of two files each. */
static void expect_two_groups(FILEDEDUP fd,
                              const char *a1, const char *a2,
                              const char *b1, const char *b2)
{
    int len = 0;
    char **dump = FDDump(fd, &len);

    assert(dump != NULL);
    assert(len == 6); /* 4 files + 2 NULL */
    assert(count_nulls(dump, len) == 2);

    int sep1 = -1;
    int sep2 = -1;

    for (int i = 0; i < len; i++)
    {
        if (dump[i] == NULL)
        {
            if (sep1 == -1)
                sep1 = i;
            else
                sep2 = i;
        }
    }

    assert(sep1 != -1);
    assert(sep2 != -1);
    assert(sep2 == len - 1); /* last item must end last group */

    int first_is_a = group_matches_two(dump, 0, sep1, a1, a2);
    int second_is_b = group_matches_two(dump, sep1 + 1, sep2, b1, b2);

    int first_is_b = group_matches_two(dump, 0, sep1, b1, b2);
    int second_is_a = group_matches_two(dump, sep1 + 1, sep2, a1, a2);

    assert((first_is_a && second_is_b) || (first_is_b && second_is_a));

    free_dump(dump, len);
}

/* Remove test files. */
static void cleanup(const char *a, const char *b, const char *c, const char *d, const char *e)
{
    if (a)
        remove(a);
    if (b)
        remove(b);
    if (c)
        remove(c);
    if (d)
        remove(d);
    if (e)
        remove(e);
}

/* Test invalid arguments. */
static void test_null_arguments(void)
{
    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(NULL, "x.txt") == 0);
    assert(FDCheck(fd, NULL) == 0);
    assert(FDDump(NULL, &(int){0}) == NULL);
    assert(FDDump(fd, NULL) == NULL);

    printf("test_null_arguments passed\n");
}

/* Test missing file. */
static void test_missing_file(void)
{
    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "this_file_does_not_exist.txt") == 0);

    printf("test_missing_file passed\n");
}

/* Test a single file: no duplicates should be found. */
static void test_one_file_only(void)
{
    write_file("t1.txt", "hello");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t1.txt") == 1);
    expect_no_duplicates(fd);

    cleanup("t1.txt", NULL, NULL, NULL, NULL);
    printf("test_one_file_only passed\n");
}

/* Test two identical small files. */
static void test_two_identical_files(void)
{
    write_file("t2_a.txt", "same");
    write_file("t2_b.txt", "same");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t2_a.txt") == 1);
    assert(FDCheck(fd, "t2_b.txt") == 1);

    expect_one_group(fd, "t2_a.txt", "t2_b.txt");

    cleanup("t2_a.txt", "t2_b.txt", NULL, NULL, NULL);
    printf("test_two_identical_files passed\n");
}

/* Test two identical large files. */
static void test_two_identical_large_files(void)
{
    write_pattern_file("t2_c.txt", 10000, "hello world");
    write_pattern_file("t2_d.txt", 10000, "hello world");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t2_c.txt") == 1);
    assert(FDCheck(fd, "t2_d.txt") == 1);

    expect_one_group(fd, "t2_c.txt", "t2_d.txt");

    cleanup("t2_c.txt", "t2_d.txt", NULL, NULL, NULL);
    printf("test_two_identical_large_files passed\n");
}

/* Test two different files. */
static void test_two_different_files(void)
{
    write_file("t3_a.txt", "hello");
    write_file("t3_b.txt", "world");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t3_a.txt") == 1);
    assert(FDCheck(fd, "t3_b.txt") == 1);

    expect_no_duplicates(fd);

    cleanup("t3_a.txt", "t3_b.txt", NULL, NULL, NULL);
    printf("test_two_different_files passed\n");
}

/* Test two large files with same size but different content. */
static void test_two_different_large_files(void)
{
    write_pattern_file("t3_c.txt", 10000, "hello world");
    write_pattern_file("t3_d.txt", 10000, "goodbye mars");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t3_c.txt") == 1);
    assert(FDCheck(fd, "t3_d.txt") == 1);

    expect_no_duplicates(fd);

    cleanup("t3_c.txt", "t3_d.txt", NULL, NULL, NULL);
    printf("test_two_different_large_files passed\n");
}

/* Test two identical files plus one same-content-pattern file with different size. */
static void test_different_size_same_content(void)
{
    write_pattern_file("t4_a.txt", 5000, "same content");
    write_pattern_file("t4_b.txt", 5000, "same content");
    write_pattern_file("t4_c.txt", 5001, "same content");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t4_a.txt") == 1);
    assert(FDCheck(fd, "t4_b.txt") == 1);
    assert(FDCheck(fd, "t4_c.txt") == 1);

    /* only t4_a and t4_b should be duplicates */
    expect_one_group(fd, "t4_a.txt", "t4_b.txt");

    cleanup("t4_a.txt", "t4_b.txt", "t4_c.txt", NULL, NULL);
    printf("test_different_size_same_content passed\n");
}

/* Test same size, different content. */
static void test_same_size_different_content(void)
{
    write_file("t5_a.txt", "abc123");
    write_file("t5_b.txt", "abc124");

    write_pattern_file("t5_c.txt", 1000, "12345");
    write_pattern_file("t5_d.txt", 1000, "12346");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t5_a.txt") == 1);
    assert(FDCheck(fd, "t5_b.txt") == 1);
    assert(FDCheck(fd, "t5_c.txt") == 1);
    assert(FDCheck(fd, "t5_d.txt") == 1);

    expect_no_duplicates(fd);

    cleanup("t5_a.txt", "t5_b.txt", "t5_c.txt", "t5_d.txt", NULL);
    printf("test_same_size_different_content passed\n");
}

/* Test three identical files. */
static void test_three_identical_files(void)
{
    write_pattern_file("t6_a.txt", 1000000, "duplicate");
    write_pattern_file("t6_b.txt", 1000000, "duplicate");
    write_pattern_file("t6_c.txt", 1000000, "duplicate");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t6_a.txt") == 1);
    assert(FDCheck(fd, "t6_b.txt") == 1);
    assert(FDCheck(fd, "t6_c.txt") == 1);

    expect_one_group_of_three(fd, "t6_a.txt", "t6_b.txt", "t6_c.txt");

    cleanup("t6_a.txt", "t6_b.txt", "t6_c.txt", NULL, NULL);
    printf("test_three_identical_files passed\n");
}

/* Test empty files. */
static void test_empty_files(void)
{
    write_file("t7_a.txt", "");
    write_file("t7_b.txt", "");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t7_a.txt") == 1);
    assert(FDCheck(fd, "t7_b.txt") == 1);

    expect_one_group(fd, "t7_a.txt", "t7_b.txt");

    cleanup("t7_a.txt", "t7_b.txt", NULL, NULL, NULL);
    printf("test_empty_files passed\n");
}

/* Test large identical files. */
static void test_large_identical_files(void)
{
    write_pattern_file("t8_a.bin", 200000, "The quick brown fox jumps over the lazy dog. ");
    write_pattern_file("t8_b.bin", 200000, "The quick brown fox jumps over the lazy dog. ");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t8_a.bin") == 1);
    assert(FDCheck(fd, "t8_b.bin") == 1);

    expect_one_group(fd, "t8_a.bin", "t8_b.bin");

    cleanup("t8_a.bin", "t8_b.bin", NULL, NULL, NULL);
    printf("test_large_identical_files passed\n");
}

/* Test large files with same size but different content. */
static void test_large_different_files(void)
{
    write_pattern_file("t8_c.bin", 200000, "The quick brown fox jumps over the lazy dog. ");
    write_pattern_file("t8_d.bin", 200000, "the quick brown dog jumps over the lazy fox. ");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t8_c.bin") == 1);
    assert(FDCheck(fd, "t8_d.bin") == 1);

    expect_no_duplicates(fd);

    cleanup("t8_c.bin", "t8_d.bin", NULL, NULL, NULL);
    printf("test_large_different_files passed\n");
}

/* Test the pending case with small files. */
static void test_pending_case(void)
{
    write_file("t9_a.txt", "123456");
    write_file("t9_b.txt", "123457");
    write_file("t9_c.txt", "123456");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t9_a.txt") == 1);
    assert(FDCheck(fd, "t9_b.txt") == 1);
    assert(FDCheck(fd, "t9_c.txt") == 1);

    expect_one_group(fd, "t9_a.txt", "t9_c.txt");

    cleanup("t9_a.txt", "t9_b.txt", "t9_c.txt", NULL, NULL);
    printf("test_pending_case passed\n");
}

/* Test the pending case with large files. */
static void test_pending_case_large_files(void)
{
    write_pattern_file("t9_d.bin", 100000, "pattern");
    write_pattern_file("t9_e.bin", 100000, "pattern1");
    write_pattern_file("t9_f.bin", 100000, "pattern");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t9_d.bin") == 1);
    assert(FDCheck(fd, "t9_e.bin") == 1);
    assert(FDCheck(fd, "t9_f.bin") == 1);

    expect_one_group(fd, "t9_d.bin", "t9_f.bin");

    cleanup("t9_d.bin", "t9_e.bin", "t9_f.bin", NULL, NULL);
    printf("test_pending_case_large_files passed\n");
}

/* Test two separate duplicate groups. */
static void test_two_duplicate_groups(void)
{
    write_file("t10_a1.txt", "alpha");
    write_file("t10_a2.txt", "alpha");
    write_file("t10_b1.txt", "beta beta");
    write_file("t10_b2.txt", "beta beta");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t10_a1.txt") == 1);
    assert(FDCheck(fd, "t10_a2.txt") == 1);
    assert(FDCheck(fd, "t10_b1.txt") == 1);
    assert(FDCheck(fd, "t10_b2.txt") == 1);

    expect_two_groups(fd, "t10_a1.txt", "t10_a2.txt", "t10_b1.txt", "t10_b2.txt");

    cleanup("t10_a1.txt", "t10_a2.txt", "t10_b1.txt", "t10_b2.txt", NULL);
    printf("test_two_duplicate_groups passed\n");
}

/* Test two separate duplicate groups with large files. */
static void test_two_duplicate_groups_large_files(void)
{
    write_pattern_file("t10_c1.bin", 100000, "gamma");
    write_pattern_file("t10_c2.bin", 100000, "gamma");
    write_pattern_file("t10_d1.bin", 100000, "delta delta");
    write_pattern_file("t10_d2.bin", 100000, "delta delta");

    FILEDEDUP fd = FDInit();
    assert(fd != NULL);

    assert(FDCheck(fd, "t10_c1.bin") == 1);
    assert(FDCheck(fd, "t10_c2.bin") == 1);
    assert(FDCheck(fd, "t10_d1.bin") == 1);
    assert(FDCheck(fd, "t10_d2.bin") == 1);

    expect_two_groups(fd, "t10_c1.bin", "t10_c2.bin", "t10_d1.bin", "t10_d2.bin");

    cleanup("t10_c1.bin", "t10_c2.bin", "t10_d1.bin", "t10_d2.bin", NULL);
    printf("test_two_duplicate_groups_large_files passed\n");
}

/* Return current time in milliseconds. */
static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/* Count how many duplicate groups are present in a dump. */
static int count_groups(char **dump, int length)
{
    int groups = 0;
    for (int i = 0; i < length; i++)
    {
        if (dump[i] == NULL)
            groups++;
    }
    return groups;
}

/* Run a test in an isolated process to prevent side effects. */
static int run_test_isolated(const TestCase *test)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return 0;
    }

    if (pid == 0)
    {
        test->func();
        exit(EXIT_SUCCESS);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        perror("waitpid");
        return 0;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return 1;

    return 0;
}

/* Benchmark 1: increasing file size, fixed number of files. */
static void benchmark_size_scaling(void)
{
    int sizes[] = {1000, 10000, 100000, 500000, 1000000};
    int n = (int)(sizeof(sizes) / sizeof(sizes[0]));

    printf("\n=== BENCHMARK: SIZE SCALABILITY ===\n");
    printf("%-12s %-12s %-12s\n", "Size(bytes)", "Time(ms)", "MB/s");

    for (int i = 0; i < n; i++)
    {
        char a[64], b[64];
        snprintf(a, sizeof(a), "bench_size_a_%d.bin", sizes[i]);
        snprintf(b, sizeof(b), "bench_size_b_%d.bin", sizes[i]);

        write_pattern_file(a, sizes[i], "benchmark-pattern");
        write_pattern_file(b, sizes[i], "benchmark-pattern");

        double start = now_ms();

        FILEDEDUP fd = FDInit();
        assert(fd != NULL);

        assert(FDCheck(fd, a) == 1);
        assert(FDCheck(fd, b) == 1);

        int len = 0;
        char **dump = FDDump(fd, &len);

        double end = now_ms();

        assert(dump != NULL);
        assert(count_groups(dump, len) == 1);

        double ms = end - start;
        double mb = (2.0 * sizes[i]) / (1024.0 * 1024.0);
        double mbps = mb / (ms / 1000.0);

        printf("%-12d %-12.2f %-12.2f\n", sizes[i], ms, mbps);

        free_dump(dump, len);
        remove(a);
        remove(b);
    }
}

/* Benchmark 2: increasing number of files. */
static void benchmark_file_count(void)
{
    int counts[] = {10, 50, 100, 200};
    int ncases = (int)(sizeof(counts) / sizeof(counts[0]));

    printf("\n=== BENCHMARK: LOAD BY NUMBER OF FILES ===\n");
    printf("%-12s %-12s %-12s\n", "Files", "Time(ms)", "ms/file");

    for (int c = 0; c < ncases; c++)
    {
        int n = counts[c];
        char **names = malloc((size_t)n * sizeof(char *));
        assert(names != NULL);

        double start = now_ms();

        FILEDEDUP fd = FDInit();
        assert(fd != NULL);

        for (int i = 0; i < n; i++)
        {
            names[i] = malloc(64);
            assert(names[i] != NULL);

            snprintf(names[i], 64, "bench_load_%d_%d.bin", n, i);

            if (i % 2 == 0)
                write_pattern_file(names[i], 20000, "duplicate");
            else
                write_pattern_file(names[i], 20000 + i, "unique");

            assert(FDCheck(fd, names[i]) == 1);
        }

        int len = 0;
        char **dump = FDDump(fd, &len);

        double end = now_ms();

        assert(dump != NULL);
        assert(count_groups(dump, len) >= 1);

        double ms = end - start;
        printf("%-12d %-12.2f %-12.3f\n", n, ms, ms / n);

        free_dump(dump, len);

        for (int i = 0; i < n; i++)
        {
            remove(names[i]);
            free(names[i]);
        }
        free(names);
    }
}

/* Benchmark 3: repeated runs for stability. */
static void benchmark_stability(void)
{
    const int iterations = 50;

    write_pattern_file("bench_stability_a.bin", 50000, "stable-pattern");
    write_pattern_file("bench_stability_b.bin", 50000, "stable-pattern");

    double total = 0.0;
    double min = -1.0;
    double max = 0.0;

    printf("\n=== BENCHMARK: STABILITY ===\n");

    for (int i = 0; i < iterations; i++)
    {
        double start = now_ms();

        FILEDEDUP fd = FDInit();
        assert(fd != NULL);

        assert(FDCheck(fd, "bench_stability_a.bin") == 1);
        assert(FDCheck(fd, "bench_stability_b.bin") == 1);

        int len = 0;
        char **dump = FDDump(fd, &len);

        double end = now_ms();
        double ms = end - start;

        assert(dump != NULL);
        assert(count_groups(dump, len) == 1);

        free_dump(dump, len);

        total += ms;
        if (min < 0 || ms < min)
            min = ms;
        if (ms > max)
            max = ms;
    }

    printf("Iterations : %d\n", iterations);
    printf("Min time   : %.2f ms\n", min);
    printf("Max time   : %.2f ms\n", max);
    printf("Avg time   : %.2f ms\n", total / iterations);

    remove("bench_stability_a.bin");
    remove("bench_stability_b.bin");
}

static void print_separator(void)
{
    printf("+--------------------------------------------------+----------+\n");
}

static int run_all_functional_tests(void)
{
    TestCase tests[] = {
        {"test_null_arguments", test_null_arguments},
        {"test_missing_file", test_missing_file},
        {"test_one_file_only", test_one_file_only},
        {"test_two_identical_files", test_two_identical_files},
        {"test_two_identical_large_files", test_two_identical_large_files},
        {"test_two_different_files", test_two_different_files},
        {"test_two_different_large_files", test_two_different_large_files},
        {"test_different_size_same_content", test_different_size_same_content},
        {"test_same_size_different_content", test_same_size_different_content},
        {"test_three_identical_files", test_three_identical_files},
        {"test_empty_files", test_empty_files},
        {"test_large_identical_files", test_large_identical_files},
        {"test_large_different_files", test_large_different_files},
        {"test_pending_case", test_pending_case},
        {"test_pending_case_large_files", test_pending_case_large_files},
        {"test_two_duplicate_groups", test_two_duplicate_groups},
        {"test_two_duplicate_groups_large_files", test_two_duplicate_groups_large_files}
    };

    int total = (int)(sizeof(tests) / sizeof(tests[0]));
    int *results = malloc((size_t)total * sizeof(int));
    assert(results != NULL);

    int passed = 0;

    for (int i = 0; i < total; i++)
    {
        results[i] = run_test_isolated(&tests[i]);
        if (results[i])
            passed++;
    }

    printf("\nTEST RESULTS\n");
    print_separator();
    printf("| %-48s | %-8s |\n", "Test", "Status");
    print_separator();

    for (int i = 0; i < total; i++)
    {
        printf("| %-48s | %-8s |\n",
               tests[i].name,
               results[i] ? "PASS" : "FAIL");
    }

    print_separator();
    printf("| %-48s | %2d/%-5d |\n", "TOTAL", passed, total);
    print_separator();
    printf("\n");

    free(results);

    return passed == total;
}

static void run_all_benchmarks(void)
{
    benchmark_size_scaling();
    benchmark_file_count();
    benchmark_stability();

    printf("\nAll benchmarks completed successfully.\n");
}

int main(void)
{
    int tests_ok = run_all_functional_tests();

    run_all_benchmarks();

    return tests_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}