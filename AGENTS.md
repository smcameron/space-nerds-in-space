# AGENTS.md

Guidance for AI coding agents (Claude Code, Gemini CLI, Codex, Cursor, Aider, and
others) working in the Space Nerds In Space repository.

`CLAUDE.md`, `GEMINI.md`, and `.cursorrules` are symlinks to this file. Edit this file,
not them.

`CONTRIBUTING.md` is authoritative for policy and style. This file is the operational
companion: it tells an agent how to work here without rediscovering it every session.

---

## 1. AI contribution policy

AI-written code is permitted **on a case by case basis** — not blanket-approved. It is
held to the ordinary development process (sane one-logical-change commits, Linux-kernel
coding style, patches that build and work) *plus* the extra obligations below. See the
`AI Policy` section of `CONTRIBUTING.md`, which is authoritative.

### Licensing

All contributions must be compatible with **GPL v2.0**. If an agent reproduces code it
did not derive from this repository, say so — provenance matters more than convenience.

### Signed-off-by and the Developer Certificate of Origin

**AI agents MUST NOT add `Signed-off-by:` tags.** Only a human can legally certify the
Developer Certificate of Origin. The human submitter is responsible for:

- Reviewing and testing all AI-generated code thoroughly
- Ensuring compliance with licensing requirements
- Adding their own `Signed-off-by:` tag to certify the DCO
- Taking full responsibility for the contribution

That last point has a blunt corollary in `CONTRIBUTING.md`: *if you don't understand the
code, do not submit it.* The practical obligation this puts on an agent is to write code
the human can actually follow, and to explain what was written and why — clever and
opaque is worse than plain and reviewable here.

### Attribution

When AI tools contribute, add an `Assisted-by:` tag naming the assistant:

```
Assisted-by: Claude Opus 5
```

That is the form `CONTRIBUTING.md` gives, and it is the form to use. If specialized
analyzers were run (cppcheck, scan-build, glslangValidator, valgrind), mention them in
the commit body prose rather than inside the trailer. Do not mention ordinary tools like
git, gcc, make, or editors.

The human adds both this tag and the sign-off when committing.

### Art assets

The policy above pertains to AI-generated **code**. `CONTRIBUTING.md` says of
AI-generated art assets — images, textures, 3D models, normal maps, sound effects, music,
voice acting — that the maintainer has not yet made up his mind, and that the project
will cross that bridge when it comes to it.

So this is an open question, not a rule in either direction, and where exactly the line
between code and art asset falls is itself undecided. Don't read the silence as
permission or as prohibition; it is the human's call to make.

---

## 2. Rules of engagement

**Do not commit.** Make the edits, run the verification steps, and show the diff. The
human makes every commit, so that authorship and the DCO sign-off are unambiguously
human. This is not a formality here — it is how the licensing story stays clean.

Do propose a commit message. The house form is a subsystem prefix, a colon, and a
lowercase imperative summary:

```
graph_dev: bend the skybox around black holes
entity: only opaque lit surfaces cast into the shadow map
mathutils: fix disc_occlusion_fraction() for a degenerate first disc
```

Keep each change to **one logical thing**. That does not mean one file — it means one
feature or one fix. Every commit must build and work on its own; someone bisecting
through the history should never land on a broken tree.

Once the human has committed, patches for upstream come from a topic branch:

```
git format-patch master..HEAD -o /tmp/snis-patches
```

Upstream submission routes, per `CONTRIBUTING.md`, are a bzip2'd tarball of a
[stacked git](http://www.procode.org/stgit/) series by email (preferred by the
maintainer) or a GitHub pull request. `stg` is not installed by default on this
machine; plain git branches are fine for local work.

---

## 3. Verification gate

Run these before claiming a change works. If a step was skipped, **say so plainly**
rather than describing the change in a way that implies it passed.

### 3.1 Build clean

```
make
```

No new warnings. The build is normally quiet; use `make V=1` to see the commands.

### 3.2 Style check

```
git diff | ./checkpatch.pl -
```

`checkpatch.pl` is in-tree and is the arbiter of style disputes. Clean it up before
handing the diff over.

### 3.3 Unit tests

```
make test
```

This builds and runs the core suite: matrix, space-partition, marshal, quat, fleet,
mtwist, commodities, solarsystem_config.

Many tests are a source module recompiled with a `-DTEST_*` macro into a standalone
binary. To run one:

```
make bin/test-quat && bin/test-quat
```

When you touch a pure, testable module — math, parsers, marshalling — extend the
existing test or add one in the same style. When you touch a fuzzed parser, the
`fuzz_*.c` files and `make run-fuzz-*` targets exist for a reason.

### 3.4 GLES build

Required whenever renderer or shader code changes:

```
make mostly-clean && make USE_GLES=1
```

The `make mostly-clean` is not optional. Object files all land in one shared directory
(`OD=object_files` in the Makefile), so switching `USE_GLES` without cleaning links a
mixture of desktop-GL and GLES objects, which fails confusingly or, worse, doesn't.
Clean again before going back to the desktop build.

There is also a "clean" makefile target. The difference between "clean" and
"mostly-clean" is that "make clean" wipes out some .stl files, and when
building, OpenSCAD will be invoked to regenerate them, which takes a long time
and is usually not necessary.  "mostly-clean" leaves these .stl files alone. In
any case, the .stl files may be fetched when assets are updated.  Unless you
are working on the .scad OpenSCAD files, you do not need to use "make clean",
and you should use "make mostly-clean" instead.

### 3.5 Smoke run

Actually start the thing and confirm it does not immediately die.

- Full game: `./snis_launcher` (interactive menu — fetch assets, start servers, launch
  clients). `./killem.sh` stops everything afterwards.
- Rendering work is much faster to check in the standalone tools: `bin/shadow_lab`,
  `bin/mesh_viewer`.

An agent cannot play the game. Starting it, watching for a clean startup, and checking
stderr is the achievable bar — meet it, and be honest that gameplay was not exercised.

---

## 4. Build reference

Dependencies: `util/install_dependencies` (apt-focused, `--dry-run` to preview). Core
deps are portaudio, libpng, libvorbis, SDL2, and lua5.2, plus optional opus, espeak, and
pico for audio and text-to-speech.

Targets:

| Target | Effect |
| --- | --- |
| `make` | Everything — client, servers, tools. Binaries land in `bin/`. |
| `make serversonly` | Just `bin/ssgl_server`, `bin/snis_server`, `bin/snis_multiverse`. |
| `make utils` | `bin/mesh_viewer`, `bin/shadow_lab`, `bin/star_light_preview`, `bin/earthlike`, and friends. |
| `make models` | Rebuild 3D models from OpenSCAD sources. Slow, needs `openscad`; normally you download prebuilt assets instead. |
| `make test` | Core unit tests. |
| `make clean` / `make mostly-clean` | Clean build artifacts. |
| `make depend` | Regenerate `Makefile.depend`. |
| `make cppcheck` / `make scan-build` | Static analysis (neither tool is installed by default). |
| `make run-fuzz-*` | libFuzzer runs; corpus in `fuzztests/`. |

Knobs:

| Variable | Meaning |
| --- | --- |
| `USE_GLES=1` | Build against OpenGL ES instead of desktop GL. Required for Raspberry Pi. See §3.4. |
| `O=0` | Debug build (`-g`). Default `O=1` is `-O3` with no debug info. |
| `V=1` | Verbose — print the compiler commands. |
| `ASAN=1` / `UBSAN=1` | AddressSanitizer / UndefinedBehaviorSanitizer. `UBSAN=1` switches `CC` to clang. Both are disabled on aarch64. |
| `P=1` | Build with gprof profiling. |
| `DOWNLOAD_OPUS=yes` | Have the Makefile fetch and build Opus if your distro lacks packages. |
| `WITHVOICECHAT=no` | Build without voice chat. |

---

## 5. Gotchas

These are the things that waste hours. Read this section before touching assets or the
renderer.

### 5.1 The game does not read assets from the repository

This is the single most common way to waste an afternoon. By default the client loads
assets from **`~/.local/share/space-nerds-in-space/share/snis`**, not from the
`share/snis` directory in your git checkout. Editing a shader, texture, model, or Lua
script in the repo and re-running the game **changes nothing**.

Resolution order is in `override_asset_dir()` in `snis_asset_dir.c`:

1. `$SNIS_ASSET_DIR` if set — wins outright
2. else `$XDG_DATA_HOME/space-nerds-in-space/share/snis` if `XDG_DATA_HOME` is set
3. else `$HOME/.local/share/space-nerds-in-space/share/snis`
4. else the compiled-in `default_asset_dir`

So when iterating on in-repo assets, point the game at your checkout:

```
SNIS_ASSET_DIR=$PWD/share/snis bin/snis_client
```

Other asset facts:

- Art assets are **not in the repo**. Get them with the client's UPDATE ASSETS button or
  `bin/snis_update_assets`. Nothing renders properly before that.
- `replacement_assets.txt` and the `replacement-files/` directory in the installed asset
  tree let downloaded assets override others — another way an edit can appear to have no
  effect.
- `./snis_launcher` is the way to run things. The `quickstart` and `nolobby-quickstart`
  scripts are **obsolete** and exit immediately.
- `./killem.sh` stops all running SNIS processes.

### 5.2 There are two shader trees, and two renderer backends

Shaders are runtime data files, not compiled into the binary — so §5.1 applies to them
in full.

There are two parallel trees:

- `share/snis/shader/` — desktop OpenGL, GLSL **150**, used by `graph_dev_opengl.c`
- `share/snis/shader-es/` — OpenGL ES, GLSL **100**, used by `graph_dev_gles.c`
  (see `default_shader_directory` in that file)

**A shader change usually has to land in both trees.** They are separate files, and
nothing warns you when they drift apart.

Likewise, `graph_dev_opengl.c` and `graph_dev_gles.c` are two independent
implementations of the same interface (`graph_dev.h`). A feature added to one is simply
absent from the other until someone ports it. The GLES backend rots silently because the
default build never compiles it — hence the mandatory `make USE_GLES=1` check in §3.4.

File conventions:

- A `.shader` file contains both stages, selected by `-DINCLUDE_VS` / `-DINCLUDE_FS`.
- Separate `.vert` / `.frag` pairs are the older style; both are in use.
- `filmic.glsl` is prepended to shaders as a common prologue.
- `csm.shader` holds the shared cascaded-shadow-map GLSL.

Validate shaders without running the game:

```
util/validate-shaders.sh
```

It runs `glslangValidator` over both trees at the correct GLSL versions, prepending
`filmic.glsl` the same way the engine does. Requires `glslangValidator` installed.

### 5.3 Protocol changes have a checklist

Skipping a step here produces silent desync or clients that refuse to connect, with no
useful error. Full detail in `doc/how-to-add-new-opcode.txt`:

1. Choose an opcode value in `snis_packet.h`
2. Implement send/receive in `snis_client.c` / `snis_server.c`
3. Add the format definition in `snis_opcode_def.c`
4. Add or adjust any struct in `snis_packet.h`
5. **Bump `SNIS_PROTOCOL_VERSION` in `snis.h`** — client and server must agree
6. Update `snis_entity_key_value_specification.h` if the data persists across saves or
   warp gates
7. Update `snis_bridge_update_packet.c` and `UPDATE_BRIDGE_PACKET_SIZE` in
   `snis_bridge_update_packet.h`
8. Update `doc/snis-protocol.html`

Serialization primitives live in `snis_marshal.c`.

### 5.4 Threading and the client/server split

`snis_server` runs a lobby-heartbeat thread, a `listener_thread()` for new connections,
and a read thread plus a write thread per connected client. Shared state — chiefly the
`go[]` game-object array — is touched from several of them. Do not add unlocked access,
and do not block in the per-client threads.

**Golden rule for features:** all meaningful game logic happens server-side. The client
only sends requests and renders what comes back. A feature implemented client-side will
look right on one screen and be invisible or wrong on the other five.

---

## 6. Architecture

Space Nerds In Space is a multiplayer networked "spaceship bridge simulator" written in
C. Several machines each run one crew-station screen — Navigation, Weapons, Engineering,
Damage Control, Comms, Science, Main View, or Game Master ("Demon") — all connected to a
shared server simulating one solar system.

Processes (separate binaries, connected over TCP/UDP):

- **`snis_server`** (`snis_server.c`) — authoritative simulation of one solar system: all
  game objects, physics, AI, damage, rules. Clients only *request* actions; the server
  decides outcomes and broadcasts state.
- **`snis_client`** (`snis_client.c`) — renders one bridge station, sends input to the
  server. Custom vector-drawn widgets over OpenGL/GLES; the GTK scaffolding is mostly
  incidental.
- **`snis_multiverse`** (`snis_multiverse.c`) — persists ship state and moves ships
  between `snis_server` instances (different solar systems) through warp gates. This is
  what makes the universe arbitrarily large.
- **`ssgl_server`** (in `ssgl/`) — the lobby: a directory service where servers register
  and clients discover them. SSGL is "Simple Server Game Lobby", a self-contained
  sub-library.

Server landmarks (`snis_server.c`): `main()`, `move_objects()` (the simulation tick),
`listener_thread()`, `per_client_read_thread()`, `per_client_write_thread()`,
`process_instructions_from_client()`, `queue_up_client_updates()`,
`write_queued_updates_to_client()`. Key arrays: `go[]` (game objects, `struct
snis_entity`), `client[]` (connections), `bridgelist[]` (crews);
`bridgelist[].damcon` holds the separate damage-control sub-game.

Client landmarks (`snis_client.c`): `advance_game()` runs ~30x/sec, `main_da_expose()`
draws everything, `ui_element_list_draw()`, and the `init_*_ui()` functions that build
each screen's widgets. Key arrays: `uiobjs`, `go[]`, `dco[]`.

Deeper guide: `doc/hacking-space-nerds-in-space.html`.

---

## 7. Mission scripting

Missions are Lua scripts in `share/snis/luascripts/` (see `MISSIONS/` and `TEST/`). The
API is documented in `doc/lua-api.txt`; the server exposes callbacks via
`snis_event_callback.c`. This is the intended extension point for new scenarios and
requires no C changes at all — prefer it when the request is "add a mission" rather than
"change the engine".

---

## 8. Coding style

Linux-kernel style, loosely. `./checkpatch.pl` settles arguments. The essentials:

- Indent with **tabs**. Tabs are 8 wide, and this is about as negotiable as the value
  of pi.
- `lowercase_with_underscores` for identifiers, `UPPERCASE` for macros. No camelCase.
- If either arm of an `if`/`else` needs braces, both get them. If neither needs them,
  neither gets them.
- Plain C only. No C++, and **no `//` comments**.
- Functions are `static` by default unless deliberately part of a module's public
  interface.
- Headers should declare only types and prototypes, using the `GLOBAL` macro pattern so
  the implementation includes the same header as its users. See `wwviaudio.h`.
- Avoid `typedef` except for function pointers.
- Long lines are tolerated more than in the kernel, but still worth avoiding.

Write code that reads like the code around it. Match the local comment density and
naming; do not import conventions from elsewhere.

---

## 9. Map of the tree

Most of these are standalone and reusable.

- **Math and geometry:** `quat.c`, `matrix.c`, `mathutils.c`, `vec4.c`,
  `oriented_bounding_box.c`, `shape_collision.c`, `elastic_collision.c`,
  `liang-barsky.c`, `a_star.c`, `space-part.c` (spatial partitioning)
- **Graphics:** `entity.c` (scene graph), `mesh.c`, `graph_dev_opengl.c`,
  `graph_dev_gles.c`, `material.c`, `shader.c`, `snis_graph.c`, `star_light.c`,
  `smaa/`, `mikktspace/`
- **Parsing and config:** `key_value_parser.c`, `solarsystem_config.c`, `commodities.c`,
  `stl_parser.c`, `joystick_config.c`, `starbase_metadata.c`, `read_menu_file.c`
- **Networking:** `snis_marshal.c`, `snis_socket_io.c`, `net_utils.c`,
  `snis_opcode_def.c`, `ssgl/`
- **Audio and speech:** `wwviaudio.c`, `ogg_to_pcm.c`, `snis_voice_chat.c`,
  `pronunciation.c`, `snis_text_to_speech.sh`
- **Procedural generation:** `open-simplex-noise.c`, `nebula_noise.c`, `earthlike.c`,
  `planetary_atmosphere.c`, `crater.c`, `mtwist.c` (PRNG), `names.c`, `infinite-taunt.c`
- **UI widgets:** `snis_button.c`, `snis_sliders.c`, `snis_gauge.c`, `snis_text_input.c`,
  `snis_text_window.c`, `snis_pull_down_menu.c`, `snis_strip_chart.c`, `snis_font.c`
- **Natural language:** `snis_nl.c` (see `doc/snis_nl.txt`), `spelled_numbers.c`
- **Hardware integration:** `snis-device-io.c`, `snis_arduino.c`, `joystick.c`,
  `snis_dmx.c`, `arduino/`, `device-io-sample-1.c`
- **Standalone tools:** `mesh_viewer.c`, `shadow_lab.c`, `star_light_preview.c`,
  `generate_skybox.c`, `print_ship_attributes.c`, plus scripts in `util/`

Other documentation lives in `doc/`: `lua-api.txt`, `how-to-add-new-opcode.txt`,
`snis-protocol.html`, `howto-add-new-solarsystems`,
`howto-generate-earthlike-planets.txt`, `star-rendering-and-lighting-notes.txt`,
`lobbyless-operation.txt`, `running-in-the-cloud.txt`, `IDEAS.txt`.
