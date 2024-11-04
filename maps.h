#pragma once

#include "symtab.h"

// Available configuration options
#define OPTION_READ_SYMTAB             (1 << 0) // Read the symbol table for each mapping 

/**
 * Structure to hold a single entry from the /proc/self/maps file.
 */
typedef struct maps_entry {
    int index;                    // Index of the entry in the mappings list
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
    symtab_t *symtab;             // Symbol table for the mapping
} maps_entry_t;

/**
 * Structure to hold the parsed /proc/self/maps file.
 */
typedef struct maps_t {
    char *path;                   // Path to the maps file
    maps_entry_t *all_entries;    // List of all entries
    int num_all_entries;          // Number of all entries
    maps_entry_t *exec_entries;   // List of executable entries
    int num_exec_entries;         // Number of executable entries
    char *main_exec;              // Path to the main executable
} maps_t;

maps_t * maps_parse_file(char *maps_file, int options);
void maps_free(maps_t *mapping_list);
maps_entry_t * maps_find_by_address(maps_entry_t *mapping_list, unsigned long address, int search_filter);

enum {
    SEARCH_ALL = 0,
    SEARCH_EXEC 
};

// Macros to search for an address in the mappings
#define search_in_all_mappings(maps, address) maps_find_by_address(maps->all_entries, address, SEARCH_ALL)
#define search_in_exec_mappings(maps, address) maps_find_by_address(maps->exec_entries, address, SEARCH_EXEC)
#define address_in_mapping(entry, address) (address >= entry->start && address < entry->end)

/*
 * Macros to iterate over the `maps_t` structure.
 *
 * - `all_mappings` and `next_mapping`: For iterating over all entries.
 * - `exec_mappings` and `next_exec_mapping`: For iterating over executable entries.
 *
 * Important: Do not mix them!
 * Using the wrong macro pair can lead to skipped entries or crashes, as the lists use different pointers (`next_all` vs. `next_exec`).
 */
#define all_mappings(mapping_list) (mapping_list->all_entries)
#define all_mappings_size(mapping_list) (mapping_list->num_all_entries)
#define next_mapping(entry) (entry->next_all)

#define exec_mappings(mapping_list) (mapping_list->exec_entries)
#define exec_mappings_size(mapping_list) (mapping_list->num_exec_entries)
#define next_exec_mapping(entry) (entry->next_exec)

// Macro to get the path to the maps file
#define maps_path(mapping_list) (mapping_list->path)

// Macro to get the path to the main executable
#define maps_main_exec(mapping_list) (mapping_list->main_exec)