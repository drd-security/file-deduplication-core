#ifndef __FILEDEDUP_H__
#define __FILEDEDUP_H__

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define ROWS 32
#define COLS 256
#define BUFFER_SIZE (1 << 17) // 128 KB
#define SIZE_TABLE 1024
#define GROUP_TABLE 256

typedef struct magic *FILEDEDUP; // Opaque pointer type for the FILEDEDUP ADT

// Forward declarations of internal structures
typedef struct SizeBucket SizeBucket; // Structure representing a bucket of files with the same size
typedef struct DupGroup DupGroup;     // Structure representing a group of duplicate files
typedef struct PathNode PathNode;     // Structure representing a node in the list of file paths within a duplicate group

/**
 *
 * @brief Initializes the data structure where FILEDEDUP is a pointer type
 *        identifying the created instance of the FILEDEDUP ADT.
 * @return A pointer to the initialized file deduplication system.
 */
FILEDEDUP FDInit();

/**
 *
 * @brief Checks if the file specified by filepath is a duplicate of any previously checked file.
 * @param fd A pointer to the FILEDEDUP instance.
 * @param filepath The path to the file to be checked for duplication.
 * @return 1 if successful, 0 otherwise (e.g., if the file is not a duplicate or if an error occurs).
 */
int FDCheck(FILEDEDUP fd, char *filepath);

/**
 *
 * @brief Dumps the list of identical files.
 * @param fd A pointer to the FILEDEDUP instance.
 * @param length A pointer to an integer where the length of the returned array will be stored.
 * @return A pointer to an array of strings containing the paths of identical files.
 */
char **FDDump(FILEDEDUP fd, int *length);

#endif // __FILEDEDUP_H__