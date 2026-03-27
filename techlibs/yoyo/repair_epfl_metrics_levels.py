#!/usr/bin/env python3

import csv
import math
import subprocess
import sys
from pathlib import Path


COMBINED_FIELDS = [
    "Config",
    "Pass_Args",
    "Benchmark",
    "Size_Nodes",
    "ABC_Nodes",
    "ABC_Edges",
    "ABC_Levels",
    "ABC_Runtime_s",
    "Ours_Nodes",
    "Ours_Levels",
    "Ours_Runtime_s",
    "Picked_Mode",
    "Total_LUTs",
    "Total_Dual",
    "LUT6D",
    "LUT6",
    "LUT5D",
    "LUT5",
    "LUT4D",
    "LUT4",
    "LUT3",
    "LUT2",
    "Status",
]

CONFIG_FIELDS = [
    "Benchmark",
    "Size_Nodes",
    "ABC_Nodes",
    "ABC_Edges",
    "ABC_Levels",
    "ABC_Runtime_s",
    "Ours_Nodes",
    "Ours_Levels",
    "Ours_Runtime_s",
    "Picked_Mode",
    "Total_LUTs",
    "Total_Dual",
    "LUT6D",
    "LUT6",
    "LUT5D",
    "LUT5",
    "LUT4D",
    "LUT4",
    "LUT3",
    "LUT2",
    "Status",
]

SUMMARY_FIELDS = [
    "Config",
    "Benchmarks",
    "Mean_Ours_Nodes",
    "Mean_Ours_Levels",
    "Mean_Ours_Runtime_s",
    "GeoMean_Node_Ratio_vs_ABC",
    "GeoMean_Level_Ratio_vs_ABC",
]


def compute_level(level_tool: Path, json_file: Path) -> str:
    return subprocess.check_output(
        [sys.executable, str(level_tool), str(json_file)],
        text=True,
    ).strip()


def load_pass_args(combined_csv: Path):
    pass_args = {}
    if not combined_csv.exists():
        return pass_args
    with combined_csv.open("r", newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            pass_args.setdefault(row["Config"], row.get("Pass_Args", ""))
    return pass_args


def write_csv(path: Path, fieldnames, rows):
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def repair_config_results(results_dir: Path, level_tool: Path):
    repaired = 0
    config_rows = {}
    for config_dir in sorted(p for p in results_dir.iterdir() if p.is_dir()):
        results_csv = config_dir / "results.csv"
        if not results_csv.exists():
            continue

        rows = []
        with results_csv.open("r", newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                json_file = config_dir / f"{row['Benchmark']}_mapped.json"
                if json_file.exists():
                    try:
                        level = compute_level(level_tool, json_file)
                    except subprocess.CalledProcessError:
                        level = ""
                    if level:
                        if row.get("Ours_Levels") != level:
                            repaired += 1
                        row["Ours_Levels"] = level
                        if row.get("Status") == "LEVEL_ERROR":
                            row["Status"] = "OK"
                rows.append(row)

        write_csv(results_csv, CONFIG_FIELDS, rows)
        config_rows[config_dir.name] = rows
    return repaired, config_rows


def rebuild_combined(results_root: Path, pass_args_map, config_rows):
    rows = []
    for config_name in sorted(config_rows):
        pass_args = pass_args_map.get(config_name, "")
        for row in config_rows[config_name]:
            combined = {"Config": config_name, "Pass_Args": pass_args}
            combined.update(row)
            rows.append(combined)
    write_csv(results_root / "all_results.csv", COMBINED_FIELDS, rows)


def rebuild_summary(results_root: Path, config_rows):
    rows = []
    for config_name in sorted(config_rows):
        ok_rows = []
        for row in config_rows[config_name]:
            if row.get("Status") != "OK":
                continue
            if not row.get("Ours_Nodes") or not row.get("Ours_Levels") or not row.get("Ours_Runtime_s"):
                continue
            ok_rows.append(row)

        if not ok_rows:
            rows.append(
                {
                    "Config": config_name,
                    "Benchmarks": "0",
                    "Mean_Ours_Nodes": "",
                    "Mean_Ours_Levels": "",
                    "Mean_Ours_Runtime_s": "",
                    "GeoMean_Node_Ratio_vs_ABC": "",
                    "GeoMean_Level_Ratio_vs_ABC": "",
                }
            )
            continue

        mean_nodes = sum(float(r["Ours_Nodes"]) for r in ok_rows) / len(ok_rows)
        mean_levels = sum(float(r["Ours_Levels"]) for r in ok_rows) / len(ok_rows)
        mean_runtime = sum(float(r["Ours_Runtime_s"]) for r in ok_rows) / len(ok_rows)

        node_logs = [
            math.log(float(r["Ours_Nodes"]) / float(r["ABC_Nodes"]))
            for r in ok_rows
            if float(r["ABC_Nodes"]) > 0 and float(r["Ours_Nodes"]) > 0
        ]
        level_logs = [
            math.log(float(r["Ours_Levels"]) / float(r["ABC_Levels"]))
            for r in ok_rows
            if float(r["ABC_Levels"]) > 0 and float(r["Ours_Levels"]) > 0
        ]

        rows.append(
            {
                "Config": config_name,
                "Benchmarks": str(len(ok_rows)),
                "Mean_Ours_Nodes": f"{mean_nodes:.3f}",
                "Mean_Ours_Levels": f"{mean_levels:.3f}",
                "Mean_Ours_Runtime_s": f"{mean_runtime:.3f}",
                "GeoMean_Node_Ratio_vs_ABC": f"{math.exp(sum(node_logs) / len(node_logs)):.6f}" if node_logs else "",
                "GeoMean_Level_Ratio_vs_ABC": f"{math.exp(sum(level_logs) / len(level_logs)):.6f}" if level_logs else "",
            }
        )

    write_csv(results_root / "summary_by_config.csv", SUMMARY_FIELDS, rows)


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <metrics_output_dir>", file=sys.stderr)
        return 2

    results_root = Path(sys.argv[1]).resolve()
    if not results_root.is_dir():
        print(f"Metrics output directory not found: {results_root}", file=sys.stderr)
        return 1

    level_tool = Path(__file__).with_name("compute_lut_levels.py")
    if not level_tool.exists():
        print(f"Level tool not found: {level_tool}", file=sys.stderr)
        return 1

    pass_args_map = load_pass_args(results_root / "all_results.csv")
    repaired, config_rows = repair_config_results(results_root, level_tool)
    rebuild_combined(results_root, pass_args_map, config_rows)
    rebuild_summary(results_root, config_rows)

    print(f"Repaired level fields: {repaired}")
    print(f"Updated: {results_root / 'all_results.csv'}")
    print(f"Updated: {results_root / 'summary_by_config.csv'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
