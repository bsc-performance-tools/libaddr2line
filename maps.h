#pragma once

/**
 * Structure to hold a single entry from the /proc/self/maps file.
 */
typedef struct maps_entry {
    unsigned long start;
    unsigned long end;
    char perms[5];
    unsigned long offset;
    int dev_major;
    int dev_minor;
    int inode;
    char pathname[4096];          // Anonymous mappings have no pathname (empty string "", strlen() == 0)
    struct maps_entry *next_all;  // Next in the list of all entries
    struct maps_entry *next_exec; // Next in the list of executable entries
} maps_entry_t;

/**
 * Structure to hold the parsed /proc/self/maps file.
 */
typedef struct maps_t {
    maps_entry_t *list_all;       // List of all entries
    int num_all_entries;          // Number of all entries
    maps_entry_t *list_exec;      // List of executable entries
    int num_exec_entries;         // Number of executable entries
    char *main_exec;              // Path to the main executable
} maps_t;

maps_t * parse_maps_file(char *maps_file);
void free_maps(maps_t *mapping_list);
maps_entry_t * find_address_in_mappings(maps_t *mapping_list, unsigned long address);
maps_entry_t * find_address_in_exec_mappings(maps_t *mapping_list, unsigned long address);
