#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libelf.h>
#include <gelf.h>
#include <unistd.h>
#include <stdlib.h>
#include "nm.h"

#define FILTER_DATA_OBJECTS     // Define this to exclude non-data objects from the symtab dump
#define SKIP_ZERO_SIZED_SYMBOLS // Define this to exclude zero-sized objects from the symtab dump

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
            entries[ii].name = elf_strptr(elf, shdr.sh_link, sym.st_name);
            entries[ii].start = sym.st_value;
            entries[ii].size = sym.st_size;
            entries[ii].end = sym.st_value + sym.st_size;
            ii ++;
        }
    }

    // Print the unfiltered symbol names and addresses
    for (i = 0; i < ii; ++i) {
        printf("%s [0x%lx-0x%lx]\n", entries[i].name, entries[i].start, entries[i].end + entries[i].size); 
    } 
    elf_end(elf);
    close(fd);

    symtab->entries = entries;
    symtab->num_entries = ii;
    return ii;
}

