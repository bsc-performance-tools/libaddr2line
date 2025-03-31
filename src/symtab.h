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

symtab_t *symtab_read(char *binary_path);
char * symtab_find_symbol(symtab_t *symtab, unsigned long addr);
void symtab_free(symtab_t *symtab);

// Macros to iterate over the symtab_t structure
#define symtab_count(symtab) (symtab != NULL ? symtab->num_entries : 0)
#define symtab_get_entry(symtab, i) (symtab != NULL && i >= 0 && i < symtab->num_entries ? &symtab->entries[i] : NULL)  