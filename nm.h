#pragma once

typedef struct symtab_entry {
    char *name;
    unsigned long start;
    unsigned int size;
    unsigned long end;
} symtab_entry_t;

typedef struct symtab {
    symtab_entry_t *entries;
    int num_entries;
} symtab_t;

int nm_symtab_dump(char *binary_path, symtab_t *symtab);