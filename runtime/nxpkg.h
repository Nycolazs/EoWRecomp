// The player's game package (NSP / XCI / program NCA) read in place (runtime/nxpkg.c): ExeFS extraction and the
// RomFS served straight from the package - decrypted (AES-CTR) and decompressed (BucketTree / LZ4, the layer
// hactool can't read) on the fly, so no 8 GB romfs.bin is written. The same layout as tools/nca_extract.py.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Setup: finds the program NCA in `package` (keys: the player's prod.keys; title.keys next to it or a ticket in the
// NSP for titlekey-encrypted NCAs), writes its ExeFS files into <out_dir>/exefs/ and the RomFS description
// <out_dir>/romfs.src (the package's path, where the RomFS section is, its key: what nx_romfs_open needs).
// title_id: the program's, out (may be NULL). false = failed (why: a short message for the player).
bool nx_setup(const char *package, const char *keys, const char *out_dir, uint64_t *title_id, char *why, size_t why_size);

typedef struct NxRomfs NxRomfs;
// From a romfs.src written by nx_setup. NULL = missing / the package moved or changed (why).
NxRomfs *nx_romfs_open(const char *src_path, char *why, size_t why_size);
uint64_t nx_romfs_size(const NxRomfs *r);
// pread of the RomFS image; thread-safe. Returns size, or -1 (an I/O error / corrupt package).
ssize_t nx_romfs_pread(NxRomfs *r, void *dst, uint64_t size, uint64_t off);
void nx_romfs_close(NxRomfs *r);

// (tests) the AES / LZ4 primitives
void nx_aes128_ecb_decrypt(const uint8_t key[16], uint8_t *data, size_t len);
void nx_aes128_ctr(const uint8_t key[16], const uint8_t iv[16], uint8_t *data, size_t len);
int nx_lz4_decompress(const uint8_t *src, int src_len, uint8_t *dst, int dst_len);

#ifdef __cplusplus
}
#endif
