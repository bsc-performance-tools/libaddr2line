#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "maps.h"

#define SKIP_SPECIAL_MAPPINGS // Define this to exclude special entries from the list of executable mappings (e.g., stack, heap, vdso, vvar, vsyscall, etc.) 

/**
 * maps_parse_file
 * 
 * Parse the /proc/self/maps file and store the entries in a maps_t structure.
 * The list of mappings has two chainings: one for all mappings, and another 
 * one for mappings with execution permissions.
 * 
 * @param maps_file Path to the /proc/self/maps file
 * @param mapping_list Pointer to the maps_t structure to store the mappings
 */
maps_t * maps_parse_file(char *maps_file) {
    int num_all_entries = 0, num_exec_entries = 0; 
    maps_entry_t *head_all = NULL, *tail_all = NULL;
    maps_entry_t *head_exec = NULL, *tail_exec = NULL;

    maps_t *mapping_list = (maps_t *)malloc(sizeof(maps_t));
    if (mapping_list == NULL) {
        return NULL;
    }

    // Open the maps file
    FILE *fd = fopen(maps_file, "r");
    if (fd != NULL)
    {
        char line[BUFSIZ];
        while (fgets(line, sizeof(line), fd) != NULL)
        {
            maps_entry_t *entry = (maps_entry_t *)malloc(sizeof(maps_entry_t));
            if (entry != NULL)
            {
                // Parse the line and store the values in the entry structure
                int ret = sscanf(line, "%lx-%lx %4s %lx %x:%x %d %4095[^\n]", &entry->start, &entry->end, entry->perms, &entry->offset, &entry->dev_major, &entry->dev_minor, &entry->inode, entry->pathname);
                if (ret >= 7)
                {
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
        fclose(fd);
    }
    // Store the lists in the maps_t structure
    mapping_list->all_entries = head_all;
    mapping_list->num_all_entries = num_all_entries;
    mapping_list->exec_entries = head_exec;
    mapping_list->num_exec_entries = num_exec_entries;
    // Get the path to the main executable from the first entry in the list of executable mappings
    mapping_list->main_exec = strdup((head_exec == NULL ? "./a.out" : head_exec->pathname));
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
            free(entry);
            entry = next;
        }
        free(mapping_list->main_exec);
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
maps_entry_t * maps_find_by_address(maps_entry_t *mapping_list, unsigned long address) {
    maps_entry_t *entry = mapping_list;
    while (entry != NULL)
    {
        if ((address >= entry->start) && (address < entry->end))
        {
            return entry;
        }
        entry = entry->next_all;
    }
    return NULL;
}
