# Production content is not present

No Unreal Editor is installed in the development host. Consequently this folder
contains no authored `.uasset` or `.umap` packages. No text file is disguised as
an Unreal asset. No placeholder vehicle or garage is represented as a finished
visual. The C++ catalogs represent mechanical **state**, not modeled geometry.

Follow `Docs/CONTENT-INTEGRATION.md` inside Unreal Engine 5.7.4 on Windows.
Only the Editor may generate the missing packages. Fill the release manifest
with real paths and documented rights after importing, inspecting and authoring
the assets. `Build-Windows.ps1 -Mode Shipping` intentionally rejects this
checkpoint until those content and QA gates are satisfied.
