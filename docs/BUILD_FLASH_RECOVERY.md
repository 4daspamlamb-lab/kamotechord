# Build, flash and recovery

KamotKeys uses SMK37 OPEN's native build system unchanged. Nothing under `sdk/` is modified: `sdk/manifest.json` hash-locks the SDK, the build script and the toolchain, and the build refuses to run if any of them differ.

## 1. Requirements

- Windows x64, PowerShell, `curl.exe`, `tar.exe`.
- [JieLi Windows toolchain 2.5.3](https://jl-update.oss-cn-shenzhen.aliyuncs.com/2.5.3.exe) (PI32v2 clang 4.0.1, target `r3`); [installation guide](https://doc.zh-jieli.com/Tools/zh-cn/dev_tools/dev_env/jl_toolchain.html). Note the `pi32` directory that contains `bin\clang.exe`.
- For the workbench package: an HTTPS URL of the **matching source archive** (GPL requirement, enforced by the script). A tagged GitHub archive of your repository works, e.g. `https://github.com/<you>/kamotkeys/archive/refs/tags/v0.37.zip`.
- The first web build downloads WASI SDK 27 (or set `FM1_WASI_SDK`).

## 2. Build

```powershell
cd smk37_open
$env:FM1_PI32_TOOLCHAIN = 'C:\JL\pi32'
$env:FM1_SOURCE_URL     = 'https://github.com/<you>/kamotkeys/archive/refs/tags/v0.37.zip'
.\build.bat pro --package        # or: .\build.bat elite --package
```

Outputs:

| File | Use |
|---|---|
| `build\official-web\pro\SMK37-Open-Source.html` | offline workbench with KamotKeys embedded: **this is what you flash with** |
| `build\pro\generated\SMK37_PRO_OPEN.fwsc` | the firmware package |
| `build\pro\sdk.elf`, `build\pro\sdk.map` | linked image and memory map |
| `build\pro\build-identity.json` | build id shown by the workbench |

Firmware image only (no workbench): `.\sdk\Build.ps1 -Model pro -FirmwareOnly -Toolchain 'C:\JL\pi32'`.

### What to check in the build log

The SDK script performs these checks and stops on failure: toolchain and SDK hashes, the DSP ABI, compile and link (unresolved symbols fail the link), `Test-Firmware`, and the package size ("Offline package fits 1 MiB: image …, VM …, user … bytes"). Record:

- the **image size** line and the `.text`/`.data`/`.bss` totals from `build\pro\sdk.map`;
- the update identity `SMK-37 Pro_8000001` / `SMK-37 Elite_8000001` (unchanged from SMK37 OPEN V01);
- the package SHA-256 from `build\official-web\pro\SHA256SUMS.txt`.

Size expectation: on an ARM Thumb `-Oz` proxy compile, KamotKeys's UI and controls are about 26.6 KB of code against 50.1 KB for the SMK37 OPEN UI they replace, and about 10.2 KB of static RAM against 13.4 KB. The router and storage additions add about 0.3 KB of code and 0.8 KB of RAM. The image should therefore be **smaller** than SMK37 OPEN V01, but only the pi32 build log is authoritative.

### Copy-and-paste build in Parallels

Put `KamotKeys-0.37.zip` in the Mac's Downloads folder, then paste into Windows PowerShell:

```powershell
cd $env:USERPROFILE\Desktop
Copy-Item \\Mac\Home\Downloads\KamotKeys-0.37.zip . -Force
Expand-Archive .\KamotKeys-0.37.zip . -Force
Expand-Archive .\KamotKeys-0.37\kamotkeys-0.37-source.zip . -Force
cd .\kamotkeys-0.37-smk37_open
$env:FM1_PI32_TOOLCHAIN = 'C:\JL\pi32'
$env:FM1_SOURCE_URL = 'https://localhost/kamotkeys-0.37-source.zip'
.\build.bat pro --package
Copy-Item .\build\official-web\pro\SMK37-Open-Source.html \\Mac\Home\Downloads\SMK37-KamotKeys-0.37.html -Force
```

Add `$env:FM1_WASI_SDK = '<folder>\build\tools\wasi-sdk-27.0-x86_64-windows'` (from an earlier build) before `build.bat` to skip the 518 MB download.

## 3. Flash

1. **Back up** with the official SMK37 OPEN workbench: projects, DX7 voice groups A–D and the sample bank. KamotKeys does not erase them, but keep backups for any firmware change.
2. Open your model's `SMK37-Open-Source.html` in desktop **Chrome or Edge**. Connect the SMK-37 by USB and allow MIDI and SysEx access.
3. Select **Device update** and follow the prompts. Migrating from M-VAVE production firmware uses force mode, and a loader connection asks you to confirm the physical model.
4. Keep USB connected and the device powered until the firmware and sample checks finish. Then power-cycle.
5. Expected first boot: the Aurora animation (~1.1 s), then the KamotKeys main screen. Run the [first-flash checklist](TESTING.md#first-flash-checklist-on-hardware).

Never flash a Pro package onto an Elite or the reverse; the loader name alone does not identify the model.

## 4. Protected areas

KamotKeys changes only application code. It does not touch the boot loader (`uboot.boot`, `ota.bin`), the VM/BTIF area holding Bluetooth bonds and radio data, the factory calibration records (`smk37_syscfg_*`, VM ids 51–54, read-only here), or the flash layout (`user_bytes = 536576`). Its settings are an extra 24-byte record appended inside SMK37 OPEN's existing journaled tuning/session sector. The journal accepts records written by the old and new layouts, so an interrupted save keeps the previous settings.

## 5. Recovery

| Situation | Action |
|---|---|
| Want official SMK37 OPEN back | Open <https://open.m-vave.com/smk37-workbench> → your model → **Device update**. Same identity family, normal update. |
| Want M-VAVE factory firmware | Workbench → **Restore factory V16** (the bundled `sdk/firmware/SMK37-<model>-V16.fwsc` is hash-checked against the official 2026-08-12 download). Power-cycle after resource verification. **Switch to Open firmware** returns to the Open flow. |
| KamotKeys does not boot / blank screen | Power off, reconnect USB, open the workbench and connect. If the application does not respond, the workbench talks to the loader: confirm the model and flash again (KamotKeys, official SMK37 OPEN or factory V16). |
| Samples show EMPTY in other firmware | Workbench → **Check / initialize samples** (no reflash needed). |
| Bluetooth will not re-pair | BT menu → E7 to `FORGET MIDI PAIRS` → ▶ twice; also remove the device on the computer. |
| Settings misbehave | BPM/ARP settings are rewritten on the next change; an invalid record is ignored at boot (defaults: 120 BPM, ARP off, 1/8, 75 %). |

These recovery paths are SMK37 OPEN's and are unchanged by KamotKeys; they have not been exercised with a KamotKeys build on hardware.
