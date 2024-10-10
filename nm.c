#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libelf.h>
#include <gelf.h>
#include <unistd.h>
#include <stdlib.h>
#include "nm.h"

#define SKIP_UNDEFINED_SYMBOLS // Define this to exclude undefined symbols from the symtab dump

int nm_symtab_dump(char *binary_path, symtab_t *symtab)
{
    Elf         *elf;
    Elf_Scn     *scn = NULL;
    GElf_Shdr   shdr;
    Elf_Data    *data;
    int         fd, i, count;

    elf_version(EV_CURRENT);

    fd = open(binary_path, O_RDONLY);
    elf = elf_begin(fd, ELF_C_READ, NULL);

    // Find the .symtab section
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        gelf_getshdr(scn, &shdr);
        if (shdr.sh_type == SHT_SYMTAB) {
            // Go print it
            break;
        }
    }

    data = elf_getdata(scn, NULL);
    count = shdr.sh_size / shdr.sh_entsize;

    int ii = 0; // Counter for the number of symbols that pass the exclusion filter
    symtab_entry_t *entries = malloc(count * sizeof(symtab_entry_t));
    for (i = 0; i < count; ++i) {
        GElf_Sym sym;
        gelf_getsym(data, i, &sym);
#if defined(SKIP_UNDEFINED_SYMBOLS) // Skips undefined symbols, forward declarations, weak symbols, etc.
        if ((sym.st_value) && (sym.st_size))
#endif
        {
            entries[ii].name = elf_strptr(elf, shdr.sh_link, sym.st_name);
            entries[ii].start = sym.st_value;
            entries[ii].size = sym.st_size;
            entries[ii].end = sym.st_value + sym.st_size;
            ii ++;
        }
    }

    /* Print the symbol names and addresses */
    for (i = 0; i < ii; ++i) {
        printf("%s [0x%lx-0x%lx]\n", entries[i].name, entries[i].start, entries[i].end + entries[i].size); 
    } 
    elf_end(elf);
    close(fd);

    symtab->entries = entries;
    symtab->num_entries = ii;
    return ii;
}

