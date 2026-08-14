# 版本更新上线说明

## 产物

先在 Windows 构建机执行：

```powershell
./scripts/build.ps1
```

构建完成后会得到：

- `dist/ManageSoftCpp/`：客户端完整运行目录
- `dist/ManageSoftCpp-update.zip`：客户端更新包
- `dist/update_manifest.json`：服务端使用的更新清单
- `dist/ManageSoftServer/`：服务端运行目录

默认会从 `src/shared/appversion.cpp` 读取当前客户端版本，并把它写入 `dist/update_manifest.json`。

## 可选参数

如果要覆盖默认发布信息，可以这样构建：

```powershell
./scripts/build.ps1 `
  -ReleaseVersion 1.0.1 `
  -MinimumSupportedVersion 1.0.0 `
  -ReleaseTitle '客户端更新 1.0.1' `
  -ReleaseNotes '配单能力修正','登录后自动更新' `
  -PublishedAt 2026-07-07
```

说明：

- `ReleaseVersion`：更新版本号
- `MinimumSupportedVersion`：低于该版本的客户端会被判定为必须更新
- `ReleaseNotes`：写入更新弹窗和 manifest

## 上线

部署服务端并同时发布客户端更新包：

```powershell
./scripts/deploy_linux_server_safe.ps1 `
  -ServerHost 111.229.149.41 `
  -Username lws2 `
  -ClientUpdatePackage .\dist\ManageSoftCpp-update.zip `
  -ClientUpdateManifest .\dist\update_manifest.json
```

脚本会把以下文件发布到服务端运行目录：

- `ManageSoftServer`
- `update_manifest.json`
- `ManageSoftCpp-update.zip`

服务端会默认从自身运行目录读取 `update_manifest.json`，并按 manifest 中的相对路径提供更新包下载。

## 验证

上线后可按下面顺序验证：

1. 用旧版本客户端登录 TCP 服务端。
2. 登录成功后确认出现版本更新提示框。
3. 点击 `Update now`。
4. 确认客户端下载更新包、退出、拉起 `ManageSoftUpdater`、覆盖文件并重新启动。
5. 重启后的客户端版本应与 `update_manifest.json` 中的 `latestVersion` 一致。
