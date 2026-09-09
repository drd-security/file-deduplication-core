
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/stat.h>

#include "filededup.h"

struct magic
{
    uint64_t T[ROWS][COLS];        // Precomputed table for hashing
    SizeBucket *sizes[SIZE_TABLE]; // Hash table for size buckets
    size_t total_files;
};

/**
 * @brief Structure representing a bucket of files with the same size. Each bucket contains groups of duplicates.
 */
struct SizeBucket
{
    uint64_t size;
    DupGroup *groups[GROUP_TABLE]; // Hash table for duplicate groups based on hash
    char *pending_path;            /* first file of this size, not hashed yet */
    SizeBucket *next;
};

/**
 * @brief Structure representing a group of duplicate files. Each group contains a hash, a representative file path, and a list of member file paths.
 */
struct DupGroup
{
    uint64_t hash;
    PathNode *members_head; // Head of the linked list of member file paths
    PathNode *members_tail; // Tail of the linked list for efficient appending
    size_t member_count;
    DupGroup *next;
};

/**
 * @brief Structure representing a node in the list of file paths within a duplicate group.
 */
struct PathNode
{
    char *filepath;
    PathNode *next;
};

/**
 * @brief Duplicates a string by allocating new memory and copying the contents.
 *
 * @param s The string to duplicate.
 * @return A pointer to the duplicated string, or NULL on failure.
 */
static char *dup_string(const char *s);

/**
 * @brief Generates a random 64-bit integer by combining multiple calls to rand().
 *
 * @return A random 64-bit integer.
 */
static uint64_t generate_rand64(void);

/**
 * @brief Creates a new SizeBucket for a given file size.
 *
 * @param size The file size for the bucket.
 * @return A pointer to the newly created SizeBucket, or NULL on failure.
 */
static SizeBucket *create_size_bucket(uint64_t size);

/**
 * @brief Creates a new PathNode for a given file path.
 *
 * @param filepath The file path to store in the node.
 * @return A pointer to the newly created PathNode, or NULL on failure.
 */
static PathNode *create_path_node(const char *filepath);

/**
 * @brief Creates a new DupGroup for a given hash and representative file path.
 *
 * @param hash The hash value representing the group of duplicates.
 * @param representative_path The file path of the representative file for the group.
 * @return A pointer to the newly created DupGroup, or NULL on failure.
 */
static DupGroup *create_dup_group(uint64_t hash, const char *representative_path);

/**
 * @brief Finds the SizeBucket corresponding to a given file size in the FILEDEDUP structure.
 *
 * @param fd The FILEDEDUP structure to search within.
 * @param size The file size to find the bucket for.
 * @return A pointer to the SizeBucket if found, or NULL if not found.
 */
static SizeBucket *find_size_bucket(FILEDEDUP fd, uint64_t size);

/**
 * @brief Adds a file path to a given DupGroup.
 *
 * @param group The DupGroup to add the file to.
 * @param filepath The file path to add to the group.
 * @return true if the file was successfully added, false on failure.
 */
static bool add_file_to_group(DupGroup *group, const char *filepath);

/**
 * @brief Computes the hash value for a given file path.
 *
 * @param fd The FILEDEDUP structure to use for computing the hash.
 * @param filepath The file path to compute the hash for.
 * @param out_hash A pointer to store the computed hash value.
 * @return true if the hash was successfully computed, false on failure.
 */
static bool compute_hash(FILEDEDUP fd, const char *filepath, uint64_t *out_hash);

/**
 * @brief Verifies if two files are identical by comparing their contents bytes by bytes.
 *
 * @param path1 The file path of the first file.
 * @param path2 The file path of the second file.
 * @return true if the files are identical, false otherwise.
 */
static bool verify_files(const char *path1, const char *path2);

/**
 * @brief Handles the pending file in a SizeBucket by computing its hash and creating a DupGroup for it.
 *
 * @param fd The FILEDEDUP structure to use for processing the pending file.
 * @param bucket The SizeBucket containing the pending file.
 * @return true if the pending file was successfully processed, false on failure.
 */
static bool pending_file(FILEDEDUP fd, SizeBucket *bucket);

static char *dup_string(const char *s)
{
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);

    if (copy == NULL)
    {
        fprintf(stderr, "Failed to duplicate string: %s\n", strerror(errno));
        return NULL;
    }

    memcpy(copy, s, len);
    return copy;
}

static uint64_t generate_rand64(void)
{
    uint64_t x = 0;

    for (int i = 0; i < 4; i++)
    {
        x <<= 16;
        x |= (uint64_t)(rand() & 0xFFFF);
    }

    return x;
}

static SizeBucket *create_size_bucket(uint64_t size)
{
    SizeBucket *bucket = malloc(sizeof(SizeBucket));
    if (bucket == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for SizeBucket: %s\n", strerror(errno));
        return NULL;
    }
    bucket->size = size;
    memset(bucket->groups, 0, sizeof(bucket->groups)); // Initialize group pointers to NULL
    bucket->pending_path = NULL;
    bucket->next = NULL;
    return bucket;
}

static PathNode *create_path_node(const char *filepath)
{
    PathNode *node = malloc(sizeof(PathNode));
    if (node == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for PathNode: %s\n", strerror(errno));
        return NULL;
    }
    node->filepath = dup_string(filepath); // Duplicate the file path string
    if (node->filepath == NULL)
    {
        fprintf(stderr, "Failed to duplicate string for PathNode: %s\n", strerror(errno));
        free(node);
        return NULL;
    }
    node->next = NULL;
    return node;
}

static DupGroup *create_dup_group(uint64_t hash, const char *representative_path)
{
    DupGroup *group = malloc(sizeof(DupGroup));
    if (group == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for DupGroup: %s\n", strerror(errno));
        return NULL;
    }
    group->hash = hash;
    group->members_head = NULL;
    group->members_tail = NULL;
    group->members_head = create_path_node(representative_path);
    if (group->members_head == NULL)
    {
        fprintf(stderr, "Failed to create path node for DupGroup: %s\n", strerror(errno));
        free(group);
        return NULL;
    }
    group->members_tail = group->members_head;
    group->member_count = 1;
    group->next = NULL;
    return group;
}

static SizeBucket *find_size_bucket(FILEDEDUP fd, uint64_t size)
{
    size_t index = size % SIZE_TABLE;
    SizeBucket *current = fd->sizes[index];

    while (current != NULL)
    {
        if (current->size == size)
        {
            return current; // Found the bucket for this size
        }
        current = current->next;
    }

    return NULL; // No bucket found for this size
}

static bool add_file_to_group(DupGroup *group, const char *filepath)
{
    PathNode *new_member = create_path_node(filepath);
    if (new_member == NULL)
    {
        return false; // Failed to create a new path node
    }

    // Add the new member to the end of the members list
    group->members_tail->next = new_member;
    group->members_tail = new_member;
    group->member_count++;
    return true;
}

static bool compute_hash(FILEDEDUP fd, const char *filepath, uint64_t *out_hash)
{
    FILE *file = fopen(filepath, "rb");
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    uint64_t hash = 0;
    size_t row = 0;

    if (file == NULL)
    {
        fprintf(stderr, "Failed to open file '%s': %s\n", filepath, strerror(errno));
        return false;
    }

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0)
    {
        for (size_t i = 0; i < bytes_read; i++)
        {
            hash ^= fd->T[row][buffer[i]];
            if (++row == ROWS)
            {
                row = 0;
            }
        }
    }

    if (ferror(file))
    {
        fclose(file);
        return false;
    }

    fclose(file);
    *out_hash = hash;
    return true;
}

static bool verify_files(const char *path1, const char *path2)
{
    FILE *f1 = fopen(path1, "rb");
    if (f1 == NULL)
    {
        fprintf(stderr, "Failed to open file '%s' for verification: %s\n", path1, strerror(errno));
        return false;
    }

    FILE *f2 = fopen(path2, "rb");
    if (f2 == NULL)
    {
        fprintf(stderr, "Failed to open file '%s' for verification: %s\n", path2, strerror(errno));
        fclose(f1);
        return false;
    }

    unsigned char buffer1[BUFFER_SIZE];
    unsigned char buffer2[BUFFER_SIZE];

    while (1)
    {
        size_t bytes_read1 = fread(buffer1, 1, BUFFER_SIZE, f1);
        size_t bytes_read2 = fread(buffer2, 1, BUFFER_SIZE, f2);

        if (bytes_read1 != bytes_read2)
        {
            fclose(f1);
            fclose(f2);
            return false;
        }

        if (memcmp(buffer1, buffer2, bytes_read1) != 0) // Compare the buffers
        {
            fclose(f1);
            fclose(f2);
            return false;
        }

        if (ferror(f1) || ferror(f2)) // Check for read errors
        {
            fclose(f1);
            fclose(f2);
            return false;
        }

        if (bytes_read1 == 0)
        {
            break;
        }
    }

    fclose(f1);
    fclose(f2);
    return true; // Files are identical
}

static bool pending_file(FILEDEDUP fd, SizeBucket *bucket)
{
    if (bucket == NULL || bucket->pending_path == NULL)
    {
        return true;
    }

    uint64_t pending_hash;
    if (!compute_hash(fd, bucket->pending_path, &pending_hash))
    {
        return false;
    }

    DupGroup *group = create_dup_group(pending_hash, bucket->pending_path);
    if (group == NULL)
    {
        return false;
    }

    size_t group_index = pending_hash % GROUP_TABLE;
    group->next = bucket->groups[group_index];
    bucket->groups[group_index] = group;

    free(bucket->pending_path);
    bucket->pending_path = NULL;
    return true;
}

FILEDEDUP FDInit()
{
    FILEDEDUP fd = calloc(1, sizeof(struct magic));

    if (fd == NULL)
    {
        fprintf(stderr, "Failed to initialize FILEDEDUP: %s\n", strerror(errno));
        return NULL;
    }

    srand(0); // Seed the random number generator

    for (size_t i = 0; i < ROWS; i++)
    {
        for (size_t j = 0; j < COLS; j++)
        {
            fd->T[i][j] = generate_rand64(); // Fill the precomputed table with random values
        }
    }

    return fd;
}

int FDCheck(FILEDEDUP fd, char *filepath)
{
    if (fd == NULL || filepath == NULL)
    {
        return 0;
    }

    struct stat st;
    if (stat(filepath, &st) != 0)
    {
        fprintf(stderr, "Failed to stat file '%s': %s\n", filepath, strerror(errno));
        return 0;
    }

    uint64_t size = (uint64_t)st.st_size; // Get the file size
    size_t index = size % SIZE_TABLE;

    SizeBucket *bucket = find_size_bucket(fd, size);

    /* First file of this size: store it, do not hash it yet */
    if (bucket == NULL)
    {
        bucket = create_size_bucket(size);
        if (bucket == NULL)
        {
            return 0;
        }

        bucket->pending_path = dup_string(filepath);
        if (bucket->pending_path == NULL)
        {
            free(bucket);
            return 0;
        }

        bucket->next = fd->sizes[index];
        fd->sizes[index] = bucket;

        fd->total_files++;
        return 1;
    }

    /* If the bucket still has one pending file, hash it now */
    if (bucket->pending_path != NULL)
    {
        if (!pending_file(fd, bucket))
        {
            return 0;
        }
    }

    /* Now hash the current file only because this size is known to matter */
    uint64_t hash;
    if (!compute_hash(fd, filepath, &hash))
    {
        return 0;
    }

    size_t group_index = hash % GROUP_TABLE;
    DupGroup *current = bucket->groups[group_index];

    while (current != NULL)
    {
        if (current->hash == hash)
        {
            if (verify_files(current->members_head->filepath, filepath))
            {
                if (!add_file_to_group(current, filepath))
                {
                    fprintf(stderr, "Failed to add file '%s' to duplicate group\n", filepath);
                    return 0;
                }

                fd->total_files++;
                return 1;
            }
        }

        current = current->next;
    }

    DupGroup *new_group = create_dup_group(hash, filepath);
    if (new_group == NULL)
    {
        return 0;
    }

    new_group->next = bucket->groups[group_index];
    bucket->groups[group_index] = new_group;

    fd->total_files++;
    return 1;
}

static void free_result_array(char **result, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        free(result[i]); /* free(NULL) is valid in C. */
    }
    free(result);
}

char **FDDump(FILEDEDUP fd, int *length)
{
    if (fd == NULL || length == NULL)
    {
        return NULL;
    }

    size_t count_slots = 0;
    int set_found = 0;

    /* Count only actual duplicate groups. */
    for (size_t i = 0; i < SIZE_TABLE; i++)
    {
        // Traverse each bucket of the size table
        for (SizeBucket *b = fd->sizes[i]; b != NULL; b = b->next)
        {
            for (size_t j = 0; j < GROUP_TABLE; j++)

                // for each bucket , traverse the groups of duplicates
                for (DupGroup *g = b->groups[j]; g != NULL; g = g->next)
                {
                    // check if the group has at least 2 members (i.e., it's a valid duplicate group)
                    if (g->member_count >= 2)
                    {
                        // add 1 for the NULL separator after the group + the number of members in the group
                        count_slots += g->member_count + 1;
                        set_found++;
                    }
                }
        }
    }
    // check if we found at least one set of duplicates, if not return an empty array
    if (set_found == 0)
    {
        *length = 0;
        return NULL;
    }

    char **result = malloc(count_slots * sizeof(char *));
    if (result == NULL)
    {
        *length = 0;
        return NULL;
    }

    size_t index = 0; // index to fill the result array

    for (size_t i = 0; i < SIZE_TABLE; i++)
    {
        for (SizeBucket *b = fd->sizes[i]; b != NULL; b = b->next)
        {
            for (size_t j = 0; j < GROUP_TABLE; j++)
            {
                for (DupGroup *g = b->groups[j]; g != NULL; g = g->next)
                {
                    // skip groups with less than 2 members, they are not duplicates
                    if (g->member_count < 2)
                    {
                        continue;
                    }

                    // for each valid duplicate group, copy the file paths to the result array
                    for (PathNode *p = g->members_head; p != NULL; p = p->next)
                    {
                        char *copy = dup_string(p->filepath);
                        if (copy == NULL)
                        {
                            free_result_array(result, index);
                            *length = 0;
                            return NULL;
                        }
                        // add the copied file path
                        result[index++] = copy;
                    }

                    // add a NULL separator after the group
                    result[index++] = NULL;
                }
            }
        }
    }
    *length = (int)index;
    return result;
}
