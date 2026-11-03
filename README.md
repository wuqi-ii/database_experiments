# database_experiments

数据库实验小组仓库，基于本次导入的 BusTub 教学数据库代码（CMake 项目标记为 2023.1）。用于共享实验代码、测试与协作记录。

当前为初始代码导入，不代表实验功能已完成或测试已通过。

## 小组成员

- 古淞滔
- 刘婧怡
- 文泰龙
- 张峻鸣
- 盛雅雯

## 项目结构

| 路径 | 内容 |
| --- | --- |
| `src/` | 数据库实现与头文件 |
| `test/` | 单元测试与 SQL 测试 |
| `tools/` | Shell、基准测试等工具 |
| `third_party/` | 随项目提供的第三方依赖 |
| `build_support/` | 格式检查、静态检查等辅助工具 |
| `build_project.sh`、`p1_test.sh`、`p2_test.sh` | 原始构建与分阶段测试脚本 |
| [CONTRIBUTING.md](CONTRIBUTING.md) | 团队协作与文档维护规范 |
| [CHANGELOG.md](CHANGELOG.md) | 项目改动记录 |

## 构建与测试

建议在 Linux 或 Windows 的 WSL2 中使用 CMake 3.10 及以上版本、支持 C++17 的编译器和 Make；项目建议使用 Clang 14。原始 Dockerfile 使用 Clang 12，与该建议存在版本差异，使用时请记录实际环境。

在仓库根目录运行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 2

# 示例：单独构建并执行 P1 的三个测试
cmake --build build --target extendible_hash_table_test lru_k_replacer_test buffer_pool_manager_instance_test -j 2
./build/test/extendible_hash_table_test
./build/test/lru_k_replacer_test
./build/test/buffer_pool_manager_instance_test

# 需要检查全部测试时，先构建测试目标
cmake --build build --target build-tests -j 2
ctest --test-dir build --output-on-failure
```

初始代码中的部分功能可能尚未实现，测试结果应以成员实际运行记录为准。构建目录中的原有缓存和二进制不作为测试证据。

## 团队协作

1. 开始工作前同步 `main`，为任务创建分支，例如 `feat/p1-buffer-pool`、`fix/lru-k` 或 `docs/build-guide`。
2. 提交代码时说明改动目的，并完成相关测试；提交中同时维护受影响的说明文档和改动记录。
3. 推送分支并创建 Pull Request，建议由另一位成员审阅后合并。此流程是团队约定，尚未配置为强制分支保护。
4. 合并前处理冲突并重新验证，避免强制推送 `main`。具体步骤见 [协作指南](CONTRIBUTING.md)。

原工程附带的 `.github/workflows/cmake.yml` 仅监听 `master`，且使用旧版运行环境；本次保留原文件，尚未适配本仓库的 `main`，请按上面的命令执行本地验证。

## README 维护说明

- 修改构建命令、依赖、目录结构、使用方式或协作流程时，由本次改动的提交者在同一个 Pull Request 中更新本文件。
- 功能实现细节放在代码注释或专门文档中；README 保留项目入口、构建步骤和协作指引，并检查相对链接是否有效。
- 只有实际验证后才能写“通过”或“已完成”；未执行的检查应标注原因。
- 维护小组成员时只记录姓名，不添加学号、个人联系方式、凭据或本机绝对路径。

## 项目改动记录维护

每个影响功能、修复、测试、依赖、构建或文档的 Pull Request，均应在 [CHANGELOG.md](CHANGELOG.md) 的“未发布”部分增加一条记录：说明改了什么、原因、验证结果及关联 PR。合并后由合并者补全 PR 链接；阶段验收时将相关记录归档到日期条目。填写格式和示例见 [协作指南](CONTRIBUTING.md#改动记录的填写方式)。

## 来源与许可

本仓库代码来源于小组提供的 BusTub 初始工程，上游项目为 [cmu-db/bustub](https://github.com/cmu-db/bustub)。由于原始目录没有 Git 历史，无法确认其对应的上游提交。本次保留原有 [LICENSE](LICENSE) 和第三方依赖中的许可文件；修改及分发时应继续保留这些声明。
