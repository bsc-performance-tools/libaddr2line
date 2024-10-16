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

int nm_dump_symtab(char *binary_path, symtab_t **symtab);
char * nm_get_symbol_name(symtab_t *symtab, unsigned long addr);
void symtab_free(symtab_t *symtab);