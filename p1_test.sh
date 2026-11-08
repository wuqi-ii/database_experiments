#!/usr/bin/env bash
set -euo pipefail

# Resolve paths from this script so it also works outside the project directory.
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$project_dir"

p1_files=(
  src/include/container/hash/extendible_hash_table.h
  src/container/hash/extendible_hash_table.cpp
  src/include/buffer/lru_k_replacer.h
  src/buffer/lru_k_replacer.cpp
  src/include/buffer/buffer_pool_manager_instance.h
  src/buffer/buffer_pool_manager_instance.cpp
  test/buffer/buffer_pool_manager_edge_test.cpp
)

# CMake creates build/ when needed. Non-PIE executables avoid ASan startup
# address-layout conflicts under WSL while keeping memory checks enabled.
cmake -S . -B build \
  -DCMAKE_C_COMPILER=clang-14 -DCMAKE_CXX_COMPILER=clang++-14 \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXE_LINKER_FLAGS=-no-pie

clang-format-14 --dry-run --Werror "${p1_files[@]}"
python3 build_support/cpplint.py --verbose=2 --quiet --linelength=120 \
  --filter=-legal/copyright,-build/header_guard,-runtime/references \
  "${p1_files[@]}"

cmake --build build --target \
  extendible_hash_table_test lru_k_replacer_test \
  buffer_pool_manager_instance_test buffer_pool_manager_edge_test -j 8
cmake --build build --target check-clang-tidy-p1
ctest --test-dir build \
  -R '^(ExtendibleHashTableTest|LRUKReplacerTest|BufferPoolManagerInstanceTest|BufferPoolManagerEdgeTest)\.' \
  --output-on-failure -j 1
