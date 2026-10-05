#!/usr/bin/env bash
# 构建三个 Keil 工程并输出体积（重构前后比对用）。只读源码，只写 MDK-ARM/build_verify.log
UV4="/d/Keil_v5/UV4/UV4.exe"
REBUILD="${1:-}"          # 传 -r 强制全量重建，否则增量
run() {
  local dir="$1" proj="$2" name="$3"
  ( cd "D:/STM32_project/$dir/MDK-ARM" || exit 1
    "$UV4" $REBUILD -b "$proj" -j0 -o "build_verify.log" >/dev/null 2>&1
    local log; log=$(iconv -f GBK -t UTF-8 build_verify.log 2>/dev/null)
    local size; size=$(printf '%s' "$log" | grep -oE "Program Size:.*")
    local err;  err=$(printf '%s' "$log"  | grep -oE "[0-9]+ Error\(s\), [0-9]+ Warning\(s\)" | tail -1)
    printf "%-6s %-48s %s\n" "$name" "${size:-<未链接>}" "$err"
  )
}
run lab7 "T7.uvprojx"            lab7
run lab8 "summer project9.uvprojx" lab8
run LAB9 "LAB9.uvprojx"          LAB9
