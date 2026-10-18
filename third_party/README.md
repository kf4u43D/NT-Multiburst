# Included SDK

This repository includes the five public `distingNT_API` headers and their MIT
license, pinned to commit `f440d20c2a96a9ca43892c7418a1d8185ad47eb5` (API 13).
This minimal snapshot is sufficient to build the engine, plugin and tests.
The API files are unmodified; their origin and the `api.h` hash are recorded in
[sdk.lock.json](../sdk.lock.json).

Upstream: [Expert Sleepers distingNT_API](https://github.com/expertsleepersltd/distingNT_API/tree/f440d20c2a96a9ca43892c7418a1d8185ad47eb5).
License: [distingNT_API/LICENSE](distingNT_API/LICENSE).

The original workspace's `nt_mcp` and `nt_emu` reference clones are not build
dependencies and are not distributed here. Rack tests require separate
installations of Rack, its SDK, Fundamental and the nt_emu plugin.
