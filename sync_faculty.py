#!/usr/bin/env python3
"""
=============================================================================
UNIVERSITY FACULTY DIRECTORY SYNCHRONIZER & CRAWLER
=============================================================================
Target: VIT-AP University (https://vitap.ac.in)
Layer: Automated Live Crawler & Synchronizer (Execution Layer)

Description:
  Automates data ingestion from the institutional Next.js dynamic client
  application. Resolves all asynchronous data bundles rendered in the client
  DOM, extracts 5 exact professional metadata columns:
    [Full Name], [Designation], [Cabin Number], [School Association], [Email]
  Saves the complete verified faculty directory into 'faculty_database.txt'
  formatted as: Name|Designation|Cabin|School|Email\n
=============================================================================
"""

import sys
import re
import json
import urllib.request
import os

HEADERS = {
    'User-Agent': (
        'Mozilla/5.0 (Windows NT 10.0; Win64; x64) '
        'AppleWebKit/537.36 (KHTML, like Gecko) '
        'Chrome/124.0.0.0 Safari/537.36'
    ),
    'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
    'Accept-Language': 'en-US,en;q=0.5',
}

OUTPUT_FILE = "faculty_database.txt"

def fetch_url(url, timeout=30):
    """Safely fetch response from remote URL with standard browser headers."""
    req = urllib.request.Request(url, headers=HEADERS)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read().decode('utf-8', errors='ignore')

def clean_field(val, default='N/A'):
    """Sanitize strings: strip whitespace, remove pipes, replace unicode dashes, ensure clean ASCII."""
    if val is None:
        return default
    s = str(val).strip()
    # Normalize unicode hyphens/dashes and quotes
    s = s.replace('\u2013', '-').replace('\u2014', '-').replace('\u2212', '-')
    s = s.replace('\u2018', "'").replace('\u2019', "'").replace('\u201c', '"').replace('\u201d', '"')
    s = s.replace('|', '/').replace('\r', ' ').replace('\n', ' ')
    # Encode to ASCII, replacing unknown characters with space
    s = s.encode('ascii', errors='replace').decode('ascii')
    s = s.replace('?', ' ')
    s = re.sub(r'\s+', ' ', s).strip()
    return s if s else default

def extract_school_acronym(department_str):
    """Normalize department string to its standard school acronym (SCOPE, SENSE, etc.)."""
    if not department_str:
        return 'N/A'
    dept = department_str.strip()
    # Check parenthetical acronym e.g., "School of Computer Science and Engineering (SCOPE)"
    match = re.search(r'\(([^)]+)\)', dept)
    if match:
        acronym = match.group(1).strip().upper()
        if acronym in {'SCOPE', 'SENSE', 'SAS', 'SMEC', 'VSB', 'VSL', 'VISH', 'SBST'}:
            return acronym
        return acronym
    
    # Fallback keyword checks
    dept_upper = dept.upper()
    if 'COMPUTER' in dept_upper or 'SCOPE' in dept_upper:
        return 'SCOPE'
    if 'ELECTRONIC' in dept_upper or 'SENSE' in dept_upper:
        return 'SENSE'
    if 'ADVANCED SCIENCE' in dept_upper or 'SAS' in dept_upper:
        return 'SAS'
    if 'MECHANICAL' in dept_upper or 'SMEC' in dept_upper:
        return 'SMEC'
    if 'BUSINESS' in dept_upper or 'VSB' in dept_upper:
        return 'VSB'
    if 'LAW' in dept_upper or 'VSL' in dept_upper:
        return 'VSL'
    if 'HUMANITIES' in dept_upper or 'SOCIAL' in dept_upper or 'VISH' in dept_upper:
        return 'VISH'
    if 'BIO' in dept_upper or 'SBST' in dept_upper:
        return 'SBST'
    return clean_field(dept)

def discover_faculty_chunks():
    """Scrapes the live HTML of /faculty to locate exact dynamic JS script chunks."""
    base_url = "https://vitap.ac.in"
    faculty_page_url = f"{base_url}/faculty"
    print(f"[*] Navigating institutional endpoint: {faculty_page_url}")
    html = fetch_url(faculty_page_url)
    
    scripts = re.findall(r'<script[^>]*src=["\']([^"\']+)["\']', html)
    print(f"[*] Located {len(scripts)} dynamic script tags in rendered DOM.")
    
    chunk_8525 = None
    chunk_8693 = None
    
    for s in scripts:
        if '8525-' in s:
            chunk_8525 = f"{base_url}{s}" if s.startswith('/') else s
        elif '8693-' in s:
            chunk_8693 = f"{base_url}{s}" if s.startswith('/') else s
            
    # Fallback to known production hash URLs if dynamic hashes changed
    if not chunk_8525:
        # scan scripts for module 45946
        print("[*] Probing script chunks for faculty data module...")
        for s in scripts:
            full_s = f"{base_url}{s}" if s.startswith('/') else s
            try:
                content = fetch_url(full_s, timeout=10)
                if '45946:' in content:
                    chunk_8525 = full_s
                    break
            except Exception:
                continue

    return chunk_8525, chunk_8693

def crawl_and_sync():
    """Extracts, parses, validates, and serializes the complete faculty database."""
    print("=" * 70)
    print("VIT-AP UNIVERSITY FACULTY DIRECTORY HARVESTER & SYNCHRONIZER")
    print("=" * 70)
    
    chunk_main, chunk_sbst = discover_faculty_chunks()
    
    if not chunk_main:
        print("[!] Fatal: Could not resolve main faculty JavaScript chunk.")
        sys.exit(1)
        
    print(f"[*] Found Primary Data Chunk: {chunk_main}")
    main_js = fetch_url(chunk_main)
    print(f"[*] Successfully downloaded Primary Chunk ({len(main_js):,} bytes).")
    
    all_faculty = []
    seen_keys = set()
    
    # 1. Parse JSON payload inside primary chunk
    start_tag = "JSON.parse('"
    s_idx = main_js.find(start_tag)
    if s_idx != -1:
        s_idx += len(start_tag)
        # Search for closing delimiter
        e_idx = main_js.find("'),n=", s_idx)
        if e_idx == -1:
            e_idx = main_js.find("')\n", s_idx)
        if e_idx == -1:
            e_idx = main_js.find("')", s_idx)
            
        raw_json_str = main_js[s_idx:e_idx]
        unescaped = raw_json_str.encode('utf-8').decode('unicode_escape')
        parsed_data = json.loads(unescaped)
        
        raw_items = parsed_data.get('a', [])
        print(f"[*] Extracted {len(raw_items)} profiles from primary data bundle.")
        
        for item in raw_items:
            attrs = item.get('attributes', item)
            name = clean_field(attrs.get('Name'))
            desig = clean_field(attrs.get('Designation'), 'Faculty')
            cabin = clean_field(attrs.get('Office_Address'), 'N/A')
            school = extract_school_acronym(attrs.get('Department'))
            email = clean_field(attrs.get('EMAIL'), 'N/A')
            
            if name and name != 'N/A':
                # Dedup key
                key = (name.lower(), email.lower())
                if key not in seen_keys:
                    seen_keys.add(key)
                    all_faculty.append({
                        'name': name,
                        'designation': desig,
                        'cabin': cabin,
                        'school': school,
                        'email': email
                    })
    
    # 2. Parse secondary chunk for SBST faculty if available
    if chunk_sbst:
        print(f"[*] Found Secondary Data Chunk (SBST): {chunk_sbst}")
        try:
            sbst_js = fetch_url(chunk_sbst)
            start_r = sbst_js.find("r=[{")
            if start_r != -1:
                r_str = sbst_js[start_r:]
                items = r_str.split("{id:")
                sbst_count = 0
                for it in items[1:]:
                    name_m = re.search(r'Name:\s*"([^"]+)"', it)
                    desig_m = re.search(r'Designation:\s*"([^"]*)"', it)
                    dept_m = re.search(r'Department:\s*"([^"]*)"', it)
                    email_m = re.search(r'EMAIL:\s*"([^"]*)"', it)
                    cabin_m = re.search(r'Office_Address:\s*"([^"]*)"', it)
                    
                    if name_m:
                        name = clean_field(name_m.group(1))
                        desig = clean_field(desig_m.group(1) if desig_m else '', 'Faculty')
                        school = extract_school_acronym(dept_m.group(1) if dept_m else 'SBST')
                        email = clean_field(email_m.group(1) if email_m else '', 'N/A')
                        cabin = clean_field(cabin_m.group(1) if cabin_m else '', 'N/A')
                        
                        key = (name.lower(), email.lower())
                        if key not in seen_keys and name != 'N/A':
                            seen_keys.add(key)
                            all_faculty.append({
                                'name': name,
                                'designation': desig,
                                'cabin': cabin,
                                'school': school,
                                'email': email
                            })
                            sbst_count += 1
                print(f"[*] Extracted {sbst_count} profiles from secondary data bundle.")
        except Exception as e:
            print(f"[-] Warning reading secondary chunk: {e}")

    # Write output flat-file
    total_records = len(all_faculty)
    print(f"[*] Serializing {total_records} faculty profiles to '{OUTPUT_FILE}'...")
    
    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        for fac in all_faculty:
            line = f"{fac['name']}|{fac['designation']}|{fac['cabin']}|{fac['school']}|{fac['email']}\n"
            f.write(line)
            
    print(f"[+] Successfully generated '{OUTPUT_FILE}' with {total_records} records.")
    
    # Statistical Summary
    school_breakdown = {}
    for fac in all_faculty:
        s = fac['school']
        school_breakdown[s] = school_breakdown.get(s, 0) + 1
        
    print("\n" + "=" * 70)
    print("SYNCHRONIZATION METRICS SUMMARY")
    print("=" * 70)
    print(f"Total Faculty Profiles Synchronized : {total_records}")
    print("Distribution by School:")
    for s, c in sorted(school_breakdown.items(), key=lambda x: -x[1]):
        print(f"  - {s:<10} : {c:>3} faculty members")
    print("=" * 70)

if __name__ == '__main__':
    crawl_and_sync()
