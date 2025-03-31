#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if defined(HAVE_ELFUTILS)
# include <libelf.h>
# include <gelf.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

#define FILTER_DATA_OBJECTS     // Define this to exclude non-data objects from the symtab dump
#define SKIP_ZERO_SIZED_SYMBOLS // Define this to exclude zero-sized objects from the symtab dump

/**
 * read_symtab_with_libelf
 * 
 * Uses libelf to read the symbol table of the given binary file.
 * The defines FILTER_DATA_OBJECTS and SKIP_ZERO_SIZED_SYMBOLS can be used to exclude certain types of symbols.
 * 
 * @param binary_path The path to the binary file
 * @param symtab_out Pointer to an empty symtab_t structure to store the symbol table
 */
#if defined(HAVE_ELFUTILS)
static void read_symtab_with_libelf(char *binary_path, symtab_t **symtab_out)
{
    Elf             *elf = NULL;
    Elf_Scn         *scn = NULL;
    GElf_Shdr       shdr;
    Elf_Data        *data = NULL;
    int             fd = -1, i = 0;
    int             count = 0; // Counter for the number of symbols in the .symtab section
    int             unfiltered = 0; // Counter for the number of symbols that pass the exclusion filter
    symtab_entry_t *symtab_entries = NULL;

    if ((binary_path == NULL) || (symtab_out == NULL)) return;

    elf_version(EV_CURRENT);

    fd = open(binary_path, O_RDONLY);
    if (fd < 0) return;

    elf = elf_begin(fd, ELF_C_READ, NULL);
    if (elf != NULL) 
    {
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

        if (found_symtab) 
        {
            // Allocate memory for the symbol table
            data = elf_getdata(scn, NULL);
            count = shdr.sh_size / shdr.sh_entsize;
            if (count > 0) 
            {
                symtab_entries = malloc(count * sizeof(symtab_entry_t));
                if (symtab_entries != NULL)
                {
                    // Iterate over the symbols in the section
                    for (i = 0; i < count; ++i)
                    {
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
                            symtab_entries[unfiltered].name = strdup(elf_strptr(elf, shdr.sh_link, sym.st_name));
                            symtab_entries[unfiltered].start = sym.st_value;
                            symtab_entries[unfiltered].size = sym.st_size;
                            symtab_entries[unfiltered].end = sym.st_value + sym.st_size;
                            unfiltered ++;
                        }
                    }
                }  
            }  
        }      
        elf_end(elf);
    }
    close(fd);

    /* Print the unfiltered symbol names and addresses 
    for (i = 0; i < unfiltered; ++i) {
        fprintf(stderr, "%s %s [0x%lx-0x%lx]\n", binary_path, symtab->entries[i].name, symtab->entries[i].start, symtab->entries[i].end); 
    } */

    if ((unfiltered > 0) && (unfiltered < count))
    {
        // Shrink the array if necessary
        symtab_entries = realloc(symtab_entries, unfiltered * sizeof(symtab_entry_t));
    }
    else if ((count > 0) && (unfiltered == 0))
    {
        // All symbols were filtered out, so free the array
        free(symtab_entries);
        symtab_entries = NULL;
    }
    (*symtab_out)->entries = symtab_entries;
    (*symtab_out)->num_entries = unfiltered;
}
#endif

/**
 * symtab_read
 *
 * Read the symbol table from a binary file.
 * Currently this operation is only supported through libelf.
 * If libelf is not available, this function will return an empty symtab_t structure (zero entries).
 *
 * @param binary_path The path to the binary file
 * @return A symtab_t structure containing the symbol table, or NULL if an error occurred
 */
symtab_t *symtab_read(char *binary_path)
{
    symtab_t *symtab = malloc(sizeof(symtab_t));

    if (symtab != NULL)
    {
        symtab->entries = NULL;
        symtab->num_entries = 0;
#if defined(HAVE_ELFUTILS)
        read_symtab_with_libelf(binary_path, &symtab);
#endif
    }
    return symtab;
}

/**
 * symtab_find_symbol
 * 
 * Given an address, return the name of the symbol that contains it.
 * 
 * @param symtab The symtab_t structure containing the symbol table
 * @param addr The address to look up
 * @return The name of the symbol containing the address, or NULL if not found
 */
char * symtab_find_symbol(symtab_t *symtab, unsigned long addr)
{
    int i;
    for (i = 0; i < symtab->num_entries; ++i) {
        if ((addr >= symtab->entries[i].start) && (addr < symtab->entries[i].end)) {
            return symtab->entries[i].name;
        }
    }
    return NULL;
}

/**
 * symtab_free
 * 
 * Free the symtab_t structure and its contents.
 * 
 * @param symtab The symtab_t structure to free
 */
void symtab_free(symtab_t *symtab)
{
    if (symtab != NULL) {
        if (symtab->entries != NULL) {
            int i = 0;
            for (i = 0; i < symtab->num_entries; ++i) {
                free(symtab->entries[i].name);
            }
            free(symtab->entries);
        }
        free(symtab);
    }
}
