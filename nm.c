#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libelf.h>
#include <gelf.h>
#include <unistd.h>

void
main(int argc, char **argv)
{
    Elf         *elf;
    Elf_Scn     *scn = NULL;
    GElf_Shdr   shdr;
    Elf_Data    *data;
    int         fd, ii, count;

    elf_version(EV_CURRENT);

    fd = open(argv[1], O_RDONLY);
    elf = elf_begin(fd, ELF_C_READ, NULL);

    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        gelf_getshdr(scn, &shdr);
        if (shdr.sh_type == SHT_SYMTAB) {
            /* found a symbol table, go print it. */
            break;
        }
    }

    data = elf_getdata(scn, NULL);
    count = shdr.sh_size / shdr.sh_entsize;

    /* print the symbol names */
    for (ii = 0; ii < count; ++ii) {
        GElf_Sym sym;
        gelf_getsym(data, ii, &sym);
	
// XXX These filter out entries with addr 0x0, and with size 0 
// XXX Those with size 0 showed in binutils with a negative size (end address smaller than start address)
//      if ((sym.st_value) && (sym.st_size))
        if ((sym.st_value))
                printf("Symbol name = %s value=0x%jx size=%ju [0x%x-0x%x]\n", elf_strptr(elf, shdr.sh_link, sym.st_name), (uintmax_t)sym.st_value, (uintmax_t)sym.st_size, sym.st_value, sym.st_value + sym.st_size);
    }
    elf_end(elf);
    close(fd);
}

