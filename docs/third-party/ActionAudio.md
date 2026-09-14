# Action-audio dependency notice

Octaryn's native action-audio owner uses these pinned dependencies:

- **OpenAL Soft 1.25.1**, statically linked for playback and mixing. Its source
  notices specify GNU Library General Public License version 2 or later.
  [Bundled license](OpenALSoft.txt) ·
  [Pinned source](https://github.com/kcat/openal-soft/tree/1.25.1).
- **miniaudio 0.11.25**, compiled into the client for PCM synthesis with device
  I/O disabled. Its license offers the Unlicense or MIT No Attribution.
  [Bundled license](miniaudio.txt) ·
  [Pinned source](https://github.com/mackron/miniaudio/tree/0.11.25).

This notice is copied to `Licenses/ActionAudio.md`; the local links refer to its
accompanying bundle files. The matching source checkouts are retained under
`build/dependencies/src/openalsoft` and `build/dependencies/src/miniaudio`.
Dependency pins and build options are recorded in
`cmake/Dependencies/ClientDependencies.cmake`.

This records dependency provenance and accompanies the upstream license texts.
Distribution qualification, including source and relinking materials for the
statically linked library, remains a separate release task.
