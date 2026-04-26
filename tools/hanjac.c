#include <stdio.h>

#include "../hangul/hangul.h"

int
main(int argc, char *argv[])
{
    HanjaTable* table;

    if (argc != 3) {
	fprintf(stderr, "usage: %s <source.txt> <output.bin>\n", argv[0]);
	return 1;
    }

    table = hanja_table_load(argv[1]);
    if (table == NULL) {
	fprintf(stderr, "failed to load hanja source: %s\n", argv[1]);
	return 1;
    }

    if (!hanja_table_save_binary(table, argv[2], argv[1])) {
	fprintf(stderr, "failed to save hanja binary cache: %s\n", argv[2]);
	hanja_table_delete(table);
	return 1;
    }

    hanja_table_delete(table);

    return 0;
}
