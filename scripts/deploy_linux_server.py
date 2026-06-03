from __future__ import annotations

import argparse
import getpass
import os
import posixpath
import shlex
import sys
import tarfile
import tempfile
from pathlib import Path

import paramiko


EXCLUDED_TOP_LEVEL = {
    '.git',
    '.idea',
    '.venv',
    '.vs',
    'build',
    'build-test',
    'debug',
    'dist',
    'release',
}

EXCLUDED_DIR_NAMES = {
    '__pycache__',
    'CMakeFiles',
}

EXCLUDED_FILE_SUFFIXES = {
    '.o',
    '.obj',
    '.pyc',
    '.pyo',
    '.user',
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description='Deploy ManageSoftServer to a Linux host over SSH.')
    parser.add_argument('--host', required=True, help='Linux server host or IP.')
    parser.add_argument('--port', type=int, default=22, help='SSH port. Default: 22')
    parser.add_argument('--username', required=True, help='SSH username used for upload and build.')
    parser.add_argument('--password', help='SSH password. If omitted, prompt securely.')
    parser.add_argument('--configuration', choices=('Release', 'Debug'), default='Release', help='CMake build type.')
    parser.add_argument('--remote-root', default='~/manage_soft_cpp_linux_build', help='Remote working directory.')
    parser.add_argument('--service-name', default='manage-soft-server', help='systemd service name.')
    parser.add_argument('--listen-host', default='0.0.0.0', help='Server listen host.')
    parser.add_argument('--listen-port', type=int, default=45454, help='Server listen port.')
    parser.add_argument('--api-url', default='', help='Optional MANAGE_SOFT_AI_API_URL value.')
    parser.add_argument('--api-key', default='', help='Optional MANAGE_SOFT_AI_API_KEY value.')
    parser.add_argument('--api-model', default='', help='Optional MANAGE_SOFT_AI_MODEL value.')
    parser.add_argument('--api-timeout-ms', type=int, default=30000, help='MANAGE_SOFT_AI_TIMEOUT_MS value.')
    parser.add_argument('--skip-systemd', action='store_true', help='Only upload and build, do not install or restart systemd service.')
    return parser.parse_args()


def should_skip_dir(relative_dir: Path) -> bool:
    parts = relative_dir.parts
    if not parts:
        return False
    if parts[0] in EXCLUDED_TOP_LEVEL:
        return True
    return any(part in EXCLUDED_DIR_NAMES for part in parts)


def should_skip_file(relative_file: Path) -> bool:
    parts = relative_file.parts
    if not parts:
        return False
    if parts[0] in EXCLUDED_TOP_LEVEL:
        return True
    if any(part in EXCLUDED_DIR_NAMES for part in parts[:-1]):
        return True
    return relative_file.suffix.lower() in EXCLUDED_FILE_SUFFIXES


def create_source_archive(repo_root: Path) -> Path:
    temp_dir = Path(tempfile.mkdtemp(prefix='manage_soft_linux_deploy_'))
    archive_path = temp_dir / 'manage_soft_cpp_linux_src.tar.gz'

    with tarfile.open(archive_path, 'w:gz') as archive:
        for current_root, dir_names, file_names in os.walk(repo_root):
            current_path = Path(current_root)
            relative_dir = current_path.relative_to(repo_root)

            filtered_dirs = []
            for dir_name in dir_names:
                relative_child = relative_dir / dir_name if relative_dir != Path('.') else Path(dir_name)
                if not should_skip_dir(relative_child):
                    filtered_dirs.append(dir_name)
            dir_names[:] = filtered_dirs

            for file_name in file_names:
                relative_file = relative_dir / file_name if relative_dir != Path('.') else Path(file_name)
                if should_skip_file(relative_file):
                    continue
                source_path = repo_root / relative_file
                archive.add(source_path, arcname=posixpath.join('manage_soft_cpp', relative_file.as_posix()))

    return archive_path


def resolve_remote_root(home_dir: str, remote_root: str) -> str:
    if remote_root == '~':
        return home_dir
    if remote_root.startswith('~/'):
        return posixpath.join(home_dir, remote_root[2:])
    return remote_root


def quote_remote(value: str) -> str:
    return shlex.quote(value)


def escape_systemd_value(value: str) -> str:
    return value.replace('\\', '\\\\').replace('"', '\\"').replace('%', '%%')


class RemoteSession:
    def __init__(self, host: str, port: int, username: str, password: str):
        self._password = password
        self._client = paramiko.SSHClient()
        self._client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        self._client.connect(
            hostname=host,
            port=port,
            username=username,
            password=password,
            timeout=15,
            banner_timeout=15,
            auth_timeout=15,
            look_for_keys=False,
            allow_agent=False,
        )

    def close(self) -> None:
        self._client.close()

    def run(self, command: str, *, sudo: bool = False, timeout: int = 1200, check: bool = True) -> str:
        remote_command = command
        if sudo:
            remote_command = f"sudo -S -p '' sh -lc {quote_remote(command)}"

        stdin, stdout, stderr = self._client.exec_command(remote_command, timeout=timeout)
        if sudo:
            stdin.write(self._password + '\n')
            stdin.flush()

        stdout_text = stdout.read().decode('utf-8', errors='replace')
        stderr_text = stderr.read().decode('utf-8', errors='replace')
        exit_status = stdout.channel.recv_exit_status()
        if check and exit_status != 0:
            raise RuntimeError(
                f"Remote command failed with exit code {exit_status}: {command}\nSTDOUT:\n{stdout_text}\nSTDERR:\n{stderr_text}"
            )
        return stdout_text.strip()

    def upload_file(self, local_path: Path, remote_path: str) -> None:
        remote_dir = posixpath.dirname(remote_path)
        self.run(f"mkdir -p {quote_remote(remote_dir)}")
        with self._client.open_sftp() as sftp:
            sftp.put(str(local_path), remote_path)


def build_unit_file(args: argparse.Namespace, remote_root: str) -> str:
    run_dir = posixpath.join(remote_root, 'run')
    env_lines = [
        f'Environment="MANAGE_SOFT_SERVER_LISTEN_HOST={escape_systemd_value(args.listen_host)}"',
        f'Environment="MANAGE_SOFT_SERVER_LISTEN_PORT={escape_systemd_value(str(args.listen_port))}"',
        f'Environment="MANAGE_SOFT_AI_TIMEOUT_MS={escape_systemd_value(str(args.api_timeout_ms))}"',
    ]

    if args.api_url:
        env_lines.append(f'Environment="MANAGE_SOFT_AI_API_URL={escape_systemd_value(args.api_url)}"')
    if args.api_key:
        env_lines.append(f'Environment="MANAGE_SOFT_AI_API_KEY={escape_systemd_value(args.api_key)}"')
    if args.api_model:
        env_lines.append(f'Environment="MANAGE_SOFT_AI_MODEL={escape_systemd_value(args.api_model)}"')

    env_block = '\n'.join(env_lines)

    return f"""[Unit]
Description=ManageSoft TCP backend server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User={args.username}
WorkingDirectory={run_dir}
{env_block}
ExecStart={posixpath.join(run_dir, 'ManageSoftServer')}
Restart=on-failure
RestartSec=3
NoNewPrivileges=true

[Install]
WantedBy=multi-user.target
"""


def write_temp_file(content: str, suffix: str) -> Path:
    temp_dir = Path(tempfile.mkdtemp(prefix='manage_soft_linux_deploy_'))
    temp_path = temp_dir / f'temp{suffix}'
    temp_path.write_text(content, encoding='utf-8', newline='\n')
    return temp_path


def main() -> int:
    args = parse_args()
    password = args.password or getpass.getpass(f'SSH password for {args.username}@{args.host}: ')
    repo_root = Path(__file__).resolve().parent.parent

    print('==> Packaging source archive')
    archive_path = create_source_archive(repo_root)

    session = RemoteSession(args.host, args.port, args.username, password)
    try:
        home_dir = session.run('printf %s "$HOME"')
        remote_root = resolve_remote_root(home_dir, args.remote_root)
        remote_archive = posixpath.join(remote_root, 'manage_soft_cpp_linux_src.tar.gz')
        remote_source_dir = posixpath.join(remote_root, 'src')
        remote_build_dir = posixpath.join(remote_root, 'build')
        remote_run_dir = posixpath.join(remote_root, 'run')
        remote_unit_temp = posixpath.join(remote_root, f'{args.service_name}.service')
        remote_binary = posixpath.join(remote_run_dir, 'ManageSoftServer')
        unit_file_path = f'/etc/systemd/system/{args.service_name}.service'

        print(f'==> Uploading source archive to {args.host}:{remote_archive}')
        session.upload_file(archive_path, remote_archive)

        generator = session.run("if command -v ninja >/dev/null 2>&1; then printf 'Ninja'; else printf 'Unix Makefiles'; fi")
        parallel_jobs = session.run('nproc') or '2'
        qt_prefix = session.run('qmake -query QT_INSTALL_PREFIX', check=False)

        print('==> Extracting source archive on remote host')
        session.run(
            f"rm -rf {quote_remote(remote_source_dir)} && "
            f"mkdir -p {quote_remote(remote_source_dir)} {quote_remote(remote_run_dir)} && "
            f"tar -xzf {quote_remote(remote_archive)} -C {quote_remote(remote_source_dir)} --strip-components=1"
        )

        configure_parts = [
            'cmake',
            '-S', quote_remote(remote_source_dir),
            '-B', quote_remote(remote_build_dir),
            '-G', quote_remote(generator),
            f'-DCMAKE_BUILD_TYPE={quote_remote(args.configuration)}',
            '-DMANAGE_SOFT_BUILD_CLIENT=OFF',
            '-DMANAGE_SOFT_BUILD_SERVER=ON',
        ]
        if qt_prefix:
            configure_parts.append(f'-DCMAKE_PREFIX_PATH={quote_remote(qt_prefix)}')

        print('==> Configuring remote build')
        session.run(' '.join(configure_parts))

        print('==> Building ManageSoftServer on remote host')
        session.run(
            f"cmake --build {quote_remote(remote_build_dir)} --target ManageSoftServer -j {quote_remote(parallel_jobs)}"
        )

        print('==> Staging server binary')
        session.run(
            f"install -m 755 {quote_remote(posixpath.join(remote_build_dir, 'src/server/ManageSoftServer'))} {quote_remote(remote_binary)}"
        )

        if not args.skip_systemd:
            print('==> Installing systemd service')
            unit_file_content = build_unit_file(args, remote_root)
            local_unit_file = write_temp_file(unit_file_content, '.service')
            session.upload_file(local_unit_file, remote_unit_temp)
            print('==> Stopping existing ManageSoftServer processes')
            session.run("pkill -f '/ManageSoftServer' || true", check=False)
            session.run(f"install -m 644 {quote_remote(remote_unit_temp)} {quote_remote(unit_file_path)}", sudo=True)
            session.run('systemctl daemon-reload', sudo=True)
            session.run(f"systemctl enable {quote_remote(args.service_name)}", sudo=True)
            session.run(f"systemctl restart {quote_remote(args.service_name)}", sudo=True)
            state_output = session.run(
                "for i in 1 2 3 4 5 6 7 8; do "
                f"state=$(systemctl is-active {quote_remote(args.service_name)} 2>/dev/null || true); "
                "if [ \"$state\" = active ]; then printf %s \"$state\"; exit 0; fi; "
                "sleep 1; "
                "done; "
                f"systemctl is-active {quote_remote(args.service_name)} 2>/dev/null || true; "
                "exit 1",
                sudo=True,
                check=False,
            )
            if state_output.strip() != 'active':
                status_output = session.run(
                    f"systemctl --no-pager --full status {quote_remote(args.service_name)} || true",
                    sudo=True,
                    check=False,
                )
                journal_output = session.run(
                    f"journalctl -u {quote_remote(args.service_name)} -n 50 --no-pager || true",
                    sudo=True,
                    check=False,
                )
                raise RuntimeError(
                    'Deployment finished uploading and building, but the systemd service did not become active.\n'
                    f'State: {state_output}\nStatus:\n{status_output}\nJournal:\n{journal_output}'
                )
            status_output = session.run(f"systemctl --no-pager --full status {quote_remote(args.service_name)}", sudo=True)
            print(status_output)
        else:
            print('==> Skipped systemd install/restart')

        print('==> Cleaning remote source and build artifacts')
        session.run(
            f"find {quote_remote(remote_root)} -mindepth 1 -maxdepth 1 ! -name run -exec rm -rf {{}} +"
        )
        retained_files = session.run(
            f"find {quote_remote(remote_root)} -maxdepth 2 -type f | sort",
            check=False,
        )
        if retained_files:
            print(retained_files)

        print('==> Deployment completed')
        print(f'Remote root   : {remote_root}')
        print(f'Server binary : {remote_binary}')
        if not args.skip_systemd:
            print(f'Service name  : {args.service_name}')

        return 0
    finally:
        session.close()


if __name__ == '__main__':
    sys.exit(main())