#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libelf.h>
#include <gelf.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "nm.h"

#warning "Makefile.am has a fixed path... change it to ELFUTILS_LIBSDIR or similar"

#warning "Rename library to libsymtab... it collides with a system's libnm"

#define FILTER_DATA_OBJECTS     // Define this to exclude non-data objects from the symtab dump
#define SKIP_ZERO_SIZED_SYMBOLS // Define this to exclude zero-sized objects from the symtab dump

#warning "Just return a symtab_t *"
int nm_dump_symtab(char *binary_path, symtab_t **symtab)
{
    Elf         *elf;
    Elf_Scn     *scn = NULL;
    GElf_Shdr   shdr;
    Elf_Data    *data;
    int         fd, i, count;

    (*symtab) = NULL;

    elf_version(EV_CURRENT);

    fd = open(binary_path, O_RDONLY);
    if (fd < 0) return 0;

    elf = elf_begin(fd, ELF_C_READ, NULL);
    if (elf == NULL) return 0;

    // Find the .symtab section
    int found_symtab = 0;
    while (((scn = elf_nextscn(elf, scn)) != NULL) && (!found_symtab)) {
        gelf_getshdr(scn, &shdr);
        if (shdr.sh_type == SHT_SYMTAB) {
            // Go print it
            found_symtab = 1;
            break;
        }
    }
    if (!found_symtab) return 0;

    // Get the number of symbols in the section
    data = elf_getdata(scn, NULL);
    count = shdr.sh_size / shdr.sh_entsize;
    if (count <= 0) return 0;
    symtab_entry_t *entries = malloc(count * sizeof(symtab_entry_t));
    if (entries == NULL) return 0;

    // Iterate over the symbols in the section
    int unfiltered = 0; // Counter for the number of symbols that pass the exclusion filter
    for (i = 0; i < count; ++i) {
        GElf_Sym sym;
        gelf_getsym(data, i, &sym);

        // Exclude non-data objects and zero-sized symbols
        int filter = 0;
#if defined(FILTER_DATA_OBJECTS)
#if 0
        /* Legal values for ST_TYPE subfield of st_info (symbol type). See elf.h from elfutils for details. */

        #define STT_NOTYPE      0               /* Symbol type is unspecified */
        #define STT_OBJECT      1               /* Symbol is a data object */
        #define STT_FUNC        2               /* Symbol is a code object */
        #define STT_SECTION     3               /* Symbol associated with a section */
        #define STT_FILE        4               /* Symbol's name is file name */
        #define STT_COMMON      5               /* Symbol is a common data object */
        #define STT_TLS         6               /* Symbol is thread-local data object*/
        #define STT_NUM         7               /* Number of defined types.  */
        #define STT_LOOS        10              /* Start of OS-specific */
        #define STT_GNU_IFUNC   10              /* Symbol is indirect code object */
        #define STT_HIOS        12              /* End of OS-specific */
        #define STT_LOPROC      13              /* Start of processor-specific */
        #define STT_HIPROC      15              /* End of processor-specific */
#endif
        int sym_type = GELF_ST_TYPE(sym.st_info);
        if ((sym_type != STT_OBJECT) && (sym_type != STT_COMMON) && (sym_type != STT_TLS)) filter = 1;
#endif
#if defined(SKIP_ZERO_SIZED_SYMBOLS)
        if ((!sym.st_value) || (!sym.st_size)) filter = 1;
#endif
        if (!filter)
        {
            // Copy the symbol name and address range
            entries[unfiltered].name = strdup(elf_strptr(elf, shdr.sh_link, sym.st_name));
            entries[unfiltered].start = sym.st_value;
            entries[unfiltered].size = sym.st_size;
            entries[unfiltered].end = sym.st_value + sym.st_size;
            unfiltered ++;
        }
    }

    /* Print the unfiltered symbol names and addresses
    for (i = 0; i < unfiltered; ++i) {
        fprintf(stderr, "!!! %s %s [0x%lx-0x%lx]\n", binary_path, entries[i].name, entries[i].start, entries[i].end + entries[i].size); 
    } */

    elf_end(elf);
    close(fd);

    // Return the symtab_t structure and the symbol count
    (*symtab) = malloc(sizeof(symtab_t));
    if (*symtab != NULL) {
        (*symtab)->entries = entries;
        (*symtab)->num_entries = unfiltered;
        return unfiltered;
    }
    else {
#warning "Should free entries[i].name here"
        free(entries);
        return 0;
    }
}

char * nm_get_symbol_name(symtab_t *symtab, unsigned long addr)
{
    int i;
    for (i = 0; i < symtab->num_entries; ++i) {
        if ((addr >= symtab->entries[i].start) && (addr < symtab->entries[i].end)) {
            return symtab->entries[i].name;
        }
    }
    return NULL;
}

void symtab_free(symtab_t *symtab)
{
    for (int i = 0; i < symtab->num_entries; ++i) {
        free(symtab->entries[i].name);
    }
    free(symtab->entries);
    free(symtab);
}

#warning "Implement some iterator macros as in maps"