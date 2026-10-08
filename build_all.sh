#!/usr/bin/env bash
# =============================================================================
# 一次构建三个 lab 的固件
#
# 三个 lab 各自是独立的 CMake 工程（CubeMX 维护 cmake/stm32cubemx/），
# 产物在 <lab>/build/<Preset>/ 下：.elf / .hex / .bin，hex 直接加载进 Proteus。
#
# 用法：
#   ./build_all.sh               增量构建 Debug
#   ./build_all.sh Release       构建 Release
#   ./build_all.sh Debug clean   先删掉 build/ 再全量构建
#
# 注：本脚本早先走的是 Keil UV4 命令行，三个 lab 迁到 CMake 后已作废。
# =============================================================================
set -u

PRESET="${1:-Debug}"
ACTION="${2:-}"

ROOT="$(cd "$(dirname "$0")" && pwd)"
LABS=(lab7 lab8 LAB9)

fail=0

for lab in "${LABS[@]}"; do
    echo
    echo "════════════════════════════════════════"
    echo "  $lab   [$PRESET]"
    echo "════════════════════════════════════════"

    if ! cd "$ROOT/$lab"; then
        echo "  ✗ 目录不存在"
        fail=1
        continue
    fi

    if [ "$ACTION" = "clean" ]; then
        echo "  清理 build/$PRESET"
        rm -rf "build/$PRESET"
    fi

    if ! cmake --preset "$PRESET" > /dev/null; then
        echo "  ✗ 配置失败"
        fail=1
        continue
    fi

    if ! output=$(cmake --build --preset "$PRESET" 2>&1); then
        printf '%s\n' "$output" | tail -25
        echo "  ✗ 构建失败"
        fail=1
        continue
    fi

    # 正常时只打印体积统计那几行（POST_BUILD 里跑的是 arm-none-eabi-size）
    printf '%s\n' "$output" | tail -6
    echo "  ✓ $lab/build/$PRESET/$lab.hex"
done

echo
if [ "$fail" -eq 0 ]; then
    echo "全部构建成功"
else
    echo "有构建失败，见上面输出"
fi
exit "$fail"
