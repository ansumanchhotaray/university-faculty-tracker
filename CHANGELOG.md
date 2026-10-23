# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-10-24

### Added
- **Next.js Directory Crawler (`sync_faculty.py`)**: Automated harvester that locates and extracts faculty metadata from VIT-AP University's client-side JavaScript chunks and serializes normalized records into `faculty_database.txt`.
- **Local C Search Engine (`tracker.c`)**: High-speed, zero-dependency offline terminal search engine featuring dynamic heap resizing (`malloc`/`realloc`), `strncpy`-guarded string handling, and ASCII profile card rendering.
- **Multi-Mode Search & Filtering**: Case-insensitive, multi-word search by faculty name, school acronym filtering (`SCOPE`, `SENSE`, `SAS`, `SMEC`, `VSB`, `VSL`, `VISH`, `SBST`), and cabin/building location lookup.
- **Directory Analytics & Persistence**: Real-time departmental distribution statistics, append-only query audit logging (`tracker_audit.log`), and binary snapshot export/restore (`tracker_backup.bin`).
