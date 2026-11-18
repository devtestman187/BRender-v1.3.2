# AMD FidelityFX Super Resolution 1.0 (FSR1)

Vendored, **unmodified** copies of the MIT-licensed AMD FidelityFX FSR 1.0
shader headers:

| file | upstream path | version | sha256 |
|---|---|---|---|
| `ffx_a.h` | `ffx-fsr/ffx_a.h` | 1.20210629 | `F60E2722FCD13989523B9164D776AB382B3692791767F3BF8BB19967F763F3FB` |
| `ffx_fsr1.h` | `ffx-fsr/ffx_fsr1.h` | 1.20210629 | `93C3922362EA7FC99CBCC698CA30C98DE4F8C246D1FBB0B09E015DDEF38CE3A5` |
| `LICENSE.txt` | `license.txt` | – | `DB089274CE766DA70F5B7D791029C3486F9F9E27C8C79C652689603D3192E802` |

Upstream repository: <https://github.com/GPUOpen-Effects/FidelityFX-FSR>
(reference FSR1 v1.20210629, master tree
`a21ffb8f6c13233ba336352bdff293894c706575`).

Do not edit these files in place: they are kept byte-identical to upstream so
they can be re-verified against the hashes above. The dethrace integration
lives in the parent directory:

- `fsr1_easu.frag.glsl` / `fsr1_rcas.frag.glsl` — platform glue (gather
  emulation for GLSL 1.40 / ES 3.00, uniforms, entry points).
  `cmake/EmbedShader.cmake` inlines `ffx_a.h` / `ffx_fsr1.h` into these
  shaders at build time, because dethrace compiles its shaders from embedded
  C strings and has no GLSL include extension.
- `fsr1_constants.c` — CPU-side `FsrEasuCon` / `FsrRcasCon`.
- `shader_fsr1.c` — program compilation and the EASU + RCAS present passes.

License: MIT, Copyright (c) 2021 Advanced Micro Devices, Inc. See
`LICENSE.txt` (its text is also carried inside the compiled shader sources).
