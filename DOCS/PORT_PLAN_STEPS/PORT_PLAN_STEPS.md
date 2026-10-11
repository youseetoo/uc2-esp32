# UC2-ESP32 — ESP-IDF 6.1 Port Plan

> **Pure ESP-IDF 6.1 build. No PlatformIO. No shims. Direct IDF calls.**

## Strategy

| Aspect | Decision |
|--------|----------|
| **Build system** | ESP-IDF 6.1 (CMake + Ninja), `idf.py` |
| **Arduino APIs** | Converted to direct IDF calls (no shim layer) |
| **PlatformIO** | Not used (not in build, not in CI) |
| **Target** | ESP32-S3 (laser), ESP32-C3 (slaves), ESP32 (masters) |
| **CI** | Single `idf-build` job; grows one app per verified board |

## Phases

1. **Phase 1** — Build system + laser app (T1.1–T1.8)
2. **Phase 2** — All non-BT boards (T2.1–T2.12)
3. **Phase 3** — BT boards (T3.1–T3.8)
4. **Phase 4** — C-ify first-party modules (T4.1–T4.8)
5. **Phase 5** — Vendor leaves (T5.1–T5.5)
6. **Phase 6** — Cleanup + cutover (T6.1–T6.6)

## Step Index

| Step | Title | File |
|------|-------|------|
| T1.1 | Baselines + build env | [steps/T1.1.md](steps/T1.1.md) |
| T1.2 | CMake skeleton + Kconfig | [steps/T1.2.md](steps/T1.2.md) |
| T1.3 | Convert `main/` to direct IDF | [steps/T1.3.md](steps/T1.3.md) |
| T1.4 | Kconfig feature gating | [steps/T1.4.md](steps/T1.4.md) |
| T1.5 | App skeleton (laser) | [steps/T1.5.md](steps/T1.5.md) |
| T1.6 | Port laser app to direct IDF | [steps/T1.6.md](steps/T1.6.md) |
| T1.7 | Size baseline (laser) | [steps/T1.7.md](steps/T1.7.md) |
| T1.8 | CI: `idf-build` job (laser) | [steps/T1.8.md](steps/T1.8.md) |

*(T2.x–T6.x: see the `steps/` directory for the full list.)*

## Ref Index

| Ref | Title | File |
|-----|-------|------|
| hal-shim | Arduino → IDF API mapping | [refs/hal-shim.md](refs/hal-shim.md) |
| component-layout | Target component layout | [refs/component-layout.md](refs/component-layout.md) |
| env-app-mapping | PIO env → IDF app mapping | [refs/env-app-mapping.md](refs/env-app-mapping.md) |
| ci-cutover | CI cutover mechanics | [refs/ci-cutover.md](refs/ci-cutover.md) |
| risk-rollback | Risks + rollback strategy | [refs/risk-rollback.md](refs/risk-rollback.md) |
