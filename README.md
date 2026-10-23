# University Faculty Information Tracker

An offline CLI directory and search engine for VIT-AP University faculty records. This repository pairs an automated Python web synchronizer—capable of harvesting Next.js client data bundles—with a fast, memory-safe C query engine for local search, departmental analytics, query auditing, and binary snapshot management.

---

## Repository Architecture

```text
university-faculty-tracker/
├── tracker.c              # Core C search engine, CLI dashboard, and binary persistence layer
├── sync_faculty.py        # Automated Next.js crawler and flat-file database generator
├── faculty_database.txt   # Pipe-delimited offline faculty dataset (Name|Designation|Cabin|School|Email)
├── tracker_audit.log      # Append-only timestamped audit trail of executed search queries
├── requirements.txt       # Python package dependencies for web synchronization
├── CHANGELOG.md           # Versioned release history and milestone notes
├── LICENSE                # MIT License (2026)
└── .gitignore             # Git exclusion rules for compiled binaries and local artifacts
```

### Component Breakdown

| File | Role |
| :--- | :--- |
| **`tracker.c`** | Ingests `faculty_database.txt` into dynamically resized heap memory (`malloc`/`realloc`), enforces `strncpy` buffer bounds safety, renders ASCII faculty cards, computes school-level analytics, logs queries, and exports/restores binary backups (`tracker_backup.bin`). |
| **`sync_faculty.py`** | Scrapes live Next.js script chunks from `https://vitap.ac.in/faculty`, sanitizes Unicode fields, resolves school acronyms, deduplicates profiles, and writes `faculty_database.txt`. |
| **`faculty_database.txt`** | Structured flat-file data store containing 5 pipe-delimited columns per faculty record. |
| **`tracker_audit.log`** | Persistent operational log recording query timestamps, query categories (`NAME_SEARCH`, `SCHOOL_FILTER`, `CABIN_SEARCH`), match counts, and search terms. |
| **`requirements.txt`** | Specifies Python packages (`requests`, `beautifulsoup4`, `urllib3`) for the synchronization environment. |
| **`CHANGELOG.md`** | Tracks release versions, starting with the `v1.0.0` initial release. |
| **`LICENSE`** | Standard MIT open-source license governing distribution and modification. |

---

## Getting Started

### 1. Synchronize Faculty Data (Optional)

If you need to refresh `faculty_database.txt` from the live university portal:

```bash
pip install -r requirements.txt
python sync_faculty.py
```

### 2. Compile and Run with GCC

#### Windows (MinGW / PowerShell / CMD)

```powershell
# Compile the C source into an executable
gcc tracker.c -o tracker

# Run the application
.\tracker.exe
```

#### Linux & macOS

```bash
# Compile the C source into an executable
gcc tracker.c -o tracker

# Run the application
./tracker
```

---

## License

Distributed under the **MIT License**. See [`LICENSE`](./LICENSE) for full license text.
