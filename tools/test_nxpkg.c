// Tests for runtime/nxpkg.c: AES (FIPS-197, SP 800-38A CTR, IEEE 1619 XTS vectors), LZ4 (blocks made by the lz4
// Python module), and with a package: the program NCA's header, keys and first RomFS bytes (compare with
// tools/nca_extract.py). Build and run: tools/test_nxpkg.sh [prod.keys package]
#include "../runtime/nxpkg.c"

static int fails;
static void unhex_s(const char *s, uint8_t *o) { unhex(s, o, 512); }
static void check(const char *what, const uint8_t *got, const char *want_hex) {
    uint8_t w[256];
    size_t n = strlen(want_hex) / 2;
    unhex_s(want_hex, w);
    bool ok = !memcmp(got, w, n);
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}

int main(int argc, char **argv) {
    uint8_t key[32], b[512];
    unhex_s("000102030405060708090a0b0c0d0e0f", key);
    unhex_s("00112233445566778899aabbccddeeff", b);
    uint8_t rk[176];
    aes_expand(key, rk);
    aes_encrypt_block(rk, b);
    check("aes encrypt (FIPS-197 C.1)", b, "69c4e0d86a7b0430d8cdb78070b4c55a");
    nx_aes128_ecb_decrypt(key, b, 16);
    check("aes decrypt", b, "00112233445566778899aabbccddeeff");

    uint8_t iv[16];
    unhex_s("2b7e151628aed2a6abf7158809cf4f3c", key);
    unhex_s("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff", iv);
    unhex_s("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52ef", b);
    nx_aes128_ctr(key, iv, b, 45);  // (a partial last block)
    check("aes ctr (SP 800-38A F.5.1)", b, "874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff5ae4df3edbd5d35e5b4f0902");

    memset(key, 0, 32);
    memset(b, 0, sizeof b);
    unhex_s("917cf69ebd68b2ec9b9fe9a3eadda692cd43d2f59598ed858c02c2652fbf922e", b);
    aes_xts_decrypt(key, b, 0x200, 0);
    check("aes xts (IEEE 1619 vector 1)", b, "0000000000000000000000000000000000000000000000000000000000000000");

    for (int i = 1; i + 1 < argc && !strcmp(argv[i], "--lz4"); i += 2) {  // --lz4 <file>: [u32 raw size][raw][block]
        FILE *f = fopen(argv[i + 1], "rb");
        uint32_t rn, cn;
        fread(&rn, 4, 1, f), fread(&cn, 4, 1, f);
        uint8_t *raw = malloc(rn), *comp = malloc(cn), *out = malloc(rn);
        fread(raw, 1, rn, f), fread(comp, 1, cn, f);
        fclose(f);
        int n = nx_lz4_decompress(comp, (int)cn, out, (int)rn);
        bool ok = n == (int)rn && !memcmp(raw, out, rn);
        printf("%s lz4 %s (%u -> %u)\n", ok ? "ok  " : "FAIL", argv[i + 1], cn, rn);
        fails += !ok;
        free(raw), free(comp), free(out);
    }

    if (argc >= 3 && strcmp(argv[1], "--lz4")) {  // <prod.keys> <package>: header, key, first RomFS bytes
        Keys ks = {0};
        keys_load(argv[1], &ks);
        int fd = open(argv[2], O_RDONLY);
        Entries files = {0};
        read_fs0(fd, 0, &files);
        for (int i = 0; i < files.n; i++) {
            Nca nca;
            if (!nca_header(fd, files.e[i].off, &ks, &nca)) { printf("     %s: not decrypted\n", files.e[i].name); continue; }
            uint64_t tid;
            memcpy(&tid, nca.hdr + 0x210, 8);
            printf("     %s: NCA3 type %d title %016llx\n", files.e[i].name, nca.hdr[0x205], (unsigned long long)tid);
            if (nca.hdr[0x205] != 0) continue;
            int gen = nca.hdr[0x206] > nca.hdr[0x220] ? nca.hdr[0x206] : nca.hdr[0x220];
            char kn[48];
            snprintf(kn, sizeof kn, "key_area_key_application_%02x", gen ? gen - 1 : 0);
            uint8_t area[0x40];
            memcpy(area, nca.hdr + 0x300, 0x40);
            nx_aes128_ecb_decrypt(key_get(&ks, kn, 16), area, 0x40);
            Section s;
            section_info(&nca, 1, &s);
            uint8_t first[32];
            section_read(fd, nca.base, &s, area + 0x20, first, s.data_off, 32);
            printf("     romfs section at %llx, data %llx, first bytes: ", (unsigned long long)s.off, (unsigned long long)s.data_off);
            for (int k = 0; k < 32; k++) printf("%02x", first[k]);
            printf("\n");
        }
    }
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
