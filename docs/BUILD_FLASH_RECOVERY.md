# Build, flash and recovery

KamoteKeys uses each device's native build system unchanged (SMK37 OPEN for the SMK-37, FM1 OPEN for the FM-1). Nothing under `devices/*/sdk/` is modified: each `sdk/manifest.json` hash-locks the SDK, the build script and the toolchain, and the build refuses to run if any of them differ. FM-1 flashing and recovery: [FM1.md](FM1.md).

## 1. Requirements

- Windows x64, PowerShell, `curl.exe`, `tar.exe`.
- [JieLi Windows toolchain 2.5.3](https://jl-update.oss-cn-shenzhen.aliyuncs.com/2.5.3.exe) (PI32v2 clang 4.0.1, target `r3`); [installation guide](https://doc.zh-jieli.com/Tools/zh-cn/dev_tools/dev_env/jl_toolchain.html). Note the `pi32` directory that contains `bin\clang.exe`.
- For the workbench package: an HTTPS URL of the **matching source archive** (GPL requirement, enforced by the script). A tagged GitHub archive of your repository works, e.g. `https://github.com/<you>/kamotkeys/archive/refs/tags/v0.37.zip`.
- The first web build downloads WASI SDK 27 (or set `FM1_WASI_SDK`).

## 2. Build

Run from the repository root. `build.bat` first copies the shared code (`core/`) into the device tree, then runs that device's own SDK build:

```powershell
cd kamotekeys
$env:FM1_PI32_TOOLCHAIN = 'C:\JL\pi32'
$env:FM1_SOURCE_URL     = 'https://github.com/<you>/<repo>/archive/refs/heads/kamotekeys.zip'
.\build.bat smk37-pro --package        # or: smk37-elite, fm1
```

Outputs (paths relative to the repository root):

| File | Use |
|---|---|
| `devices\smk37\build\official-web\pro\SMK37-Open-Source.html` (`elite` for the Elite) | SMK-37 offline workbench with KamoteKeys embedded: **this is what you flash with** |
| `devices\fm1\build\official-web\FM1-Official-Open-Source.html` | FM-1 offline workbench with KamoteKeys embedded |
| `devices\smk37\build\pro\generated\SMK37_PRO_OPEN.fwsc`, `devices\fm1\build\dx7\generated\FM1_OFFICIAL_OPEN.fwsc` | the firmware packages |
| `devices\<device>\build\...\sdk.elf`, `sdk.map`, `build-identity.json` | linked image, memory map, build id |

Firmware image only (no workbench): `.\tools\Sync-Core.ps1 -Device smk37`, then `.\devices\smk37\sdk\Build.ps1 -Model pro -FirmwareOnly -Toolchain 'C:\JL\pi32'` (FM-1: `-Device fm1`, then `.\devices\fm1\sdk\Build.ps1 -FirmwareOnly -Toolchain 'C:\JL\pi32'`).

### What to check in the build log

The SDK script performs these checks and stops on failure: toolchain and SDK hashes, the DSP ABI, compile and link (unresolved symbols fail the link), `Test-Firmware`, and the package size ("Offline package fits 1 MiB: image …, VM …, user … bytes"). Record:

- the **image size** line and the `.text`/`.data`/`.bss` totals from `build\pro\sdk.map`;
- the update identity `SMK-37 Pro_8000001` / `SMK-37 Elite_8000001` (unchanged from SMK37 OPEN V01);
- the package SHA-256 from `build\official-web\pro\SHA256SUMS.txt`.

Size expectation: on an ARM Thumb `-Oz` proxy compile, KamoteKeys's UI and controls are about 26.6 KB of code against 50.1 KB for the SMK37 OPEN UI they replace, and about 10.2 KB of static RAM against 13.4 KB. The router and storage additions add about 0.3 KB of code and 0.8 KB of RAM. The image should therefore be **smaller** than SMK37 OPEN V01, but only the pi32 build log is authoritative.

### Copy-and-paste build in Parallels

Put `KamoteKeys-0.54.zip` in the Mac's Downloads folder, then paste into Windows PowerShell (change the device in the `build.bat` line and the output path for Elite or FM-1, see the table above):

```powershell
cd $env:USERPROFILE\Desktop
Copy-Item \\Mac\Home\Downloads\KamoteKeys-0.54.zip . -Force
Expand-Archive .\KamoteKeys-0.54.zip . -Force
Expand-Archive .\KamoteKeys-0.54\kamotekeys-0.54-source.zip . -Force
cd .\kamotekeys-0.54
$env:FM1_PI32_TOOLCHAIN = 'C:\JL\pi32'
$env:FM1_SOURCE_URL = 'https://localhost/kamotekeys-0.54-source.zip'
.\build.bat smk37-pro --package
Copy-Item .\devices\smk37\build\official-web\pro\SMK37-Open-Source.html \\Mac\Home\Downloads\SMK37-KamoteKeys-0.54.html -Force
```

Add `$env:FM1_WASI_SDK = '<folder>\build\tools\wasi-sdk-27.0-x86_64-windows'` (from an earlier build) before `build.bat` to skip the 518 MB download.

## 3. Flash

1. **Back up** with the official SMK37 OPEN workbench: projects, DX7 voice groups A–D and the sample bank. KamoteKeys does not erase them, but keep backups for any firmware change.
2. Open your model's `SMK37-Open-Source.html` in desktop **Chrome or Edge**. Connect the SMK-37 by USB and allow MIDI and SysEx access.
3. Select **Device update** and follow the prompts. Migrating from M-VAVE production firmware uses force mode, and a loader connection asks you to confirm the physical model.
4. Keep USB connected and the device powered until the firmware and sample checks finish. Then power-cycle.
5. Expected first boot: the Aurora animation (~1.1 s), then the KamoteKeys main screen. Run the [first-flash checklist](TESTING.md#first-flash-checklist-on-hardware).

Never flash a Pro package onto an Elite or the reverse; the loader name alone does not identify the model.

## 4. Protected areas

KamoteKeys changes only application code. It does not touch the boot loader (`uboot.boot`, `ota.bin`), the VM/BTIF area holding Bluetooth bonds and radio data, the factory calibration records (`smk37_syscfg_*`, VM ids 51–54, read-only here), or the flash layout (`user_bytes = 536576`). Its settings are an extra 24-byte record appended inside SMK37 OPEN's existing journaled tuning/session sector. The journal accepts records written by the old and new layouts, so an interrupted save keeps the previous settings.

## 5. Recovery

| Situation | Action |
|---|---|
| Want official SMK37 OPEN back | Open <https://open.m-vave.com/smk37-workbench> → your model → **Device update**. Same identity family, normal update. |
| Want M-VAVE factory firmware | Workbench → **Restore factory V16** (the bundled `sdk/firmware/SMK37-<model>-V16.fwsc` is hash-checked against the official 2026-08-12 download). Power-cycle after resource verification. **Switch to Open firmware** returns to the Open flow. |
| KamoteKeys does not boot / blank screen | Power off, reconnect USB, open the workbench and connect. If the application does not respond, the workbench talks to the loader: confirm the model and flash again (KamoteKeys, official SMK37 OPEN or factory V16). |
| Samples show EMPTY in other firmware | Workbench → **Check / initialize samples** (no reflash needed). |
| Bluetooth will not re-pair | BT menu → E7 to `FORGET MIDI PAIRS` → ▶ twice; also remove the device on the computer. |
| Settings misbehave | BPM/ARP settings are rewritten on the next change; an invalid record is ignored at boot (defaults: 120 BPM, ARP off, 1/8, 75 %). |

These recovery paths are SMK37 OPEN's and are unchanged by KamoteKeys; they have not been exercised with a KamoteKeys build on hardware.
