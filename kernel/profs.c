// Minimal ProFS implementation (RAM-backed MVP)
// Notes: Crypto is a simplified placeholder for the MVP. Replace with AES-256 and secure KDF.

#include "profs.h"
#include "memory.h"
#include "logging.h"
#include "event.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Simple SHA-256 implementation (minimal) - adapted for brevity
// For production, replace with a well-tested crypto library.
typedef struct {
    uint32_t state[8];
    uint64_t bitcount;
    uint8_t buffer[64];
} sha256_ctx;

// Forward-declare small helper functions (implementations below)
static void sha256_init(sha256_ctx *c);
static void sha256_update(sha256_ctx *c, const uint8_t *data, size_t len);
static void sha256_final(sha256_ctx *c, uint8_t digest[32]);

// Simple XOR-based "encryption" for MVP. Replace with AES-256.
static void simple_xor_encrypt(uint8_t *data, uint32_t len, const uint8_t key[32]) {
    for (uint32_t i = 0; i < len; i++) data[i] ^= key[i % 32];
}

// Small helpers to avoid libc dependency in kernel
static size_t my_strlen(const char *s) {
    size_t n = 0; if (!s) return 0; while (s[n]) n++; return n;
}

static void my_strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0; if (!dst) return; if (!src) { if (n) dst[0]=0; return; }
    for (i = 0; i < n - 1 && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = 0;
}

static int my_memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *pa = (const unsigned char*)a, *pb = (const unsigned char*)b;
    for (size_t i = 0; i < n; i++) if (pa[i] != pb[i]) return (int)pa[i] - (int)pb[i];
    return 0;
}

static void *my_memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dst; const unsigned char *s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

static void my_itoa(int v, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return; char buf[32]; int pos=0; int neg=0; unsigned int uv;
    if (v < 0) { neg = 1; uv = (unsigned int)(-v); } else uv = (unsigned int)v;
    if (uv == 0) { buf[pos++] = '0'; }
    while (uv > 0) { buf[pos++] = '0' + (uv % 10); uv /= 10; }
    int idx = 0; if (neg && idx < (int)out_sz-1) out[idx++] = '-';
    for (int i = pos-1; i >= 0 && idx < (int)out_sz-1; i--) out[idx++] = buf[i];
    out[idx] = '\0';
}

// Storage
static profs_inode_t inodes[PROFS_MAX_INODES];
static profs_blob_t blobs[PROFS_MAX_BLOBS];
static profs_commit_t commits[PROFS_MAX_COMMITS];
static uint32_t inode_count = 0;
static uint32_t blob_count = 0;
static uint32_t commit_count = 0;
static uint8_t master_key[PROFS_MASTER_KEY_LEN];

// Internal helpers
static profs_inode_t* profs_find_inode(const char *path) {
    if (!path || path[0] == '\0') return NULL;
    // only support single-level names or root-relative like /name for MVP
    const char *name = path;
    if (name[0] == '/') name++;
    for (uint32_t i = 0; i < PROFS_MAX_INODES; i++) {
        if (inodes[i].inode_num != 0 && inodes[i].is_dir == 0) {
            if (my_memcmp(inodes[i].name, name, PROFS_MAX_NAME) == 0) return &inodes[i];
        }
    }
    return NULL;
}

static profs_blob_t* profs_create_blob(const uint8_t *data, uint32_t size, const uint8_t key[32], int encrypt) {
    if (blob_count >= PROFS_MAX_BLOBS) return NULL;
    profs_blob_t *b = &blobs[blob_count++];
    b->data = (uint8_t*)malloc(size);
    if (!b->data) return NULL;
    my_memcpy(b->data, data, size);
    b->size = size;
    b->encrypted = encrypt ? 1 : 0;
    if (encrypt) simple_xor_encrypt(b->data, size, key);
    // compute id as sha256 of content
    sha256_ctx ctx; sha256_init(&ctx); sha256_update(&ctx, b->data, b->size); sha256_final(&ctx, b->id.hash);
    return b;
}

void profs_init(const uint8_t *master, uint32_t key_len) {
    memset(inodes, 0, sizeof(inodes));
    memset(blobs, 0, sizeof(blobs));
    memset(commits, 0, sizeof(commits));
    inode_count = 0; blob_count = 0; commit_count = 0;
    if (master && key_len >= PROFS_MASTER_KEY_LEN) my_memcpy(master_key, master, PROFS_MASTER_KEY_LEN);
    else memset(master_key, 0, PROFS_MASTER_KEY_LEN);
    // create root inode
    inodes[0].inode_num = 1;
    my_strncpy(inodes[0].name, "/", PROFS_MAX_NAME);
    inodes[0].parent = 0;
    inodes[0].is_dir = 1;
    inode_count = 1;
    log_info("ProFS: initialized (RAM mode)");
}

int profs_create_file(const char *path, uint32_t owner_uid) {
    if (!path) return -1;
    const char *name = path[0] == '/' ? &path[1] : path;
    // find free inode slot
    for (uint32_t i = 1; i < PROFS_MAX_INODES; i++) {
        if (inodes[i].inode_num == 0) {
            inodes[i].inode_num = i + 1;
            my_strncpy(inodes[i].name, name, PROFS_MAX_NAME);
            inodes[i].parent = 0;
            inodes[i].is_dir = 0;
            inodes[i].size = 0;
            inodes[i].owner_uid = owner_uid;
            memset(inodes[i].password_hash, 0, 32);
            return 0;
        }
    }
    return -1;
}

int profs_write_file(const char *path, const uint8_t *data, uint32_t size, const uint8_t *password) {
    profs_inode_t *ino = profs_find_inode(path);
    if (!ino) return -1;
    // password check if set
    if (ino->password_hash[0]) {
        uint8_t ph[32]; sha256_ctx c; sha256_init(&c); sha256_update(&c, password, my_strlen((const char*)password)); sha256_final(&c, ph);
        if (my_memcmp(ph, ino->password_hash, 32) != 0) return -1;
    }
    // derive per-file key from master + inode number
    uint8_t key[32]; sha256_ctx kctx; sha256_init(&kctx); sha256_update(&kctx, master_key, PROFS_MASTER_KEY_LEN); sha256_update(&kctx, (uint8_t*)&ino->inode_num, sizeof(ino->inode_num)); sha256_final(&kctx, key);
    profs_blob_t *b = profs_create_blob(data, size, key, 1);
    if (!b) return -1;
    ino->size = size;
    ino->oid = b->id;
    return 0;
}

int profs_read_file(const char *path, uint8_t *out, uint32_t maxlen, const uint8_t *password) {
    profs_inode_t *ino = profs_find_inode(path);
    if (!ino) return -1;
    if (ino->password_hash[0]) {
        uint8_t ph[32]; sha256_ctx c; sha256_init(&c); sha256_update(&c, password, my_strlen((const char*)password)); sha256_final(&c, ph);
        if (my_memcmp(ph, ino->password_hash, 32) != 0) return -1;
    }
    // find blob by oid
    for (uint32_t i = 0; i < blob_count; i++) {
        if (my_memcmp(&blobs[i].id, &ino->oid, sizeof(profs_oid_t)) == 0) {
            // decrypt into out (simple xor)
            uint8_t key[32]; sha256_ctx kctx; sha256_init(&kctx); sha256_update(&kctx, master_key, PROFS_MASTER_KEY_LEN); sha256_update(&kctx, (uint8_t*)&ino->inode_num, sizeof(ino->inode_num)); sha256_final(&kctx, key);
            uint32_t copy = (maxlen < blobs[i].size) ? maxlen : blobs[i].size;
            my_memcpy(out, blobs[i].data, copy);
            // decrypt in-place for output
            simple_xor_encrypt(out, copy, key);
            return copy;
        }
    }
    return -1;
}

int profs_commit_file(const char *path, const char *message, uint8_t out_hash[32]) {
    profs_inode_t *ino = profs_find_inode(path);
    if (!ino) return -1;
    // Create a commit object pointing to current blob
    if (commit_count >= PROFS_MAX_COMMITS) return -1;
    profs_commit_t *c = &commits[commit_count++];
    // tree_oid is simply inode->oid for MVP
    c->tree_oid = ino->oid;
    c->timestamp = 0; // timestamp not available in freestanding kernel MVP
    my_strncpy(c->message, message ? message : "", sizeof(c->message)-1);
    // compute id = sha256(tree_oid || timestamp || message)
    sha256_ctx ctx; sha256_init(&ctx); sha256_update(&ctx, c->tree_oid.hash, 32); sha256_update(&ctx, (uint8_t*)&c->timestamp, sizeof(c->timestamp)); sha256_update(&ctx, (uint8_t*)c->message, my_strlen(c->message)); sha256_final(&ctx, c->id.hash);
    if (out_hash) my_memcpy(out_hash, c->id.hash, 32);
    return 0;
}

int profs_history(const char *path, char *out_buf, uint32_t buf_len) {
    profs_inode_t *ino = profs_find_inode(path);
    if (!ino) return -1;
    // iterate commits and find those that reference this inode oid
    uint32_t pos = 0;
    for (uint32_t i = 0; i < commit_count; i++) {
        if (my_memcmp(commits[i].tree_oid.hash, ino->oid.hash, 32) == 0) {
            // format: commit XX.. msg: MESSAGE\n  (very small formatter)
            if (pos + 20 >= buf_len) break;
            const char hex[] = "0123456789abcdef";
            out_buf[pos++] = 'c'; out_buf[pos++] = 'o'; out_buf[pos++] = 'm'; out_buf[pos++] = 'm'; out_buf[pos++] = 'i'; out_buf[pos++] = 't'; out_buf[pos++] = ' ';
            out_buf[pos++] = hex[(commits[i].id.hash[0] >> 4) & 0xF];
            out_buf[pos++] = hex[commits[i].id.hash[0] & 0xF];
            out_buf[pos++] = '.'; out_buf[pos++] = '.'; out_buf[pos++] = ' ';
            out_buf[pos++] = 'm'; out_buf[pos++] = 's'; out_buf[pos++] = 'g'; out_buf[pos++] = ':'; out_buf[pos++] = ' ';
            // copy message
            for (size_t m = 0; m < my_strlen(commits[i].message) && pos + 1 < buf_len; m++) out_buf[pos++] = commits[i].message[m];
            if (pos < buf_len) out_buf[pos++] = '\n';
            if (pos >= buf_len) break;
        }
    }
    return 0;
}

int profs_revert(const char *path, const uint8_t commit_hash[32]) {
    profs_inode_t *ino = profs_find_inode(path);
    if (!ino) return -1;
    // find commit matching hash and set inode->oid to its tree
    for (uint32_t i = 0; i < commit_count; i++) {
        if (my_memcmp(commits[i].id.hash, commit_hash, 32) == 0) {
            ino->oid = commits[i].tree_oid;
            return 0;
        }
    }
    return -1;
}

int profs_lock(const char *path) {
    profs_inode_t *ino = profs_find_inode(path);
    if (!ino) return -1;
    ino->is_locked = 1;
    return 0;
}

int profs_unlock(const char *path) {
    profs_inode_t *ino = profs_find_inode(path);
    if (!ino) return -1;
    ino->is_locked = 0;
    return 0;
}

void profs_print_stats(void) {
    char buf[128];
    // build a simple stats string without snprintf
    const char *prefix = "ProFS: inodes=";
    my_memcpy(buf, prefix, my_strlen(prefix));
    size_t p = my_strlen(prefix);
    char tmp[32]; my_itoa(PROFS_MAX_INODES, tmp, sizeof(tmp));
    my_memcpy(buf + p, tmp, my_strlen(tmp)); p += my_strlen(tmp);
    const char *m1 = " blobs="; my_memcpy(buf + p, m1, my_strlen(m1)); p += my_strlen(m1);
    my_itoa((int)blob_count, tmp, sizeof(tmp)); my_memcpy(buf + p, tmp, my_strlen(tmp)); p += my_strlen(tmp);
    const char *m2 = " commits="; my_memcpy(buf + p, m2, my_strlen(m2)); p += my_strlen(m2);
    my_itoa((int)commit_count, tmp, sizeof(tmp)); my_memcpy(buf + p, tmp, my_strlen(tmp)); p += my_strlen(tmp);
    if (p < sizeof(buf)) buf[p] = '\0'; else buf[sizeof(buf)-1] = '\0';
    log_info(buf);
}

// --- Minimal SHA256 implementation ---
// The implementation below is intentionally compact and not fully optimized.
// For clarity, many constants are inlined.

static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

static void sha256_transform(sha256_ctx *c, const uint8_t *data) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = (data[i*4]<<24) | (data[i*4+1]<<16) | (data[i*4+2]<<8) | (data[i*4+3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i-15],7) ^ rotr(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr(w[i-2],17) ^ rotr(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a = c->state[0], b = c->state[1], d = c->state[3];
    uint32_t e = c->state[4], f = c->state[5], g = c->state[6], h = c->state[7];
    // Note: use correct mapping
    a = c->state[0]; b = c->state[1]; d = c->state[3];
    uint32_t c2 = c->state[2];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
        uint32_t maj = (a & b) ^ (a & c2) ^ (b & c2);
        uint32_t temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1; d = c2; c2 = b; b = a; a = temp1 + temp2;
    }
    c->state[0] += a; c->state[1] += b; c->state[2] += c2; c->state[3] += d;
    c->state[4] += e; c->state[5] += f; c->state[6] += g; c->state[7] += h;
}

static void sha256_init(sha256_ctx *c) {
    c->state[0]=0x6a09e667; c->state[1]=0xbb67ae85; c->state[2]=0x3c6ef372; c->state[3]=0xa54ff53a;
    c->state[4]=0x510e527f; c->state[5]=0x9b05688c; c->state[6]=0x1f83d9ab; c->state[7]=0x5be0cd19;
    c->bitcount = 0;
}

static void sha256_update(sha256_ctx *c, const uint8_t *data, size_t len) {
    // simple (not streaming optimized)
    uint8_t block[64];
    size_t i = 0;
    while (i + 64 <= len) { sha256_transform(c, &data[i]); i += 64; }
    size_t rem = len - i;
    if (rem) {
        memset(block, 0, 64); my_memcpy(block, &data[i], rem);
        // pad per standard
        block[rem] = 0x80;
        // if room for length
        if (rem <= 55) {
            uint64_t bits = (c->bitcount + len*8);
            block[56] = (bits >> 56) & 0xff; block[57] = (bits >> 48) & 0xff; block[58] = (bits >> 40) & 0xff; block[59] = (bits >> 32) & 0xff;
            block[60] = (bits >> 24) & 0xff; block[61] = (bits >> 16) & 0xff; block[62] = (bits >> 8) & 0xff; block[63] = (bits) & 0xff;
            sha256_transform(c, block);
        } else {
            // two block padding - simplified
            uint64_t bits = (c->bitcount + len*8);
            uint8_t blk2[64]; memset(blk2,0,64);
            blk2[56] = (bits >> 56) & 0xff; blk2[57] = (bits >> 48) & 0xff; blk2[58] = (bits >> 40) & 0xff; blk2[59] = (bits >> 32) & 0xff;
            blk2[60] = (bits >> 24) & 0xff; blk2[61] = (bits >> 16) & 0xff; blk2[62] = (bits >> 8) & 0xff; blk2[63] = (bits) & 0xff;
            sha256_transform(c, block);
            sha256_transform(c, blk2);
        }
    }
    c->bitcount += len * 8;
}

static void sha256_final(sha256_ctx *c, uint8_t digest[32]) {
    // Not a perfect streaming final but OK for this MVP where inputs are small
    uint8_t tmp[64]; memset(tmp,0,64);
    // encode state
    for (int i = 0; i < 8; i++) {
        digest[i*4] = (c->state[i] >> 24) & 0xff;
        digest[i*4+1] = (c->state[i] >> 16) & 0xff;
        digest[i*4+2] = (c->state[i] >> 8) & 0xff;
        digest[i*4+3] = (c->state[i]) & 0xff;
    }
}
