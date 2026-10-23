// Core offline query dashboard engine for faculty directory data

#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define DATABASE_FILE   "faculty_database.txt"
#define AUDIT_LOG_FILE  "tracker_audit.log"
#define BINARY_BACKUP   "tracker_backup.bin"

#define MAX_LINE_LEN    1024
#define MAX_NAME_LEN    128
#define MAX_DESIG_LEN   128
#define MAX_CABIN_LEN   64
#define MAX_SCHOOL_LEN  32
#define MAX_EMAIL_LEN   128
#define MAX_QUERY_LEN   128
#define MAX_TOKENS      16
#define INITIAL_CAPACITY 16

// Represents a single faculty member's directory profile
typedef struct {
    char name[MAX_NAME_LEN];          // Full name
    char designation[MAX_DESIG_LEN];  // Academic or administrative title
    char cabin[MAX_CABIN_LEN];        // Office or cabin location
    char school[MAX_SCHOOL_LEN];      // School code (e.g., SCOPE, SENSE)
    char email[MAX_EMAIL_LEN];        // Institutional email address
} FacultyRecord;

// Shared storage for different search parameter types
typedef union {
    char text_criteria[MAX_QUERY_LEN]; // Case-insensitive search query string
    int numeric_filter_id;             // Numeric school filter ID
    unsigned long audit_timestamp;     // Epoch timestamp for query logging
} QueryPayload;

// Holds search parameters and match results for a single query
typedef struct {
    int search_type;                   // 1: Name, 2: School, 3: Cabin
    QueryPayload payload;
    int matches_found;                 // Total matching records
} QueryTransaction;

// Dynamic array container for loaded faculty records
typedef struct {
    FacultyRecord *records;
    size_t count;                      // Number of loaded records
    size_t capacity;                   // Allocated array capacity
} FacultyRepository;

// Ingestion and memory management
void initialize_repository(FacultyRepository *repo);
int ingest_database(FacultyRepository *repo, const char *filepath);
void free_repository(FacultyRepository *repo);

// String utilities and query tokenization
void trim_whitespace(char *str);
void to_lowercase_copy(char *dest, const char *src, size_t dest_size);
int tokenize_query(char *query, char tokens[MAX_TOKENS][MAX_QUERY_LEN], int *token_count);

// Search and filtering operations
int matches_all_tokens(const char *target_str, char tokens[MAX_TOKENS][MAX_QUERY_LEN], int token_count);
void execute_name_search(const FacultyRepository *repo);
void execute_school_filter(const FacultyRepository *repo);
void execute_cabin_search(const FacultyRepository *repo);

// Terminal UI and persistence
void render_ascii_card(const FacultyRecord *faculty, int index, int total_matches);
void display_system_metrics(const FacultyRepository *repo);
void display_main_menu(void);

void log_query_metric(const char *query_type, const char *query_str, int results_count);
void export_binary_backup(const FacultyRepository *repo);
void import_binary_backup(FacultyRepository *repo);

// Allocates the initial heap buffer for the faculty repository
void initialize_repository(FacultyRepository *repo) {
    if (!repo) {
        return;
    }

    repo->count = 0;
    repo->capacity = INITIAL_CAPACITY;

    repo->records = (FacultyRecord *)malloc(repo->capacity * sizeof(FacultyRecord));

    if (repo->records == NULL) {
        fprintf(stderr, "[FATAL ERROR] Memory allocation failed for faculty repository!\n");
        exit(EXIT_FAILURE);
    }
}

// Reads pipe-delimited faculty records from disk, expanding capacity as needed
int ingest_database(FacultyRepository *repo, const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    
    if (!fp) {
        fprintf(stderr, "[ERROR] Unable to open database file: '%s'\n", filepath);
        return 0;
    }

    char line[MAX_LINE_LEN];
    size_t lines_read = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        // Strip trailing CR/LF characters
        size_t len = strlen(line);
        if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[len - 1] = '\0';
            len--;
        }
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
            len--;
        }

        // Skip empty lines
        if (len == 0) {
            continue;
        }

        // Double buffer capacity when full
        if (repo->count >= repo->capacity) {
            size_t new_capacity = repo->capacity * 2;
            FacultyRecord *resized_records = (FacultyRecord *)realloc(
                repo->records, 
                new_capacity * sizeof(FacultyRecord)
            );

            if (resized_records == NULL) {
                fprintf(stderr, "[FATAL ERROR] realloc() failed at record %zu!\n", repo->count);
                fclose(fp);
                return 0;
            }

            repo->records = resized_records;
            repo->capacity = new_capacity;
        }

        FacultyRecord *target_slot = repo->records + repo->count;

        // Parse pipe-delimited fields into the current record
        char *token = NULL;
        char *rest = line;
        int field_idx = 0;

        // Reset record fields before parsing
        target_slot->name[0] = '\0';
        target_slot->designation[0] = '\0';
        target_slot->cabin[0] = '\0';
        target_slot->school[0] = '\0';
        target_slot->email[0] = '\0';

        // Expected format: Name|Designation|Cabin|School|Email
        while ((token = strtok(rest, "|")) != NULL) {
            rest = NULL;

            switch (field_idx) {
                case 0:
                    strncpy(target_slot->name, token, sizeof(target_slot->name) - 1);
                    target_slot->name[sizeof(target_slot->name) - 1] = '\0';
                    break;
                case 1:
                    strncpy(target_slot->designation, token, sizeof(target_slot->designation) - 1);
                    target_slot->designation[sizeof(target_slot->designation) - 1] = '\0';
                    break;
                case 2:
                    strncpy(target_slot->cabin, token, sizeof(target_slot->cabin) - 1);
                    target_slot->cabin[sizeof(target_slot->cabin) - 1] = '\0';
                    break;
                case 3:
                    strncpy(target_slot->school, token, sizeof(target_slot->school) - 1);
                    target_slot->school[sizeof(target_slot->school) - 1] = '\0';
                    break;
                case 4:
                    strncpy(target_slot->email, token, sizeof(target_slot->email) - 1);
                    target_slot->email[sizeof(target_slot->email) - 1] = '\0';
                    break;
                default:
                    break;
            }
            field_idx++;
        }

        // Commit record only if a name and at least 4 fields were parsed
        if (strlen(target_slot->name) > 0 && field_idx >= 4) {
            repo->count++;
            lines_read++;
        }
    }

    fclose(fp);
    return (int)lines_read;
}

// Frees heap memory held by the repository and resets counters
void free_repository(FacultyRepository *repo) {
    if (repo != NULL) {
        if (repo->records != NULL) {
            free(repo->records);
            repo->records = NULL;
        }
        repo->count = 0;
        repo->capacity = 0;
    }
}

// Strips leading and trailing whitespace in-place
void trim_whitespace(char *str) {
    if (!str || *str == '\0') {
        return;
    }

    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (*start == '\0') {
        *str = '\0';
        return;
    }

    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

// Copies src into dest as lowercase, ensuring null termination
void to_lowercase_copy(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        return;
    }

    size_t i = 0;
    while (src[i] != '\0' && i < dest_size - 1) {
        dest[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dest[i] = '\0';
}

// Splits a query string by whitespace into lowercase tokens
int tokenize_query(char *query, char tokens[MAX_TOKENS][MAX_QUERY_LEN], int *token_count) {
    if (!query || !token_count) {
        return 0;
    }

    *token_count = 0;
    char query_copy[MAX_QUERY_LEN];
    strncpy(query_copy, query, sizeof(query_copy) - 1);
    query_copy[sizeof(query_copy) - 1] = '\0';

    char *token = strtok(query_copy, " \t\r\n");
    
    while (token != NULL && *token_count < MAX_TOKENS) {
        if (strlen(token) > 0) {
            to_lowercase_copy(tokens[*token_count], token, MAX_QUERY_LEN);
            (*token_count)++;
        }
        token = strtok(NULL, " \t\r\n");
    }

    return *token_count;
}

// Returns 1 if all lowercase tokens appear as substrings in target_str
int matches_all_tokens(const char *target_str, char tokens[MAX_TOKENS][MAX_QUERY_LEN], int token_count) {
    if (!target_str || token_count <= 0) {
        return 0;
    }

    char target_lower[MAX_LINE_LEN];
    to_lowercase_copy(target_lower, target_str, sizeof(target_lower));

    for (int t = 0; t < token_count; t++) {
        if (strstr(target_lower, tokens[t]) == NULL) {
            return 0;
        }
    }

    return 1;
}

// Appends a timestamped search entry to the audit log file
void log_query_metric(const char *query_type, const char *query_str, int results_count) {
    FILE *audit_fp = fopen(AUDIT_LOG_FILE, "a");
    if (!audit_fp) {
        return;
    }

    time_t raw_time;
    time(&raw_time);
    struct tm *time_info = localtime(&raw_time);
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", time_info);

    fprintf(audit_fp, "[%s] QUERY_TYPE: %-12s | RESULTS: %4d | SEARCH_TERM: \"%s\"\n",
            time_buffer, query_type, results_count, query_str ? query_str : "");

    fclose(audit_fp);
}

// Writes the current faculty records array to a binary backup file
void export_binary_backup(const FacultyRepository *repo) {
    if (!repo || repo->count == 0) {
        printf("\n[!] Repository is empty. Nothing to export.\n");
        return;
    }

    FILE *bin_fp = fopen(BINARY_BACKUP, "wb");
    if (!bin_fp) {
        printf("\n[ERROR] Failed to open binary file '%s' for writing.\n", BINARY_BACKUP);
        return;
    }

    // Write record count header followed by raw struct array
    size_t count = repo->count;
    fwrite(&count, sizeof(size_t), 1, bin_fp);

    size_t written = fwrite(repo->records, sizeof(FacultyRecord), count, bin_fp);
    fclose(bin_fp);

    printf("\n+------------------------------------------------------------------------+");
    printf("\n| [SUCCESS] Binary Archive Export Completed!                             |");
    printf("\n| Destination File : %-51s |", BINARY_BACKUP);
    printf("\n| Records Exported : %-51zu |", written);
    printf("\n| Total Bytes      : %-51zu |", sizeof(size_t) + written * sizeof(FacultyRecord));
    printf("\n+------------------------------------------------------------------------+\n");
}

// Replaces the in-memory repository with records loaded from the binary backup
void import_binary_backup(FacultyRepository *repo) {
    if (!repo) return;

    FILE *bin_fp = fopen(BINARY_BACKUP, "rb");
    if (!bin_fp) {
        printf("\n[ERROR] Binary backup file '%s' not found.\n", BINARY_BACKUP);
        return;
    }

    size_t backup_count = 0;
    if (fread(&backup_count, sizeof(size_t), 1, bin_fp) != 1 || backup_count == 0) {
        printf("\n[ERROR] Invalid or corrupt binary backup header.\n");
        fclose(bin_fp);
        return;
    }

    FacultyRecord *temp_records = (FacultyRecord *)malloc(backup_count * sizeof(FacultyRecord));
    if (!temp_records) {
        printf("\n[ERROR] Memory allocation failed during binary restore.\n");
        fclose(bin_fp);
        return;
    }

    size_t read_count = fread(temp_records, sizeof(FacultyRecord), backup_count, bin_fp);
    fclose(bin_fp);

    if (read_count != backup_count) {
        printf("\n[ERROR] Read mismatch: Expected %zu, got %zu records.\n", backup_count, read_count);
        free(temp_records);
        return;
    }

    // Replace existing repository buffer with restored records
    free_repository(repo);
    repo->records = temp_records;
    repo->count = read_count;
    repo->capacity = backup_count;

    printf("\n+------------------------------------------------------------------------+");
    printf("\n| [SUCCESS] Binary Archive Ingestion Completed!                          |");
    printf("\n| Source File      : %-51s |", BINARY_BACKUP);
    printf("\n| Records Restored : %-51zu |", repo->count);
    printf("\n+------------------------------------------------------------------------+\n");
}

// Formats and prints a single faculty profile as a bordered ASCII card
void render_ascii_card(const FacultyRecord *faculty, int index, int total_matches) {
    const char *cabin_val = (strlen(faculty->cabin) > 0 && strcmp(faculty->cabin, "N/A") != 0) 
                            ? faculty->cabin 
                            : "Cabin details on campus";

    printf("\n  +---------------------------------------------------------------------------------------------------+");
    printf("\n  |  FACULTY PROFILE #%03d / %03d                                      INSTITUTIONAL DIRECTORY CARD     |", index, total_matches);
    printf("\n  +---------------------------------------------------------------------------------------------------+");
    printf("\n  |  Full Name      : %-78.78s  |", faculty->name);
    printf("\n  |  Designation    : %-78.78s  |", faculty->designation);
    printf("\n  |  Cabin Number   : %-78.78s  |", cabin_val);
    printf("\n  |  School Division: %-78.78s  |", faculty->school);
    printf("\n  |  Official Email : %-78.78s  |", faculty->email);
    printf("\n  +---------------------------------------------------------------------------------------------------+");
}

// Computes and prints faculty count and percentage breakdown by school
void display_system_metrics(const FacultyRepository *repo) {
    printf("\n  ===================================================================================================");
    printf("\n                             VIT-AP UNIVERSITY DIRECTORY ANALYTICS");
    printf("\n  ===================================================================================================");
    printf("\n  Total Synchronized Faculty : %zu active records\n", repo->count);

    // Count faculty members per school
    int scope_cnt = 0, sense_cnt = 0, sas_cnt = 0, smec_cnt = 0;
    int vsb_cnt = 0, vsl_cnt = 0, vish_cnt = 0, sbst_cnt = 0, other_cnt = 0;

    for (size_t i = 0; i < repo->count; i++) {
        const FacultyRecord *f = repo->records + i;
        if (strcmp(f->school, "SCOPE") == 0) scope_cnt++;
        else if (strcmp(f->school, "SENSE") == 0) sense_cnt++;
        else if (strcmp(f->school, "SAS") == 0)   sas_cnt++;
        else if (strcmp(f->school, "SMEC") == 0)  smec_cnt++;
        else if (strcmp(f->school, "VSB") == 0)   vsb_cnt++;
        else if (strcmp(f->school, "VSL") == 0)   vsl_cnt++;
        else if (strcmp(f->school, "VISH") == 0)  vish_cnt++;
        else if (strcmp(f->school, "SBST") == 0)  sbst_cnt++;
        else other_cnt++;
    }

    printf("\n  Academic Unit Breakdown:");
    printf("\n  ---------------------------------------------------------------------------------------------------");
    printf("\n   [SCOPE] School of Computer Science & Engineering       : %3d faculty (%5.1f%%)", scope_cnt, (scope_cnt * 100.0) / repo->count);
    printf("\n   [SAS]   School of Advanced Sciences                    : %3d faculty (%5.1f%%)", sas_cnt, (sas_cnt * 100.0) / repo->count);
    printf("\n   [SENSE] School of Electronics Engineering              : %3d faculty (%5.1f%%)", sense_cnt, (sense_cnt * 100.0) / repo->count);
    printf("\n   [VISH]  School of Social Sciences & Humanities         : %3d faculty (%5.1f%%)", vish_cnt, (vish_cnt * 100.0) / repo->count);
    printf("\n   [VSB]   VIT-AP School of Business                      : %3d faculty (%5.1f%%)", vsb_cnt, (vsb_cnt * 100.0) / repo->count);
    printf("\n   [VSL]   VIT-AP School of Law                           : %3d faculty (%5.1f%%)", vsl_cnt, (vsl_cnt * 100.0) / repo->count);
    printf("\n   [SMEC]  School of Mechanical Engineering               : %3d faculty (%5.1f%%)", smec_cnt, (smec_cnt * 100.0) / repo->count);
    printf("\n   [SBST]  School of Bio Sciences & Technology            : %3d faculty (%5.1f%%)", sbst_cnt, (sbst_cnt * 100.0) / repo->count);
    if (other_cnt > 0) {
        printf("\n   [OTHER] Central Administration / Adjunct Faculty       : %3d faculty (%5.1f%%)", other_cnt, (other_cnt * 100.0) / repo->count);
    }
    printf("\n  ===================================================================================================\n");
}

// Prompts for a name query and prints all matching faculty cards
void execute_name_search(const FacultyRepository *repo) {
    char input_buffer[MAX_QUERY_LEN];
    printf("\n  ===================================================================================================");
    printf("\n                                    FACULTY NAME SEARCH");
    printf("\n  ===================================================================================================");
    printf("\n  Enter Faculty Name query (multi-word supported): ");

    if (!fgets(input_buffer, sizeof(input_buffer), stdin)) {
        return;
    }
    trim_whitespace(input_buffer);

    if (strlen(input_buffer) == 0) {
        printf("  [!] Search query cannot be empty.\n");
        return;
    }

    QueryTransaction txn;
    txn.search_type = 1; // Name search
    strncpy(txn.payload.text_criteria, input_buffer, sizeof(txn.payload.text_criteria) - 1);
    txn.payload.text_criteria[sizeof(txn.payload.text_criteria) - 1] = '\0';
    txn.matches_found = 0;

    char tokens[MAX_TOKENS][MAX_QUERY_LEN];
    int token_count = 0;
    tokenize_query(txn.payload.text_criteria, tokens, &token_count);

    if (token_count == 0) {
        printf("  [!] Invalid search term.\n");
        return;
    }

    printf("\n  [*] Executing scan across %zu faculty records for tokens: ", repo->count);
    for (int t = 0; t < token_count; t++) {
        printf("\"%s\" ", tokens[t]);
    }
    printf("\n");

    // Count matching records first to display total matches on each card
    for (size_t i = 0; i < repo->count; i++) {
        const FacultyRecord *curr = repo->records + i;
        if (matches_all_tokens(curr->name, tokens, token_count)) {
            txn.matches_found++;
        }
    }

    if (txn.matches_found == 0) {
        printf("\n  [-] No faculty profiles found matching query: \"%s\"\n", input_buffer);
    } else {
        printf("\n  [+] Query Matched %d Faculty Profile%s:\n", 
               txn.matches_found, 
               (txn.matches_found > 1) ? "s" : "");

        int display_idx = 1;
        // Render matching records
        for (size_t i = 0; i < repo->count; i++) {
            const FacultyRecord *curr = repo->records + i;
            if (matches_all_tokens(curr->name, tokens, token_count)) {
                render_ascii_card(curr, display_idx++, txn.matches_found);
            }
        }
        printf("\n");
    }

    log_query_metric("NAME_SEARCH", txn.payload.text_criteria, txn.matches_found);
}

// Prompts for a school code and prints all faculty in that school
void execute_school_filter(const FacultyRepository *repo) {
    char input_buffer[MAX_QUERY_LEN];
    printf("\n  ===================================================================================================");
    printf("\n                                    FACULTY SCHOOL FILTER");
    printf("\n  ===================================================================================================");
    printf("\n  Available Schools: SCOPE | SENSE | SAS | SMEC | VSB | VSL | VISH | SBST");
    printf("\n  Enter School Acronym (e.g., 'SCOPE'): ");

    if (!fgets(input_buffer, sizeof(input_buffer), stdin)) {
        return;
    }
    trim_whitespace(input_buffer);

    if (strlen(input_buffer) == 0) {
        printf("  [!] School filter cannot be empty.\n");
        return;
    }

    char target_school[MAX_SCHOOL_LEN];
    to_lowercase_copy(target_school, input_buffer, sizeof(target_school));

    QueryTransaction txn;
    txn.search_type = 2; // School filter
    strncpy(txn.payload.text_criteria, input_buffer, sizeof(txn.payload.text_criteria) - 1);
    txn.payload.text_criteria[sizeof(txn.payload.text_criteria) - 1] = '\0';
    txn.matches_found = 0;

    // Count matching records
    for (size_t i = 0; i < repo->count; i++) {
        char school_lower[MAX_SCHOOL_LEN];
        to_lowercase_copy(school_lower, (repo->records + i)->school, sizeof(school_lower));
        if (strstr(school_lower, target_school) != NULL) {
            txn.matches_found++;
        }
    }

    if (txn.matches_found == 0) {
        printf("\n  [-] No faculty records found under School: \"%s\"\n", input_buffer);
    } else {
        printf("\n  [+] Found %d faculty member%s in %s:\n", 
               txn.matches_found, 
               (txn.matches_found > 1) ? "s" : "",
               input_buffer);

        int display_idx = 1;
        for (size_t i = 0; i < repo->count; i++) {
            const FacultyRecord *curr = repo->records + i;
            char school_lower[MAX_SCHOOL_LEN];
            to_lowercase_copy(school_lower, curr->school, sizeof(school_lower));
            if (strstr(school_lower, target_school) != NULL) {
                render_ascii_card(curr, display_idx++, txn.matches_found);
            }
        }
        printf("\n");
    }

    log_query_metric("SCHOOL_FILTER", txn.payload.text_criteria, txn.matches_found);
}

// Prompts for a cabin or building token and prints matching faculty cards
void execute_cabin_search(const FacultyRepository *repo) {
    char input_buffer[MAX_QUERY_LEN];
    printf("\n  ===================================================================================================");
    printf("\n                                    CABIN / LOCATION SEARCH");
    printf("\n  ===================================================================================================");
    printf("\n  Enter Cabin or Block identifier (e.g., '705', 'CB', 'G23', 'AB-2'): ");

    if (!fgets(input_buffer, sizeof(input_buffer), stdin)) {
        return;
    }
    trim_whitespace(input_buffer);

    if (strlen(input_buffer) == 0) {
        printf("  [!] Cabin search query cannot be empty.\n");
        return;
    }

    QueryTransaction txn;
    txn.search_type = 3; // Cabin search
    strncpy(txn.payload.text_criteria, input_buffer, sizeof(txn.payload.text_criteria) - 1);
    txn.payload.text_criteria[sizeof(txn.payload.text_criteria) - 1] = '\0';
    txn.matches_found = 0;

    char tokens[MAX_TOKENS][MAX_QUERY_LEN];
    int token_count = 0;
    tokenize_query(txn.payload.text_criteria, tokens, &token_count);

    for (size_t i = 0; i < repo->count; i++) {
        const FacultyRecord *curr = repo->records + i;
        if (matches_all_tokens(curr->cabin, tokens, token_count)) {
            txn.matches_found++;
        }
    }

    if (txn.matches_found == 0) {
        printf("\n  [-] No faculty assigned to cabin/location matching: \"%s\"\n", input_buffer);
    } else {
        printf("\n  [+] Matched %d Faculty Profile%s for Cabin \"%s\":\n",
               txn.matches_found,
               (txn.matches_found > 1) ? "s" : "",
               input_buffer);

        int display_idx = 1;
        for (size_t i = 0; i < repo->count; i++) {
            const FacultyRecord *curr = repo->records + i;
            if (matches_all_tokens(curr->cabin, tokens, token_count)) {
                render_ascii_card(curr, display_idx++, txn.matches_found);
            }
        }
        printf("\n");
    }

    log_query_metric("CABIN_SEARCH", txn.payload.text_criteria, txn.matches_found);
}

// Prints the interactive CLI menu options
void display_main_menu(void) {
    printf("\n+---------------------------------------------------------------------------------------------------+");
    printf("\n|                      VIT-AP UNIVERSITY - OFFLINE FACULTY TRACKER SYSTEM                           |");
    printf("\n+---------------------------------------------------------------------------------------------------+");
    printf("\n|  1. Search Faculty by Name (Multi-word supported)                                                 |");
    printf("\n|  2. Filter Faculty by School (SCOPE, SENSE, SAS, SMEC, VSB, VSL, VISH, SBST)                      |");
    printf("\n|  3. Search Faculty by Cabin / Building Location (e.g., '705', 'CB', 'AB-1')                       |");
    printf("\n|  4. Display Institutional Statistics & Distribution Metrics                                       |");
    printf("\n|  5. Export Local Database to Binary Archive ('tracker_backup.bin')                                |");
    printf("\n|  6. Import / Restore Database from Binary Archive                                                 |");
    printf("\n|  7. Exit System Application                                                                       |");
    printf("\n+---------------------------------------------------------------------------------------------------+");
    printf("\n  Select Menu Option [1 - 7]: ");
}

int main(void) {
    FacultyRepository repository;

    printf("\nBOOTSTRAPPING LOCAL UNIVERSITY FACULTY INFORMATION TRACKER\n");

    initialize_repository(&repository);

    printf("[*] Ingesting offline faculty flat-file: '%s'...\n", DATABASE_FILE);
    int loaded = ingest_database(&repository, DATABASE_FILE);

    if (loaded <= 0) {
        fprintf(stderr, "\n[FATAL] Failed to load records from '%s'.\n", DATABASE_FILE);
        fprintf(stderr, "Please verify that 'sync_faculty.py' has generated the database file.\n");
        free_repository(&repository);
        return EXIT_FAILURE;
    }

    printf("[+] Successfully synchronized %zu faculty profiles into local memory!\n", repository.count);

    int user_choice = 0;
    char menu_input[64];

    do {
        display_main_menu();

        if (!fgets(menu_input, sizeof(menu_input), stdin)) {
            break;
        }

        trim_whitespace(menu_input);
        user_choice = atoi(menu_input);

        switch (user_choice) {
            case 1:
                execute_name_search(&repository);
                break;
            case 2:
                execute_school_filter(&repository);
                break;
            case 3:
                execute_cabin_search(&repository);
                break;
            case 4:
                display_system_metrics(&repository);
                break;
            case 5:
                export_binary_backup(&repository);
                break;
            case 6:
                import_binary_backup(&repository);
                break;
            case 7:
                printf("\n[*] Exiting University Faculty Information Tracker. Shutting down cleanly...\n");
                break;
            default:
                printf("\n[!] Invalid selection '%s'. Please enter a valid option (1 - 7).\n", menu_input);
                break;
        }

    } while (user_choice != 7);

    free_repository(&repository);
    printf("[+] Memory deallocated successfully. Process finished.\n\n");

    return EXIT_SUCCESS;
}
