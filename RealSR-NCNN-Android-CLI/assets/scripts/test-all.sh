#!/bin/bash
# Test script for all CLI programs on Linux
# Usage: test-all.sh [options] [program_name]
#
# Options:
#   --no-html    Do not generate HTML report
#   -h, --help   Show this help message
#
# Examples:
#   test-all.sh                     Test all programs
#   test-all.sh resize-ncnn         Test only resize-ncnn
#   test-all.sh --no-html           Test all without HTML report

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

BIN_DIR="$ROOT_DIR/Linux-x64"
INPUT_DIR="$ROOT_DIR/input"
OUTPUT_DIR="$ROOT_DIR/output"
REPORT_DIR="$ROOT_DIR/report"

total_param_groups=0
total_passed_tests=0
total_failed_tests=0

# ======================== Parse arguments ========================
target_program=""
generate_html=1

for arg in "$@"; do
    case "$arg" in
        -h|--help)
            echo "Usage:"
            echo "  test-all.sh [options] [program_name]"
            echo ""
            echo "Options:"
            echo "  --no-html    Do not generate HTML report"
            echo "  -h, --help   Show this help message"
            echo ""
            echo "Programs:"
            echo "  resize-ncnn  realcugan-ncnn  realsr-ncnn  srmd-ncnn  waifu2x-ncnn  mnnsr-ncnn"
            echo ""
            echo "Examples:"
            echo "  test-all.sh                     Test all programs"
            echo "  test-all.sh resize-ncnn         Test only resize-ncnn"
            echo "  test-all.sh --no-html mnnsr-ncnn  Test mnnsr without HTML report"
            exit 0
            ;;
        --no-html)
            generate_html=0
            ;;
        resize-ncnn|realcugan-ncnn|realsr-ncnn|srmd-ncnn|waifu2x-ncnn|mnnsr-ncnn)
            target_program="$arg"
            ;;
    esac
done

# ======================== Validate directories ========================
if [[ ! -d "$BIN_DIR" ]]; then
    echo "Error: Binary directory not found at: $BIN_DIR"
    exit 1
fi

if [[ ! -d "$INPUT_DIR" ]]; then
    echo "Error: Input directory not found at: $INPUT_DIR"
    exit 1
fi

file_count=$(find "$INPUT_DIR" -maxdepth 1 -type f | wc -l)
if [[ "$file_count" -eq 0 ]]; then
    echo "Error: No files in input directory $INPUT_DIR"
    exit 1
fi

# ======================== Prepare output & report ========================
echo "Cleaning output directory..."
mkdir -p "$OUTPUT_DIR"
rm -f "$OUTPUT_DIR"/*

mkdir -p "$REPORT_DIR"

timestamp=$(date '+%Y%m%d_%H%M%S')
if [[ -z "$target_program" ]]; then
    csv_prog="all"
else
    csv_prog="${target_program%-ncnn}"
fi
CSV_FILE="$REPORT_DIR/test_${csv_prog}_${timestamp}.csv"

# Write CSV header
echo "input_filename,program_name,param_group,params,output_filename" > "$CSV_FILE"

# ======================== Helper functions ========================

run_test() {
    local program="$1"
    local params="$2"
    local index="$3"
    local test_passed=0

    echo "Test parameter group $index: $params"
    ((total_param_groups++))

    local prog_short="${program%-ncnn}"

    for input_file in "$INPUT_DIR"/*; do
        [[ -f "$input_file" ]] || continue

        local input_name=$(basename "$input_file" | sed 's/\.[^.]*$//')
        local input_ext=$(basename "$input_file" | sed 's/.*\(\.[^.]*\)$/\1/')

        # Parse params to extract model name, scale, denoise
        local model_name=""
        local scale_suffix=""
        local denoise_suffix=""
        local prev_token=""
        local tokens=($params)

        for token in "${tokens[@]}"; do
            if [[ "$prev_token" == "m" ]]; then
                model_name=$(basename "$token")
                prev_token=""
            elif [[ "$prev_token" == "s" ]]; then
                scale_suffix="_x${token}"
                prev_token=""
            elif [[ "$prev_token" == "n" ]]; then
                denoise_suffix="_n${token}"
                prev_token=""
            elif [[ "$token" == "-m" ]]; then
                prev_token="m"
            elif [[ "$token" == "-s" ]]; then
                prev_token="s"
            elif [[ "$token" == "-n" ]]; then
                prev_token="n"
            else
                prev_token=""
            fi
        done

        local output_name="${prog_short}_${model_name}${scale_suffix}${denoise_suffix}_${input_name}${input_ext}"
        local output_file="$OUTPUT_DIR/$output_name"

        rm -f "$output_file"

        echo "  Processing: ${input_name}${input_ext}"
        "$BIN_DIR/$program" $params -i "$input_file" -o "$output_file"

        if [[ -f "$output_file" ]] && [[ -s "$output_file" ]]; then
            test_passed=1
            echo -e "  \033[32m[OK] Test succeeded: $output_name\033[0m"
            echo "${input_name}${input_ext},${prog_short},${index},${params},${output_name}" >> "$CSV_FILE"
        else
            echo -e "  \033[31m[FAIL] No output generated: $output_name\033[0m"
            echo "${input_name}${input_ext},${prog_short},${index},${params}," >> "$CSV_FILE"
        fi
    done

    echo ""
    if [[ "$test_passed" -eq 1 ]]; then
        ((total_passed_tests++))
    else
        ((total_failed_tests++))
    fi
}

# ======================== Test blocks ========================

block_resize() {
    local prog="resize-ncnn"
    if [[ ! -x "$BIN_DIR/$prog" ]]; then
        echo "Program $prog does not exist, skipping."
        return
    fi
    echo "Testing program: $prog"
    run_test "$prog" "-m bicubic -s 2" 1
    run_test "$prog" "-m bilinear -s 2" 2
    run_test "$prog" "-m nearest -s 2" 3
    run_test "$prog" "-m avir -s 2" 4
    run_test "$prog" "-m avir-lancir -s 2" 5
    run_test "$prog" "-m avir -s 0.5" 6
    run_test "$prog" "-m de-nearest -s 2" 7
    run_test "$prog" "-m de-nearest2 -s 2" 8
    run_test "$prog" "-m de-nearest3 -s 2" 9
    run_test "$prog" "-m perfectpixel -s 0" 10
    echo ""
}

block_realcugan() {
    local prog="realcugan-ncnn"
    if [[ ! -x "$BIN_DIR/$prog" ]]; then
        echo "Program $prog does not exist, skipping."
        return
    fi
    echo "Testing program: $prog"
    run_test "$prog" "-m models-nose -s 2 -n 0" 1
    run_test "$prog" "-m models-se -s 2 -n -1" 2
    run_test "$prog" "-m models-se -s 2 -n 0" 3
    run_test "$prog" "-m models-pro -s 2 -n -1" 4
    run_test "$prog" "-m models-pro -s 2 -n 0" 5
    echo ""
}

block_realsr() {
    local prog="realsr-ncnn"
    if [[ ! -x "$BIN_DIR/$prog" ]]; then
        echo "Program $prog does not exist, skipping."
        return
    fi
    echo "Testing program: $prog"
    run_test "$prog" "-m models-Real-ESRGAN-anime" 1
    run_test "$prog" "-m models-Real-ESRGANv3-general -s 4" 2
    run_test "$prog" "-m models-Real-ESRGANv3-anime -s 3" 3
    echo ""
}

block_srmd() {
    local prog="srmd-ncnn"
    if [[ ! -x "$BIN_DIR/$prog" ]]; then
        echo "Program $prog does not exist, skipping."
        return
    fi
    echo "Testing program: $prog"
    run_test "$prog" "-s 2" 1
    run_test "$prog" "-s 3" 2
    run_test "$prog" "-s 4" 3
    echo ""
}

block_waifu2x() {
    local prog="waifu2x-ncnn"
    if [[ ! -x "$BIN_DIR/$prog" ]]; then
        echo "Program $prog does not exist, skipping."
        return
    fi
    echo "Testing program: $prog"
    run_test "$prog" "-m models-cunet -n 0" 1
    run_test "$prog" "-m models-cunet -n 2" 2
    run_test "$prog" "-m models-upconv_7_anime_style_art_rgb -n 0" 3
    run_test "$prog" "-m models-upconv_7_anime_style_art_rgb -n 2" 4
    run_test "$prog" "-m models-upconv_7_photo -n 0" 5
    run_test "$prog" "-m models-upconv_7_photo -n 2" 6
    echo ""
}

block_mnnsr() {
    local prog="mnnsr-ncnn"
    if [[ ! -x "$BIN_DIR/$prog" ]]; then
        echo "Program $prog does not exist, skipping."
        return
    fi
    echo "Testing program: $prog"
    run_test "$prog" "-b 0 -m models-MNN/ESRGAN-MoeSR-jp_Illustration-x4.mnn -s 4" 1
    run_test "$prog" "-b 0 -m models-MNN/ESRGAN-Nomos8kSC-x4.mnn -s 4" 2
    echo ""
}

# ======================== Run tests ========================

if [[ -z "$target_program" ]]; then
    echo "Testing all programs..."
    echo ""
    block_resize
    block_realcugan
    block_realsr
    block_srmd
    block_waifu2x
    block_mnnsr
else
    echo "Testing single program: $target_program"
    echo ""
    case "$target_program" in
        resize-ncnn)    block_resize ;;
        realcugan-ncnn) block_realcugan ;;
        realsr-ncnn)    block_realsr ;;
        srmd-ncnn)      block_srmd ;;
        waifu2x-ncnn)   block_waifu2x ;;
        mnnsr-ncnn)     block_mnnsr ;;
        *)
            echo "Program not found: $target_program"
            exit 1
            ;;
    esac
fi

# ======================== Summary ========================

total_output_files=$(find "$OUTPUT_DIR" -maxdepth 1 -type f | wc -l)

echo "All tests completed!"
echo "Results saved in $OUTPUT_DIR"
echo ""
echo "Test Statistics:"
echo -e "  Total parameter groups tested: \033[36m$total_param_groups\033[0m"
echo -e "  Total output files generated: \033[33m$total_output_files\033[0m"
echo -e "  Total passed tests: \033[32m$total_passed_tests\033[0m"
echo -e "  Total failed tests: \033[31m$total_failed_tests\033[0m"

# Generate HTML report
if [[ "$generate_html" -eq 1 ]]; then
    echo ""
    echo "Generating HTML report..."
    if command -v python3 &>/dev/null; then
        python3 "$SCRIPT_DIR/evaluate_image_consistency.py" "$CSV_FILE"
        html_file="${CSV_FILE}.html"
        if [[ -f "$html_file" ]]; then
            echo "HTML report: $html_file"
        fi
    else
        echo "Warning: python3 not found, skipping HTML report generation."
    fi
fi
