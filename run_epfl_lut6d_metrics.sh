#!/bin/bash

if [ -z "${BASH_VERSION:-}" ] || shopt -qo posix; then
    exec /bin/bash "$0" "$@"
fi

set -euo pipefail

ROOT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
YOSYS="$ROOT_DIR/yosys"
ABC="$ROOT_DIR/yosys-abc"
AIG_INPUT_DIR="$ROOT_DIR/epfl_benchmark"
OUT_DIR="$ROOT_DIR/test_output_epfl_ablation2"
LEVEL_TOOL="$ROOT_DIR/techlibs/yoyo/compute_lut_levels.py"

CONFIG_COUNT=0
USE_ABLATION_PRESET=0

add_config() {
    CONFIG_NAMES[$CONFIG_COUNT]="$1"
    CONFIG_ARGS[$CONFIG_COUNT]="$2"
    CONFIG_COUNT=$((CONFIG_COUNT + 1))
}

usage() {
    cat <<EOF
Usage: $0 [options]

Options:
  --aig-dir DIR              EPFL AIG directory. Default: $AIG_INPUT_DIR
  --out-dir DIR              Output directory. Default: $OUT_DIR
  --config "name|pass args"  Add one lut6d_map configuration. May be repeated.
  --preset ablation          Run a built-in ablation set.
  --help, -h                 Show this message.

Built-in ablation preset:
  full|<default>
  no_postpack|-no-postpack
  no_disjoint|-no-disjoint-mode
  no_area_recovery|-mapping-rounds 1 -area-recovery-rounds 0
  single_only|-covering-mode bestcut-single -no-postpack
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --aig-dir)
            AIG_INPUT_DIR="$2"
            shift 2
            ;;
        --out-dir)
            OUT_DIR="$2"
            shift 2
            ;;
        --config)
            entry="$2"
            name="${entry%%|*}"
            args=""
            if [ "$entry" != "$name" ]; then
                args="${entry#*|}"
            fi
            if [ -z "$name" ]; then
                echo "Error: invalid --config '$entry'" >&2
                exit 1
            fi
            add_config "$name" "$args"
            shift 2
            ;;
        --preset)
            if [ "$2" != "ablation" ]; then
                echo "Error: unknown preset '$2'" >&2
                exit 1
            fi
            USE_ABLATION_PRESET=1
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "Error: unknown argument '$1'" >&2
            usage
            exit 1
            ;;
    esac
done

if [ $CONFIG_COUNT -eq 0 ]; then
    if [ $USE_ABLATION_PRESET -eq 1 ]; then
        add_config "full" ""
        add_config "no_postpack" "-no-postpack"
        add_config "no_disjoint" "-no-disjoint-mode"
        add_config "no_area_recovery" "-mapping-rounds 1 -area-recovery-rounds 0"
        add_config "single_only" "-covering-mode bestcut-single -no-postpack"
    else
        add_config "full" ""
    fi
fi

if [ ! -x "$YOSYS" ]; then
    echo "Error: $YOSYS not found or not executable." >&2
    exit 1
fi
if [ ! -x "$ABC" ]; then
    echo "Error: $ABC not found or not executable." >&2
    exit 1
fi
if [ ! -f "$LEVEL_TOOL" ]; then
    echo "Error: level tool not found at $LEVEL_TOOL" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

get_aig_stats() {
    local aig_file="$1"
    local stats_text
    if ! stats_text="$("$ABC" -c "read_aiger $aig_file; ps" 2>/dev/null)"; then
        echo 0
        return
    fi
    echo "$stats_text" | awk '{
        for (i = 1; i <= NF; i++) {
            if ($i == "nd" && $(i+1) == "=") {
                print $(i+2);
                exit;
            }
            if ($i == "and" && $(i+1) == "=") {
                print $(i+2);
                exit;
            }
        }
    }' | tr -d ' '
}

get_lut_stat_summary() {
    local output_log="$1"
    awk '
        BEGIN {
            in_cell_table = 0
            total_cells = ""
            lut_cells = 0
            other_cells = 0
            other_types = ""
        }
        /^[[:space:]]*[0-9]+[[:space:]]+cells[[:space:]]*$/ {
            total_cells = $1
            lut_cells = 0
            other_cells = 0
            other_types = ""
            in_cell_table = 1
            next
        }
        in_cell_table && /^[[:space:]]*[0-9]+[[:space:]]+[A-Za-z0-9_$]+[[:space:]]*$/ {
            count = $1
            type = $2
            if (type ~ /^GTP_LUT[0-9]+D?$/) {
                lut_cells += count
            } else {
                other_cells += count
                other_types = other_types (other_types == "" ? "" : ";") type "=" count
            }
            next
        }
        in_cell_table && /^[[:space:]]*$/ { next }
        in_cell_table && !/^[[:space:]]*[0-9]+[[:space:]]+[A-Za-z0-9_$]+[[:space:]]*$/ {
            in_cell_table = 0
        }
        END {
            if (total_cells == "")
                total_cells = "NA"
            if (other_types == "")
                other_types = "-"
            print total_cells, lut_cells, other_cells, other_types
        }
    ' "$output_log"
}

timed_run() {
    local runtime_file="$1"
    local log_file="$2"
    shift 2
    TIMEFORMAT='%3R'
    { time "$@" >"$log_file" 2>&1; } 2>"$runtime_file"
}

run_abc_baseline() {
    local aig_file="$1"
    local out_v="$2"
    local log_file="$3"
    local runtime_file="$4"

    local status=0
    if timed_run "$runtime_file" "$log_file" "$ABC" -c "read_aiger $aig_file; ps; if -K 6; ps; write_verilog $out_v"; then
        status=0
    else
        status=$?
    fi

    if [ $status -ne 0 ]; then
        echo "$status,0,0,0,0"
        return
    fi

    local stats_line
    stats_line="$(grep "nd =" "$log_file" | tail -n 1 | tr -d '\r')"
    local num_nodes num_edges num_levels runtime_s
    num_nodes="$(echo "$stats_line" | awk '{for(i=1;i<=NF;i++) if($i=="nd" && $(i+1)=="=") {print $(i+2); exit}}' | tr -d ' ')"
    num_edges="$(echo "$stats_line" | awk '{for(i=1;i<=NF;i++) if($i=="edge" && $(i+1)=="=") {print $(i+2); exit}}' | tr -d ' ')"
    num_levels="$(echo "$stats_line" | awk '{for(i=1;i<=NF;i++) if($i=="lev" && $(i+1)=="=") {print $(i+2); exit}}' | tr -d ' ')"
    runtime_s="$(tr -d '[:space:]' < "$runtime_file")"

    echo "$status,${num_nodes:-0},${num_edges:-0},${num_levels:-0},${runtime_s:-0}"
}

run_lut6d_config() {
    local aig_file="$1"
    local pass_args="$2"
    local out_v="$3"
    local out_json="$4"
    local log_file="$5"
    local runtime_file="$6"

    local lut6d_cmd="lut6d_map"
    if [ -n "$pass_args" ]; then
        lut6d_cmd="$lut6d_cmd $pass_args"
    fi

    local status=0
    if timed_run "$runtime_file" "$log_file" "$YOSYS" -p "
        read_aiger $aig_file
        hierarchy -check -auto-top
        proc; opt; techmap; opt;
        abc -lut 6
        opt
        $lut6d_cmd
        clean
        stat
        write_verilog $out_v
        write_json $out_json
    "; then
        status=0
    else
        status=$?
    fi

    if [ $status -ne 0 ]; then
        echo "$status,0,0,0,0,0,0,0,0,0,0,0,-,FAILED"
        return
    fi

    local num_lut6d num_lut6 num_lut5d num_lut5 num_lut4d num_lut4 num_lut3 num_lut2
    num_lut6d="$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT6D[[:space:]]*$/ {print $1}' "$log_file" || true)"
    num_lut6="$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT6[[:space:]]*$/ {print $1}' "$log_file" || true)"
    num_lut5d="$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT5D[[:space:]]*$/ {print $1}' "$log_file" || true)"
    num_lut5="$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT5[[:space:]]*$/ {print $1}' "$log_file" || true)"
    num_lut4d="$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT4D[[:space:]]*$/ {print $1}' "$log_file" || true)"
    num_lut4="$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT4[[:space:]]*$/ {print $1}' "$log_file" || true)"
    num_lut3="$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT3[[:space:]]*$/ {print $1}' "$log_file" || true)"
    num_lut2="$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT2[[:space:]]*$/ {print $1}' "$log_file" || true)"

    num_lut6d="${num_lut6d:-0}"
    num_lut6="${num_lut6:-0}"
    num_lut5d="${num_lut5d:-0}"
    num_lut5="${num_lut5:-0}"
    num_lut4d="${num_lut4d:-0}"
    num_lut4="${num_lut4:-0}"
    num_lut3="${num_lut3:-0}"
    num_lut2="${num_lut2:-0}"

    local total_luts total_dual stat_summary lut_nodes stat_lut_cells non_lut_nodes non_lut_types
    total_luts=$((num_lut6d + num_lut6 + num_lut5d + num_lut5 + num_lut4d + num_lut4 + num_lut3 + num_lut2))
    total_dual=$((num_lut6d + num_lut5d + num_lut4d))

    stat_summary="$(get_lut_stat_summary "$log_file")"
    lut_nodes="$(echo "$stat_summary" | awk '{print $1}')"
    stat_lut_cells="$(echo "$stat_summary" | awk '{print $2}')"
    non_lut_nodes="$(echo "$stat_summary" | awk '{print $3}')"
    non_lut_types="$(echo "$stat_summary" | awk '{print $4}')"

    if [ "$lut_nodes" = "NA" ]; then
        echo "$status,0,$total_luts,$total_dual,$num_lut6d,$num_lut6,$num_lut5d,$num_lut5,$num_lut4d,$num_lut4,$num_lut3,$num_lut2,-,STAT_PARSE_ERROR"
        return
    fi

    if [ "$lut_nodes" -ne $((total_luts + non_lut_nodes)) ] || [ "$stat_lut_cells" -ne "$total_luts" ]; then
        echo "$status,$lut_nodes,$total_luts,$total_dual,$num_lut6d,$num_lut6,$num_lut5d,$num_lut5,$num_lut4d,$num_lut4,$num_lut3,$num_lut2,$non_lut_types,STAT_PARSE_ERROR"
        return
    fi

    if [ "$non_lut_nodes" -ne 0 ]; then
        echo "$status,$lut_nodes,$total_luts,$total_dual,$num_lut6d,$num_lut6,$num_lut5d,$num_lut5,$num_lut4d,$num_lut4,$num_lut3,$num_lut2,$non_lut_types,NON_LUT_NODES"
        return
    fi

    echo "$status,$lut_nodes,$total_luts,$total_dual,$num_lut6d,$num_lut6,$num_lut5d,$num_lut5,$num_lut4d,$num_lut4,$num_lut3,$num_lut2,-,OK"
}

LIST_FILE="$OUT_DIR/test_list.csv"
SORTED_LIST="$OUT_DIR/test_list_sorted.csv"
COMBINED_CSV="$OUT_DIR/all_results.csv"
SUMMARY_CSV="$OUT_DIR/summary_by_config.csv"

printf 'Config,Pass_Args,Benchmark,Size_Nodes,ABC_Nodes,ABC_Edges,ABC_Levels,ABC_Runtime_s,Ours_Nodes,Ours_Levels,Ours_Runtime_s,Picked_Mode,Total_LUTs,Total_Dual,LUT6D,LUT6,LUT5D,LUT5,LUT4D,LUT4,LUT3,LUT2,Status\n' >"$COMBINED_CSV"
: >"$LIST_FILE"

for ((idx = 0; idx < CONFIG_COUNT; idx++)); do
    cfg_dir="$OUT_DIR/${CONFIG_NAMES[$idx]}"
    mkdir -p "$cfg_dir"
    printf 'Benchmark,Size_Nodes,ABC_Nodes,ABC_Edges,ABC_Levels,ABC_Runtime_s,Ours_Nodes,Ours_Levels,Ours_Runtime_s,Picked_Mode,Total_LUTs,Total_Dual,LUT6D,LUT6,LUT5D,LUT5,LUT4D,LUT4,LUT3,LUT2,Status\n' >"$cfg_dir/results.csv"
done

shopt -s nullglob
for aig_path in "$AIG_INPUT_DIR"/*.aig; do
    bench="$(basename "$aig_path" .aig)"
    size_nodes="$(get_aig_stats "$aig_path")"
    printf '%s,%s,%s\n' "$size_nodes" "$bench" "$aig_path" >>"$LIST_FILE"
done
shopt -u nullglob

if [ ! -s "$LIST_FILE" ]; then
    echo "Error: no AIG files found in $AIG_INPUT_DIR" >&2
    exit 1
fi

sort -t, -k1,1n -k2,2 "$LIST_FILE" >"$SORTED_LIST"

echo "========================================"
echo "  EPFL LUT6D Metrics Runner"
echo "========================================"
echo "AIG directory: $AIG_INPUT_DIR"
echo "Output dir:    $OUT_DIR"
echo "Configs:"
for ((idx = 0; idx < CONFIG_COUNT; idx++)); do
    echo "  - ${CONFIG_NAMES[$idx]} : ${CONFIG_ARGS[$idx]:-(default)}"
done
echo ""

while IFS=',' read -r size_nodes bench aig_path; do
    echo "----------------------------------------"
    echo "Benchmark: $bench"
    echo "AIG nodes: $size_nodes"

    base_dir="$OUT_DIR/_baseline"
    mkdir -p "$base_dir"
    abc_v="$base_dir/${bench}_abc_mapped.v"
    abc_log="$base_dir/${bench}_abc.log"
    abc_time="$base_dir/${bench}_abc.time"

    abc_result="$(run_abc_baseline "$aig_path" "$abc_v" "$abc_log" "$abc_time")"
    abc_status="$(echo "$abc_result" | cut -d',' -f1)"
    abc_nodes="$(echo "$abc_result" | cut -d',' -f2)"
    abc_edges="$(echo "$abc_result" | cut -d',' -f3)"
    abc_levels="$(echo "$abc_result" | cut -d',' -f4)"
    abc_runtime="$(echo "$abc_result" | cut -d',' -f5)"

    if [ "$abc_status" -ne 0 ]; then
        echo "  ABC baseline failed, skipping benchmark."
        for ((idx = 0; idx < CONFIG_COUNT; idx++)); do
            cfg_dir="$OUT_DIR/${CONFIG_NAMES[$idx]}"
            printf '%s,%s,FAILED_ABC,,,,,,,,,,,,,,,,,,\n' "$bench" "$size_nodes" >>"$cfg_dir/results.csv"
            printf '%s,"%s",%s,%s,FAILED_ABC,,,,,,,,,,,,,,,,,,\n' \
                "${CONFIG_NAMES[$idx]}" "${CONFIG_ARGS[$idx]}" "$bench" "$size_nodes" >>"$COMBINED_CSV"
        done
        echo ""
        continue
    fi

    echo "  ABC:  nodes=$abc_nodes levels=$abc_levels runtime=${abc_runtime}s"

    for ((idx = 0; idx < CONFIG_COUNT; idx++)); do
        cfg_name="${CONFIG_NAMES[$idx]}"
        cfg_args="${CONFIG_ARGS[$idx]}"
        cfg_dir="$OUT_DIR/$cfg_name"
        ours_v="$cfg_dir/${bench}_mapped.v"
        ours_json="$cfg_dir/${bench}_mapped.json"
        ours_log="$cfg_dir/${bench}.log"
        ours_time="$cfg_dir/${bench}.time"

        echo "  [$cfg_name] lut6d_map ${cfg_args:-<default>}"
        ours_result="$(run_lut6d_config "$aig_path" "$cfg_args" "$ours_v" "$ours_json" "$ours_log" "$ours_time")"
        ours_status_code="$(echo "$ours_result" | cut -d',' -f1)"
        ours_nodes="$(echo "$ours_result" | cut -d',' -f2)"
        total_luts="$(echo "$ours_result" | cut -d',' -f3)"
        total_dual="$(echo "$ours_result" | cut -d',' -f4)"
        num_lut6d="$(echo "$ours_result" | cut -d',' -f5)"
        num_lut6="$(echo "$ours_result" | cut -d',' -f6)"
        num_lut5d="$(echo "$ours_result" | cut -d',' -f7)"
        num_lut5="$(echo "$ours_result" | cut -d',' -f8)"
        num_lut4d="$(echo "$ours_result" | cut -d',' -f9)"
        num_lut4="$(echo "$ours_result" | cut -d',' -f10)"
        num_lut3="$(echo "$ours_result" | cut -d',' -f11)"
        num_lut2="$(echo "$ours_result" | cut -d',' -f12)"
        aux_info="$(echo "$ours_result" | cut -d',' -f13)"
        status_text="$(echo "$ours_result" | cut -d',' -f14)"

        ours_levels=""
        ours_runtime=""
        picked_mode=""
        final_status="$status_text"

        if [ "$ours_status_code" -eq 0 ] && [ "$status_text" = "OK" ]; then
            ours_runtime="$(tr -d '[:space:]' < "$ours_time")"
            picked_mode="$(sed -n 's/.*picked=\([^ ]*\) (.*/\1/p' "$ours_log" | tail -n 1)"
            if ! ours_levels="$(python3 "$LEVEL_TOOL" "$ours_json" 2>/dev/null)"; then
                ours_levels=""
                final_status="LEVEL_ERROR"
            fi
        fi

        echo "    status=$final_status nodes=${ours_nodes:-NA} levels=${ours_levels:-NA} runtime=${ours_runtime:-NA}s picked=${picked_mode:-NA}"

        printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
            "$bench" "$size_nodes" "$abc_nodes" "$abc_edges" "$abc_levels" "$abc_runtime" \
            "${ours_nodes:-}" "${ours_levels:-}" "${ours_runtime:-}" "${picked_mode:-}" \
            "$total_luts" "$total_dual" "$num_lut6d" "$num_lut6" "$num_lut5d" "$num_lut5" \
            "$num_lut4d" "$num_lut4" "$num_lut3" "$num_lut2" "$final_status" >>"$cfg_dir/results.csv"

        printf '%s,"%s",%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
            "$cfg_name" "$cfg_args" "$bench" "$size_nodes" "$abc_nodes" "$abc_edges" "$abc_levels" "$abc_runtime" \
            "${ours_nodes:-}" "${ours_levels:-}" "${ours_runtime:-}" "${picked_mode:-}" \
            "$total_luts" "$total_dual" "$num_lut6d" "$num_lut6" "$num_lut5d" "$num_lut5" \
            "$num_lut4d" "$num_lut4" "$num_lut3" "$num_lut2" "$final_status" >>"$COMBINED_CSV"
    done
    echo ""
done <"$SORTED_LIST"

printf 'Config,Benchmarks,Mean_Ours_Nodes,Mean_Ours_Levels,Mean_Ours_Runtime_s,GeoMean_Node_Ratio_vs_ABC,GeoMean_Level_Ratio_vs_ABC\n' >"$SUMMARY_CSV"
for ((idx = 0; idx < CONFIG_COUNT; idx++)); do
    cfg_name="${CONFIG_NAMES[$idx]}"
    cfg_dir="$OUT_DIR/$cfg_name"
    summary_line="$(
        awk -F, '
            NR == 1 { next }
            $21 == "OK" && $7 != "" && $8 != "" && $9 != "" {
                count++
                sum_nodes += $7 + 0
                sum_levels += $8 + 0
                sum_runtime += $9 + 0
                if (($3 + 0) > 0 && ($7 + 0) > 0)
                    log_node += log(($7 + 0) / ($3 + 0))
                if (($5 + 0) > 0 && ($8 + 0) > 0)
                    log_level += log(($8 + 0) / ($5 + 0))
            }
            END {
                if (count == 0) {
                    printf "0,,,,,\n"
                    exit
                }
                printf "%d,%.3f,%.3f,%.3f,%.6f,%.6f\n",
                    count,
                    sum_nodes / count,
                    sum_levels / count,
                    sum_runtime / count,
                    exp(log_node / count),
                    exp(log_level / count)
            }
        ' "$cfg_dir/results.csv"
    )"
    printf '%s,%s\n' "$cfg_name" "$summary_line" >>"$SUMMARY_CSV"
done

echo "Combined CSV: $COMBINED_CSV"
echo "Summary CSV:  $SUMMARY_CSV"
