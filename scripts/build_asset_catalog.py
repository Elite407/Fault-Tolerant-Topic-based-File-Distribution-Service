"""
Build a comprehensive catalog of all 69 visual assets in Images/
Categorizes assets into:
1. Executive Terminal Proof-of-Work Cards (Dual-Pane)
2. Part 1 Empirical Client Distribution Tests
3. Part 2 Case 1 Unmanaged Fault & Wipe Tests
4. Part 2 Case 2 Stateful Session Resumption Tests
5. Part 2 Case 2 Enhanced Parallel Multi-TCP Streaming Tests
6. Miscellaneous Verification Captures
"""

import os
import glob
from PIL import Image

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
IMAGES_DIR = os.path.join(REPO_ROOT, 'Images')
RESULTS_DIR = os.path.join(REPO_ROOT, 'Results')
CATALOG_PATH = os.path.join(IMAGES_DIR, 'ASSETS_INDEX.md')

files = sorted(glob.glob(os.path.join(IMAGES_DIR, '*.png')))

categories = {
    "proof_cards": [],
    "part1": [],
    "part2_c1": [],
    "part2_c2": [],
    "part2_enh": [],
    "other": []
}

for f in files:
    name = os.path.basename(f)
    size_bytes = os.path.getsize(f)
    try:
        with Image.open(f) as img:
            dims = f"{img.size[0]}x{img.size[1]}"
    except Exception:
        dims = "unknown"

    item = {"name": name, "size": size_bytes, "dims": dims, "path": f"Images/{name}"}

    if name.startswith("terminal_"):
        categories["proof_cards"].append(item)
    elif "part1" in name.lower() or "c1_part1" in name.lower() or "c2_part1" in name.lower() or "c3_part1" in name.lower():
        categories["part1"].append(item)
    elif "part2_case1" in name.lower() or "c1_part2_case1" in name.lower() or "c2_part2_case1" in name.lower():
        categories["part2_c1"].append(item)
    elif "en" in name.lower() or "enhanced" in name.lower():
        categories["part2_enh"].append(item)
    elif "part2_case2" in name.lower() or "c1_part2_case2" in name.lower() or "c2_part2_case2" in name.lower():
        categories["part2_c2"].append(item)
    else:
        categories["other"].append(item)

# Build markdown
md = []
md.append("# Visual Asset Catalog & Curation Index")
md.append(f"**Total Visual Assets:** {len(files)} files in `Images/`\n")

md.append("## 1. Executive Terminal Proof Cards (Dual-Pane High-Resolution)")
md.append("These 6 cards provide crisp, dark-themed, publication-ready dual-pane views pairing server daemons with client subscribers.\n")
md.append("| File | Resolution | Size | Flow Description |")
md.append("|---|---|---|---|")
for it in categories["proof_cards"]:
    desc = {
        "terminal_broker_startup.png": "Server Daemon Startup, Topic Catalog Indexing (10 Topics), Socket Listen State",
        "terminal_part1_transfer.png": "Part 1 Basic Multi-Threaded Topic File Distribution (12 files, 52.6 MB/s)",
        "terminal_part2_case1_failover.png": "Part 2 Case 1 Fault Drop Detection, Recursive Directory Wipe & Restart",
        "terminal_part2_case2_session_resume.png": "Part 2 Case 2 Checkpoint Session Resumption from Byte Offset (Zero Corrupt/Redundant)",
        "terminal_part2_enhanced_parallel.png": "Part 2 Case 2 Enhanced 4-Worker Multi-TCP Parallel Streaming (5.2 MB/s, 4x Speedup)",
        "terminal_tests_verification.png": "Automated End-to-End Integration Test Suite 15/15 PASS Verification"
    }.get(it["name"], "Proof Card")
    md.append(f"| `Images/{it['name']}` | {it['dims']} | {it['size']//1024} KB | {desc} |")

md.append("\n## 2. Benchmark Visual Graphs (300 DPI Publication Plots)")
md.append("Generated in `Results/` from empirical sweep metrics.\n")
md.append("| File | Resolution | Description |")
md.append("|---|---|---|")
md.append("| `Results/Part1_throughput.png` | 2656x1619 | Multi-client throughput scalability curve |")
md.append("| `Results/Part2_Cases_throughput.png` | 2820x1619 | Case 1 vs Case 2 throughput degradation under fault injection |")
md.append("| `Results/Part2_Enhanced_throughput.png` | 2655x1619 | Concurrency throughput speedup vs parallel TCP worker connections |")

md.append(f"\n## 3. Part 1 Empirical Client Terminal Captures ({len(categories['part1'])} files)")
md.append("Empirical multi-client physical lab testing across 1, 2, and 3 concurrent client nodes.\n")
md.append("| File | Resolution | Size |")
md.append("|---|---|---|")
for it in categories["part1"]:
    md.append(f"| `Images/{it['name']}` | {it['dims']} | {it['size']//1024} KB |")

md.append(f"\n## 4. Part 2 Case 1 Empirical Terminal Captures ({len(categories['part2_c1'])} files)")
md.append("Empirical captures of unmanaged disconnect drops and directory wipe cycles under P in {0.05, 0.1, 0.5}.\n")
md.append("| File | Resolution | Size |")
md.append("|---|---|---|")
for it in categories["part2_c1"]:
    md.append(f"| `Images/{it['name']}` | {it['dims']} | {it['size']//1024} KB |")

md.append(f"\n## 5. Part 2 Case 2 Stateful Session Checkpointing Captures ({len(categories['part2_c2'])} files)")
md.append("Empirical captures of session resumption, byte offset tracking, and 0 redundant retransmissions.\n")
md.append("| File | Resolution | Size |")
md.append("|---|---|---|")
for it in categories["part2_c2"]:
    md.append(f"| `Images/{it['name']}` | {it['dims']} | {it['size']//1024} KB |")

md.append(f"\n## 6. Part 2 Case 2 Enhanced Parallel Streaming Captures ({len(categories['part2_enh'])} files)")
md.append("Empirical captures of 4 concurrent worker threads with modulo workload partitioning.\n")
md.append("| File | Resolution | Size |")
md.append("|---|---|---|")
for it in categories["part2_enh"]:
    md.append(f"| `Images/{it['name']}` | {it['dims']} | {it['size']//1024} KB |")

md.append(f"\n## 7. Miscellaneous Historical Verification Captures ({len(categories['other'])} files)")
md.append("| File | Resolution | Size |")
md.append("|---|---|---|")
for it in categories["other"]:
    md.append(f"| `Images/{it['name']}` | {it['dims']} | {it['size']//1024} KB |")

with open(CATALOG_PATH, "w", encoding="utf-8") as f:
    f.write("\n".join(md) + "\n")

print(f"[OK] Generated asset catalog: {CATALOG_PATH}")
