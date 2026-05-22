# AGENTS.md (MxMTD)

## Build/Run Reality
- This repo is intended to be built/run from a **Xilinx Vitis GUI project** (it includes Xilinx headers like `xil_printf.h`, `xil_cache.h`, `xtime_l.h`).
- There is **no standalone build system here** (no `Makefile`, `CMakeLists.txt`, CI workflows). Don’t invent one; use the Vitis project’s BSP/include settings.

## Architecture (Where Code Lives)
- `mxmtd/`: core flash operations + per-flash implementations (`spinor/`, `spinand/`, `rawnand/`).
- `platform/`: uniform transfer interface (`xfer_info_t`, `platform_xfer()`) + compile-time host-controller selection.
- `platform/vendors/`: host controller implementation(s). Current default is Macronix UEFC.
- `testbench/`: interactive test menus and stress/verification routines.
- `main.c`: chooses which tests to run based on compile-time `CONF_*` defines.

## Real Entrypoints / Wiring
- App entry: `main.c` calls `rawnand_test()`, `spinand_test()`, `spinor_test()` depending on `CONF_RAWNAND/CONF_SPINAND/CONF_SPINOR` in `mxmtd/inc/mxmtd_config.h`.
- Driver setup/teardown: `mxmtd_setup_dev()` / `mxmtd_release()` in `mxmtd/mxmtd.c`.
- Platform dispatch: `setup_platform()` and `platform_xfer()` in `platform/src/platform_itf.c`.
  - Host controller is chosen by compile-time `CONF_HC_TYPE` in `platform/inc/platform_conf.h`.
- Pass-through (bypass mxmtd ops; talk directly to host controller via packet API): `testbench/src/pass_through.c`.

## Config Knobs That Change Behavior
- Flash selection + ECC + erase size + probe reset + ASP: `mxmtd/inc/mxmtd_config.h`.
- Host controller type + base/map addresses: `platform/inc/platform_conf.h`.
- Logging verbosity: `platform/inc/platform_print.h` via `CONF_PR_LEVEL`.

## High-Risk Gotchas
- `CONF_ENABLE_ASP` (in `mxmtd_config.h`) enables code paths that write **WPSEL in a Security Register** and is explicitly marked as an **OTP bit**. Don’t enable/exercise ASP flows unless permanent device changes are intended.
- Testbench includes destructive operations (e.g. full-chip erase):
  - `flash_erase_all()` in `testbench/src/flash_test.c`.
  - Menus also include whole-chip erase/program/read verification.
  Treat any run as operating on real attached flash hardware.

## If You Add A New Host Controller
- Update the `switch (CONF_HC_TYPE)` in `platform/src/platform_itf.c` (both `setup_platform()` and `release_platform()`).

## Quick “Where To Debug” Map
- Flash probe/setup and operation sequencing: `mxmtd/mxmtd.c` and `mxmtd/{spinor,spinand,rawnand}/*`.
- Transfer execution + polling/timeout behavior: `platform/src/platform_itf.c`.
- UEFC register/DMA/cache behavior: `platform/vendors/macronix/src/mxic_uefc.c`.
