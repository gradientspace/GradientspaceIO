# Third-Party Headers (Private to GradientspaceIO)

These files are vendored single-file dependencies used **only** from `.cpp`
files inside `Private/MeshIO/`. They MUST NOT be included from any header
under `Public/` so that the dependency does not leak into the module's
public API.

## json.hpp — nlohmann/json

- Project: https://github.com/nlohmann/json
- Version: **3.12.0** (released 2025)
- Source: https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp
- License: MIT

To update, replace `json.hpp` with the latest single-include header from the
project's GitHub releases page and bump the version above.
