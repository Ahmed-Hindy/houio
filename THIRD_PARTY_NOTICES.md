# Third-party notices

This file records source-level third-party notices retained by HouIO. Bundled binary dependencies add their own notices under `share/houio/licenses` in generated packages.

## ILM/OpenEXR half-precision floating-point implementation

The upstream HouIO history included the OpenEXR `half` implementation. That implementation was removed from the active source tree and replaced by `include/houio/HalfFloat.h`. HouIO conservatively retains the original notice because the replacement serves the same half-precision conversion functionality and the historical implementation remains part of the repository history.

Copyright (c) 2002, Industrial Light & Magic, a division of Lucas Digital Ltd. LLC

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
- Neither the name of Industrial Light & Magic nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Primary authors named in the historical source:

- Florian Kainz
- Rod Bogart

## Bundled scene I/O dependencies

The optional bundled scene-I/O prefix contains OpenUSD, Alembic, Imath, and oneTBB. Their exact source revisions, versions, runtime SHA-256 values, and copied license/notice files are recorded by `tools/dependencies/build_scene_io.ps1` in `houio-scene-dependencies.json` and installed under `share/houio/licenses`.

These component licenses do not provide a project-wide license for HouIO. See [LICENSE_STATUS.md](LICENSE_STATUS.md).
