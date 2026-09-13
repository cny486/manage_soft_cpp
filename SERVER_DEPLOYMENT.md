# ManageSoftCpp TCP 后端部署说明

## 1. 构建产物

使用项目原有脚本构建：

```powershell
./build_app.cmd
```

构建完成后会生成两个目录：

- `dist/ManageSoftCpp`：客户端
- `dist/ManageSoftServer`：后端服务

## 2. 本机联调启动

先启动后端：

```powershell
./dist/ManageSoftServer/ManageSoftServer.exe
```

默认监听：

- Host: `0.0.0.0`
- Port: `45454`

再启动客户端：

```powershell
./dist/ManageSoftCpp/ManageSoftCpp.exe
```

客户端默认连接：

- Host: `127.0.0.1`
- Port: `45454`

如果要继续单机模式：

```powershell
./dist/ManageSoftCpp/ManageSoftCpp.exe --local-storage
```

## 3. Linux 服务器部署

当前推荐方式是从本机直接执行独立部署脚本，脚本会自动完成：

- 打包当前仓库源码
- 通过 SSH 上传到 Linux 服务器
- 在远端仅构建 `ManageSoftServer`
- 停止旧的手工 `ManageSoftServer` 进程，避免端口冲突
- 安装或更新 `systemd` 服务
- 重启服务并校验服务确实进入 `active`

### 3.1 服务器前置条件

目标服务器需要已安装：

- `cmake`
- `g++`
- `qmake`
- `rsync` 或基础 OpenSSH/SFTP 能力
- `systemd`

当前已验证以下环境可用：

- Ubuntu 24.04 LTS
- Qt 5.15.13
- `sudo` 可用

### 3.2 本机执行部署

PowerShell 入口：

```powershell
./scripts/deploy_linux_server.ps1 `
	-ServerHost 111.229.149.41 `
	-Username lws2 `
	-RemoteRoot '~/manage_soft_cpp_linux_build' `
	-ServiceName manage-soft-server `
	-ListenHost 0.0.0.0 `
	-ListenPort 45454
```

如果你要在命令行里直接传密码：

```powershell
./scripts/deploy_linux_server.ps1 `
	-ServerHost 111.229.149.41 `
	-Username lws2 `
	-Password '***' `
	-RemoteRoot '~/manage_soft_cpp_linux_build'
```

也可以直接调用 Python 脚本：

```powershell
./.venv/Scripts/python.exe ./scripts/deploy_linux_server.py `
	--host 111.229.149.41 `
	--username lws2 `
	--remote-root ~/manage_soft_cpp_linux_build
```

### 3.3 部署后的远端目录

默认部署目录：

```text
/home/lws2/manage_soft_cpp_linux_build
```

脚本会使用以下结构：

- `src/`：上传并解压后的源码
- `build/`：远端 CMake 构建目录
- `run/ManageSoftServer`：实际运行的服务端二进制

### 3.4 systemd 服务

默认服务名：

```text
manage-soft-server.service
```

常用运维命令：

```bash
sudo systemctl status manage-soft-server
sudo systemctl restart manage-soft-server
sudo journalctl -u manage-soft-server -n 200 --no-pager
```

如果服务器启用了防火墙，需要放行对应 TCP 端口，例如 `45454`。

## 4. 客户端连接服务器

客户端有两种配置方式。

### 命令行方式

```powershell
./ManageSoftCpp.exe --server-host 192.168.1.10 --server-port 45454
```

### 图形界面方式

启动客户端后，点击主界面右上角“连接设置”：

- 可切换“本地文件存储”或“TCP 后端模式”
- 可填写服务器地址、端口、超时时间
- 可点击“测试连接”验证服务是否可达
- 保存后重启客户端生效

## 5. 局域网部署建议

- 后端部署在一台固定 IP 的 Windows 主机上
- 客户端统一配置为该主机内网 IP
- 给服务端机器设置固定 IP 或 DHCP 保留地址
- 如果客户端较多，优先使用主机名或内网 DNS，减少 IP 变更影响

## 6. 公网部署建议

当前协议是明文 TCP JSON，不建议直接裸露到公网。

如果必须跨公网访问，建议至少增加一层：

- VPN
- 内网穿透加访问控制
- TCP 反向代理或隧道

更稳妥的下一步改造方向：

- 增加登录鉴权
- 增加 TLS 传输加密
- 增加服务端日志和异常恢复
- 将 JSON 存储升级为数据库

## 7. 数据位置

当前后端仍使用 `JsonStorageService`，数据实际保存在服务器本机的应用数据目录。

这意味着：

- 客户端不再直接读写库存 JSON
- 数据真相在服务器端
- 备份只需要备份服务器上的数据目录

## 8. 运维建议

- 定期备份服务器数据目录
- 给后端进程设置开机自启
- 用日志重定向保存标准输出和错误输出
- 升级客户端前先验证与服务端协议兼容

## 9. AI 补齐接口配置

库存新增/编辑窗口现在支持通过 Manufacturer Part 调用 AI 补齐元件信息。

补齐结果要求：

- 返回结构化字段
- 每条字段都必须附带来源标题和来源链接，供用户查阅
- 如果没有可靠来源，该字段会被自动丢弃

当前实现优先级：

1. 先查本地库存是否已有同 Manufacturer Part 的记录
2. 本地没有命中时，再调用外部 AI API

### 环境变量

后端模式下，给 `ManageSoftServer.exe` 所在进程设置以下环境变量：

- `MANAGE_SOFT_AI_API_URL`：OpenAI 兼容接口地址，例如 `https://your-host/v1/chat/completions`
- `MANAGE_SOFT_AI_API_KEY`：接口密钥，可为空（取决于你的网关）
- `MANAGE_SOFT_AI_MODEL`：模型名，例如 `gpt-4.1-mini`
- `MANAGE_SOFT_AI_TIMEOUT_MS`：请求超时，默认 `30000`

示例：

```powershell
$env:MANAGE_SOFT_AI_API_URL = "https://your-host/v1/chat/completions"
$env:MANAGE_SOFT_AI_API_KEY = "sk-..."
$env:MANAGE_SOFT_AI_MODEL = "gpt-4.1-mini"
$env:MANAGE_SOFT_AI_TIMEOUT_MS = "30000"
./ManageSoftServer.exe --listen-host 0.0.0.0 --listen-port 45454
```

客户端远程模式的“测试 AI 连接”会使用当前已登录服务器的上述配置；客户端不会向后端发送 API Key 或覆盖服务器配置。后端 TCP 服务应只部署在受信任网络中，并建议置于 TLS 终端或 VPN 后。

### 本地模式说明

如果客户端使用 `--local-storage` 启动，则 AI 补齐在客户端进程内执行。

也就是说，本地模式下需要给 `ManageSoftCpp.exe` 自身设置同样的环境变量，或在“连接设置”中填写并保存本机 AI 配置。
