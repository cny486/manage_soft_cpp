# ManageSoftCpp Project Memory

## 1. Project at a Glance

- Desktop inventory/reimbursement management app built with Qt and C++17.
- Supports two runtime modes through one shared `AppService` interface:
  - local single-machine mode via `JsonStorageService`
  - client/server mode via `TcpAppServiceClient` <-> `TcpBackendServer`
- Main business areas currently present:
  - inventory management
  - demand list / fulfillment analysis
  - reimbursement management with attachment export/download
  - account security with email verification
  - AI-based inventory field enrichment by `manufacturerPart`

## 2. Repo Layout

- `src/shared/`
  - shared domain types and service interface
  - local JSON storage implementation
  - TCP message codec
  - Excel read/write utility
  - SMTP email client
  - AI enrichment logic and settings
- `src/client/`
  - Qt Widgets desktop app
  - `main.cpp` builds either local or TCP-backed `AppService`
  - `mainwindow.cpp` defines top-level page config and navigation
  - `managementpage.cpp` is a large, central CRUD/import/export workflow page
- `src/server/`
  - TCP backend executable
  - wraps `JsonStorageService` and exposes actions over length-prefixed JSON
- `scripts/`
  - `build.ps1` is the main Windows build/package script
  - Linux deployment scripts exist for backend-only deployment
- `dist/`, `build/`, `release/`
  - mostly generated artifacts; usually ignore unless packaging/runtime verification is needed
- `manage_soft_cpp.pro`
  - legacy qmake config
- `CMakeLists.txt`
  - current primary build entry

## 3. Build and Run Conventions

- Current primary build system is CMake, not qmake.
- Root `CMakeLists.txt` builds:
  - `ManageSoftCpp` client
  - `ManageSoftServer` backend
- Important CMake options:
  - `MANAGE_SOFT_BUILD_CLIENT=ON/OFF`
  - `MANAGE_SOFT_BUILD_SERVER=ON/OFF`
- Expected Windows toolchain in `scripts/build.ps1`:
  - Qt `5.15.2` MinGW
  - MinGW g++/gcc
  - `mingw32-make`
- `scripts/build.ps1` also:
  - copies Qt runtime into `dist/ManageSoftCpp` and `dist/ManageSoftServer`
  - can package a single-file frontend exe with `-SingleFile`

Typical local binaries after build:

- `build/src/client/ManageSoftCpp.exe`
- `build/src/server/ManageSoftServer.exe`
- runnable bundles in `dist/ManageSoftCpp/` and `dist/ManageSoftServer/`

## 4. Architecture and Data Flow

- `AppService` in `src/shared/appservice.h` is the core abstraction.
- Local mode:
  - client directly uses `JsonStorageService`
- TCP mode:
  - client uses `TcpAppServiceClient`
  - server hosts `TcpBackendServer`
  - server delegates most business logic to the same `JsonStorageService`
- Practical implication:
  - many business changes should be implemented once in `JsonStorageService`
  - then mirrored in TCP transport only if a new action or payload shape is needed

## 5. Main Functional Modules

- Inventory
  - page id: `inventory`
  - defined in `src/client/mainwindow.cpp`
  - supports CRUD, direct update, stock in/out, delete with note, history, Excel import/export, BOM import, fulfillment analysis, AI enrich
- Reimbursement
  - page id: `reimbursement`
  - has attachment archiving/export/download workflows
  - attachment field keys:
    - `invoiceAttachment`
    - `otherAttachments`
- Demand library
  - page id: `demand_library`
  - imported from Excel
  - can analyze fulfillment against inventory using `manufacturerPart` + `quantity`
- Inventory history
  - stored separately in `inventory_history`

## 6. Persistence and Storage Details

- Local storage implementation is `JsonStorageService`.
- Storage root is under Qt app data location via `QStandardPaths::AppDataLocation`.
- Data is stored as JSON files by page id, for example:
  - `inventory.json`
  - `reimbursement.json`
  - `demand_library.json`
  - `inventory_history.json`
  - `users.json`
- Reimbursement and demand-list files are archived under `attachments/`.
- In TCP mode, the server is the source of truth because it also uses `JsonStorageService` locally on the server machine.

## 7. Auth and User Initialization

- Login is required before entering the main window.
- `JsonStorageService` seeds built-in users if `users.json` is missing/empty.
- Default password constant exists in storage service: `12345678`.
- Session handling in TCP mode is simple in-memory token mapping inside `TcpBackendServer`.
- If the server restarts, sessions are effectively reset.

## 8. Connection, AI, and Email Settings

### Connection settings

- `ConnectionSettings` uses `QSettings`.
- Current default TCP server target:
  - host: `111.229.149.41`
  - port: `45454`
- Legacy localhost default is auto-migrated to the current remote host in `connectionsettings.cpp`.
- Client can also be launched with CLI flags:
  - `--local-storage`
  - `--server-host`
  - `--server-port`
  - `--server-timeout-ms`

### AI settings

- AI settings are stored in `QSettings("ManageSoftCpp", "SharedSettings")`.
- Environment variable fallbacks:
  - `MANAGE_SOFT_AI_API_URL`
  - `MANAGE_SOFT_AI_API_KEY`
  - `MANAGE_SOFT_AI_MODEL`
  - `MANAGE_SOFT_AI_TIMEOUT_MS`
- AI enrichment behavior:
  - first tries to reuse an existing local inventory record with the same `manufacturerPart`
  - only then calls the external AI API
  - AI must return structured JSON and every accepted field must include `sourceTitle` and `sourceUrl`

### Email settings

- Email settings also use `QSettings("ManageSoftCpp", "SharedSettings")`.
- Environment variable fallbacks:
  - `MANAGE_SOFT_SMTP_HOST`
  - `MANAGE_SOFT_SMTP_PORT`
  - `MANAGE_SOFT_SMTP_SENDER_EMAIL`
  - `MANAGE_SOFT_SMTP_SENDER_NAME`
  - `MANAGE_SOFT_SMTP_USER`
  - `MANAGE_SOFT_SMTP_PASSWORD`
  - `MANAGE_SOFT_SMTP_TIMEOUT_MS`
- Verification mail is sent through a raw `QSslSocket` SMTP flow in `smtpemailclient.cpp`.

## 9. TCP Protocol Notes

- Transport is custom length-prefixed JSON, implemented by `tcpmessagecodec.*`.
- Requests include:
  - `version`
  - `requestId`
  - `action`
  - `payload`
- Many file operations move file bytes as base64 through JSON.
- When adding a new capability in TCP mode, check all three places:
  - `AppService`
  - `TcpAppServiceClient`
  - `TcpBackendServer`

## 10. UI and Page Implementation Notes

- UI is Qt Widgets with a custom stylesheet-heavy look.
- Top-level page config lives inline in `src/client/mainwindow.cpp`.
- `ManagementPage` is a large shared page for generic CRUD plus many inventory/reimbursement-specific branches.
- Custom dialogs/pages exist for more specialized flows:
  - `InventoryRecordDialog`
  - `InventoryTransactionDialog`
  - `InventoryHistoryDialog`
  - `InventoryFulfillmentDialog`
  - `DemandLibraryOverviewPage`
  - `DemandLibraryDetailPage`
  - `ReimbursementOverviewPage`
  - `AccountSecurityDialog`

## 11. Important Development Habits for This Repo

- Prefer reading `src/shared/` first for business-rule changes.
- Prefer reading `mainwindow.cpp` before UI/page changes because it defines page ids and field schemas.
- For inventory-related changes, `managementpage.cpp` is often the client-side hub.
- For TCP-mode bugs, compare local-mode behavior against server-mode behavior to locate whether the issue is:
  - shared logic
  - client transport
  - server action wiring

## 12. Known Risks / Gotchas

- The repo contains generated outputs and packaged binaries; avoid treating `dist/`, `build/`, and `release/` as source of truth.
- There are hardcoded default SMTP credentials in `src/shared/emailsettings.h`; treat this as sensitive technical debt.
- The TCP server stores sessions only in memory.
- The protocol is plain TCP JSON without TLS; do not assume internet-safe deployment.
- Shell output on Windows may show garbled Chinese if the console encoding is mismatched; the source itself still contains Chinese UI text.
- `managementpage.cpp` and `jsonstorageservice.cpp` are both large files and common hotspots for regressions.

## 13. First Files to Reopen Next Time

If starting a new task, reopen these first unless the task is very narrow:

1. `PROJECT_MEMORY.md`
2. `src/shared/appservice.h`
3. `src/client/mainwindow.cpp`
4. one of:
   - `src/shared/jsonstorageservice.cpp`
   - `src/client/managementpage.cpp`
   - `src/server/tcpbackendserver.cpp`
   depending on the task

## 14. Recommended Future Cleanup

- Move secrets out of source defaults.
- Add a real README for build/run/deploy.
- Separate generic CRUD page code from inventory/reimbursement special cases.
- Add protocol/API documentation for TCP actions.
- Add tests around JSON storage rules, fulfillment logic, and attachment handling.

## 15. Recent Deployment Notes

- 2026-06-22:
  - Inventory fulfillment matching logic in `src/shared/jsonstorageservice.cpp` was tightened to compare against the actual key columns present in each demand row.
  - `manufacturerPart` stays strict after normalization.
  - Other populated key fields such as `value`, `footprint`, `voltage`, `name`, `manufacturer`, `supplier`, `device`, `category`, and `designator` now support "inventory contains demand value" matching after normalization.
  - Candidate filtering was later relaxed on the same day: matches are now scored by direct column comparison, and any row with at least one matched key field can enter the candidate list. This avoids zero-result searches caused by over-strict all-fields filtering.
  - For TCP/server mode, these fulfillment changes require redeploying `ManageSoftServer`; client-only replacement is not sufficient.
  - Preferred deployment entrypoint remains `scripts/deploy_linux_server_safe.ps1` so server data is backed up before restart.
  - Server deployment completed successfully on `111.229.149.41` for service `manage-soft-server`.
  - Confirmed runtime data root on the server: `/home/lws2/.local/share/ManageSoftCpp/ManageSoftServer/data`
  - Backup archives created before restarts:
    - `/home/lws2/manage_soft_backups/manage_soft_data_20260622_231145.tar.gz`
    - `/home/lws2/manage_soft_backups/manage_soft_data_20260622_231811.tar.gz`
