#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, const char **argv)
{
    assert(argc > 1);

    FILE *fp = fopen(argv[1], "w");
    assert(fp);

    for (int i = 2; i < argc; i++) {
        fprintf(fp, "const char ");

        const char *c = strrchr(argv[i], '/');
        assert(c);

        while (*++c) {
            if (*c == '.') {
                fputc('_', fp);
            } else {
                fputc(*c, fp);
            }
        }

        fprintf(fp, "[] = {\n");

        FILE *fp_1 = fopen(argv[i], "r");
        assert(fp_1);

        char *s = nullptr;
        size_t n = 0;
        int m;

        while((m = getline(&s, &n, fp_1)) != -1) {
            if (m < 2) {
                continue;
            }

            s[m - 1] = '\0';
            fprintf(fp, "    \"%s\\n\"\n", s);
        }

        free(s);
        fclose(fp_1);

        fprintf(fp, "};\n\n");
    }
}
