// tools/test_msl.m <file.metal>...: compiles each MSL file with the Metal API (as MoltenVK does with what
// SPIRV-Cross makes of our SPIR-V), MSL 3.0; prints the failures. For tools/test_moltenvk.sh.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

int main(int argc, char **argv) {
    @autoreleasepool {
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        MTLCompileOptions *o = [MTLCompileOptions new];
        o.languageVersion = MTLLanguageVersion3_0;
        int bad = 0;
        for (int i = 1; i < argc; i++) {
            NSString *src = [NSString stringWithContentsOfFile:@(argv[i]) encoding:NSUTF8StringEncoding error:nil];
            NSError *err = nil;
            if (!src || ![dev newLibraryWithSource:src options:o error:&err]) {
                bad++;
                printf("FAIL %s\n%s\n", argv[i], err ? err.localizedDescription.UTF8String : "unreadable");
            }
        }
        printf("%d of %d compiled\n", argc - 1 - bad, argc - 1);
        return bad != 0;
    }
}
