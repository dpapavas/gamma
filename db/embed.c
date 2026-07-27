// Copyright 2026 Dimitris Papavasiliou

// This file is part of Gamma.

// Gamma is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.

// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.

// You should have received a copy of the GNU General Public License along with
// this program. If not, see <https://www.gnu.org/licenses/>.

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
