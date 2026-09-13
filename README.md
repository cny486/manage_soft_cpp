# ManageSoftCpp 源码使用说明

本仓库的 `master` 是 GitHub 默认分支。`codex/clean-source-2026-09-13` 保存了当前版本的干净源码快照；两者合并后使用相同的项目文件。项目主入口在仓库根目录，`manage_soft_cpp-barcode/` 是保留的扫码枪相关子项目。

## 获取代码

直接使用默认分支：

```bash
git clone https://github.com/cny486/manage_soft_cpp.git
cd manage_soft_cpp
```

需要明确使用源码快照分支时，在克隆后运行：

```bash
git switch --track origin/codex/clean-source-2026-09-13
```

该分支最初是独立源码快照，旧的构建包历史没有包含在分支中。不要从旧工作目录复制 `build*`、`debug`、`release` 或 `dist` 到新克隆；这些目录都应在本机重新生成。

## 构建与运行

项目使用 CMake 3.16 及以上版本、C++17、Qt 5 或 6（Core、Network、Widgets）和 ZLIB。Windows 现有构建脚本按 Qt 5.15.2 + MinGW 配置，运行前需检查并按本机安装位置调整 `scripts/build.ps1` 开头的 Qt、编译器和 `mingw32-make` 路径：

```powershell
./build_app.cmd
```

脚本构建客户端、服务端和更新程序，并生成 `dist/ManageSoftCpp/`、`dist/ManageSoftServer/` 与客户端更新包。先启动 `dist/ManageSoftServer/ManageSoftServer.exe`，再启动 `dist/ManageSoftCpp/ManageSoftCpp.exe`。默认服务端端口为 `45454`；详细配置见 [服务端部署说明](SERVER_DEPLOYMENT.md)。

只需在 Linux 构建服务端时，可在安装 Qt Core/Network、ZLIB 和 C++ 编译器后执行：

```bash
cmake -S . -B build-server -DMANAGE_SOFT_BUILD_CLIENT=OFF -DMANAGE_SOFT_BUILD_UPDATER=OFF
cmake --build build-server --parallel
```

`build*`、`dist`、`release`、可执行文件和本地缓存已列入 `.gitignore`，不需要提交。客户端更新包的制作与发布步骤见 [版本更新上线说明](VERSION_UPDATE_RELEASE.md)；扫码枪使用方式见 [扫码枪功能说明](manage_soft_cpp-barcode/SCANNER_INTEGRATION.md)。
