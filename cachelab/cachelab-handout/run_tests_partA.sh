#!/bin/bash

# 输出文件
OUTPUT_FILE="result.txt"

# 清空输出文件
> $OUTPUT_FILE

echo "Running cache simulator tests... [ref]" >> $OUTPUT_FILE
echo "================================" >> $OUTPUT_FILE

./csim -s 1 -E 1 -b 1 -t traces/yi2.trace >> $OUTPUT_FILE

./csim -s 4 -E 2 -b 4 -t traces/yi.trace >> $OUTPUT_FILE

./csim -s 2 -E 1 -b 4 -t traces/dave.trace >> $OUTPUT_FILE

./csim -s 2 -E 1 -b 3 -t traces/trans.trace >> $OUTPUT_FILE

./csim -s 2 -E 2 -b 3 -t traces/trans.trace >> $OUTPUT_FILE

./csim -s 2 -E 4 -b 3 -t traces/trans.trace >> $OUTPUT_FILE

./csim -s 5 -E 1 -b 5 -t traces/trans.trace >> $OUTPUT_FILE

./csim -s 5 -E 1 -b 5 -t traces/long.trace >> $OUTPUT_FILE

echo "================================" >> $OUTPUT_FILE