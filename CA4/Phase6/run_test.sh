#!/usr/bin/env bash
set -euo pipefail

TEST_NAME=$(sed -n 's/const string TEST_NAME = "\([^"]*\)";/\1/p' experiment.hpp)
LOSS_PERCENT=$(sed -n 's/const int LOSS_PERCENT = \([0-9][0-9]*\);/\1/p' experiment.hpp)
ACK_DELAY_MS=$(sed -n 's/const int ACK_DELAY_MS = \([0-9][0-9]*\);/\1/p' experiment.hpp)
RESULT_DIRECTORY="results/phase6/$TEST_NAME"

if [ -z "$TEST_NAME" ] || [ -z "$LOSS_PERCENT" ] || [ -z "$ACK_DELAY_MS" ]; then
    echo "Could not read experiment.hpp"
    exit 1
fi

if [ ! -f sample_input.bin ]; then
    echo "sample_input.bin does not exist. Run: make sample"
    exit 1
fi

rm -rf "$RESULT_DIRECTORY"
mkdir -p "$RESULT_DIRECTORY"
rm -f sample_output.bin

make all

./receiver > "$RESULT_DIRECTORY/receiver_console.txt" 2>&1 &
RECEIVER_PID=$!

cleanup() {
    if kill -0 "$RECEIVER_PID" 2>/dev/null; then
        kill "$RECEIVER_PID" 2>/dev/null || true
    fi
}

trap cleanup EXIT
sleep 0.3

./sender > "$RESULT_DIRECTORY/sender_console.txt" 2>&1
wait "$RECEIVER_PID"
trap - EXIT

INPUT_HASH=$(sha256sum sample_input.bin | awk '{print $1}')
OUTPUT_HASH=$(sha256sum sample_output.bin | awk '{print $1}')

if cmp -s sample_input.bin sample_output.bin; then
    FILE_MATCH="YES"
else
    FILE_MATCH="NO"
fi

{
    echo "test_name=$TEST_NAME"
    echo "loss_percent=$LOSS_PERCENT"
    echo "ack_delay_ms=$ACK_DELAY_MS"
    echo "file_match=$FILE_MATCH"
    echo "input_sha256=$INPUT_HASH"
    echo "output_sha256=$OUTPUT_HASH"
} > "$RESULT_DIRECTORY/verification.txt"

python3 plot_results.py "$TEST_NAME"
python3 collect_results.py "$TEST_NAME"

cat "$RESULT_DIRECTORY/sender_console.txt"
echo
echo "File match: $FILE_MATCH"
echo "Results saved in: $RESULT_DIRECTORY"

if [ "$FILE_MATCH" != "YES" ]; then
    exit 1
fi
