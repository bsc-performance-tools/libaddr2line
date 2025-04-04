#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "maps.h"
#include "magic.h"

#define SKIP_SPECIAL_MAPPINGS // Define this to exclude special entries from the list of executable mappings (e.g., stack, heap, vdso, vvar, vsyscall, etc.) 


/**
 * maps_parse_file
 * 
 * Parse the /proc/self/maps file and store the entries in a maps_t structure.
 * The list of mappings has two chainings: one for all mappings, and another 
 * one for mappings with execution permissions.
 * 
 * @param maps_file Path to the /proc/self/maps file
 * @param main_binary Absolute path to the main binary (optional)
 * @param mapping_list Pointer to the maps_t structure to store the mappings
 */
maps_t * maps_parse_file(char *maps_file, int options) {
    int num_all_entries = 0, num_exec_entries = 0; 
    maps_entry_t *head_all = NULL, *tail_all = NULL;
    maps_entry_t *head_exec = NULL, *tail_exec = NULL;

    maps_t *mapping_list = (maps_t *)malloc(sizeof(maps_t));
    if (mapping_list == NULL) {
        return NULL;
    }
    mapping_list->path = strdup(maps_file);
    
    // Open the maps file
    FILE *fd = fopen(maps_file, "r");
    if (fd != NULL)
    {
        // Initialize libmagic
        int magic_exists = 0;
        magic_t magic = magic_open(MAGIC_NONE);
        if (magic != NULL) {
            // Load the magic database
            if (magic_load(magic, NULL) == 0) {
                magic_exists = 1;
            }
        }

        char line[BUFSIZ];
        while (fgets(line, sizeof(line), fd) != NULL)
        {
            maps_entry_t *entry = (maps_entry_t *)malloc(sizeof(maps_entry_t));
            if (entry != NULL)
            {             
                entry->index = num_all_entries;
                entry->mapping_type = OTHER_MAPPING;

                // Parse the line and store the values in the entry structure
                int ret = sscanf(line, "%lx-%lx %4s %lx %x:%x %d %4095[^\n]", &entry->start, &entry->end, entry->perms, &entry->offset, &entry->dev_major, &entry->dev_minor, &entry->inode, entry->pathname);
                if (ret >= 7)
                {
                    if (magic_exists)
                    {
                        // Get the file type from libmagic if available
                        const char *file_type = magic_file(magic, entry->pathname);
                        if (file_type != NULL) {
                            // Check if the mapping is an executable
                            if (strstr(file_type, "executable")) {
                                // Check if the executable is position-independent
                                if (!strstr(file_type, "pie executable")) {
                                    entry->mapping_type = BINARY_PIE;
                                }
                                else entry->mapping_type = BINARY_NONPIE;
                            }
                            else if (strstr(file_type, "shared object")) {
                                entry->mapping_type = SHARED_LIBRARY;
                            }
                        }
                    }

                    entry->next_all = NULL;
                    entry->next_exec = NULL;
                    // Append the entry to the list of all mappings
                    if (head_all == NULL) 
                    {
                        head_all = entry;
                        tail_all = entry;
                    }
                    else
                    {
                        tail_all->next_all = entry;
                        tail_all = entry;
                    }
                    if (entry->perms[2] == 'x')
                    {
#if defined(SKIP_SPECIAL_MAPPINGS)
                        if ((strlen(entry->pathname) > 0) && (entry->pathname[0] != '['))
#endif
                        {
                            // Append the entry to the list of executable mappings
                            if (head_exec == NULL)
                            {
                                head_exec = entry;
                                tail_exec = entry;
                            }
                            else
                            {
                                tail_exec->next_exec = entry;
                                tail_exec = entry;
                            }
                            num_exec_entries++;
                        }
                    }
                    num_all_entries++;
                }
                else
                {
                    free(entry);
                }
            }
        }

      	// Clean up
    	if (magic_exists) magic_close(magic);
        fclose(fd);
    }

    // Store the lists in the maps_t structure
    mapping_list->all_entries = head_all;
    mapping_list->num_all_entries = num_all_entries;
    mapping_list->exec_entries = head_exec;
    mapping_list->num_exec_entries = num_exec_entries;

    // Read the symbol tables for all mappings if requested
    maps_entry_t *entry = mapping_list->all_entries;
    while (entry != NULL) {
        entry->symtab = NULL;

#if defined(HAVE_LIBSYMTAB)
        if (options & OPTION_READ_SYMTAB) {
            entry->symtab = symtab_read(entry->pathname);
        }
#endif
        entry = entry->next_all;
    }

    return mapping_list;
}

/**
 * maps_free
 * 
 * Free the memory used by the maps_t structure.
 * 
 * @param mapping_list Pointer to the maps_t structure to free
 */
void maps_free(maps_t *mapping_list) 
{
    if (mapping_list != NULL) 
    {
        maps_entry_t *entry = mapping_list->all_entries;
        while (entry != NULL)
        {
            maps_entry_t *next = entry->next_all;
#if defined(HAVE_LIBSYMTAB)
            symtab_free(entry->symtab);
#endif
            free(entry);
            entry = next;
        }
        free(mapping_list);
    }
}

/**
 * maps_find_by_address
 * 
 * Find the entry in the list of mappings that contains the specified address.
 * 
 * @param mapping_list Pointer to the list of mappings
 * @param address Address to search for
 * @return Pointer to the entry that contains the address, or NULL if not found
 */
maps_entry_t * maps_find_by_address(maps_entry_t *mapping_list, unsigned long address, int search_filter)
{
    maps_entry_t *entry = mapping_list;
    while (entry != NULL)
    {
        if ((address >= entry->start) && (address < entry->end))
        {
            return entry;
        }
        entry = (search_filter == SEARCH_EXEC) ? entry->next_exec : entry->next_all;
    }
    return NULL;
}

#if 0
/**
 * maps_find_by_name
 *
 * Finds all entries in the list of mappings that corresponds to the given name.
 *
 * @param mapping_list Pointer to the list of mappings
 * @param name Name of the mapping to search for
 * @return Pointer to a NULL-terminated array of pointers to matching entries, or NULL if not found
 */
maps_entry_t ** maps_find_by_name(maps_entry_t *mapping_list, const char *name)
{
    int count_matches = 0;
    maps_entry_t **matching_entries = NULL;

    maps_entry_t *entry = mapping_list;
    while (entry != NULL)
    {
        if (strcmp(entry->pathname, name) == 0) count_matches++;
    }
    if (count_matches > 0)
    {
        matching_entries = malloc(sizeof(maps_entry_t *) * count_matches + 1);
        if (matching_entries != NULL) 
        {
            matching_entries[count_matches] = NULL; // Null-terminate the array

            entry = mapping_list;
            for (int i = 0; i < count_matches; i++)
            {
                if (strcmp(entry->pathname, name) == 0)
                {
                    matching_entries[count_matches] = entry;
                }
            }
        }
    }
    return matching_entries;
}
#endif
