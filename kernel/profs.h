// ProFS - Minimal RAM-backed filesystem with optional commit/version support
// MVP for ProOS: data structures and API surface

#ifndef PROFS_H
#define PROFS_H

#include <stdint.h>

#define PROFS_MAX_INODES 128
#define PROFS_MAX_NAME 64
#define PROFS_MAX_BLOBS 512
#define PROFS_MAX_COMMITS 256
#define PROFS_MASTER_KEY_LEN 32

typedef enum {
    PROFS_OBJ_BLOB = 1,
    PROFS_OBJ_TREE = 2,
    PROFS_OBJ_COMMIT = 3,
} profs_obj_type_t;

// Simple object id (hash) - use SHA256 length (32 bytes)
typedef struct {
    uint8_t hash[32];
} profs_oid_t;

typedef struct {
    uint32_t inode_num;
    char name[PROFS_MAX_NAME];
    uint32_t parent; // inode index of parent (self for root)
    uint8_t is_dir;
    uint8_t is_locked;
    uint8_t tracked; // commit tracking state
    uint32_t size;
    profs_oid_t oid; // points to latest blob if file
    // Security: owner id and simple password hash (sha256)
    uint32_t owner_uid;
    uint8_t password_hash[32]; // zero if no password
} profs_inode_t;

// Blob stored in memory
typedef struct {
    profs_oid_t id;
    uint8_t *data;
    uint32_t size;
    uint8_t encrypted; // if set, data is encrypted
} profs_blob_t;

// Commit metadata
typedef struct {
    profs_oid_t id;
    profs_oid_t tree_oid;
    uint8_t parent_commit[32];
    uint64_t timestamp;
    char message[128];
} profs_commit_t;

// Public API (MVP)
void profs_init(const uint8_t *master_key, uint32_t key_len);
int profs_create_file(const char *path, uint32_t owner_uid);
int profs_write_file(const char *path, const uint8_t *data, uint32_t size, const uint8_t *password);
int profs_read_file(const char *path, uint8_t *out, uint32_t maxlen, const uint8_t *password);
int profs_commit_file(const char *path, const char *message, uint8_t out_hash[32]);
int profs_history(const char *path, char *out_buf, uint32_t buf_len);
int profs_revert(const char *path, const uint8_t commit_hash[32]);
int profs_lock(const char *path);
int profs_unlock(const char *path);

// Utility
void profs_print_stats(void);

#endif // PROFS_H
