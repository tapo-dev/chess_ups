# chess_ups

KIV/UPS semester project: online chess for two players.

- **Server:** C++17, BSD sockets, single-threaded `poll()` loop
- **Client:** Unity (C#)
- **Protocol:** custom text protocol over TCP

The course assignment is in `docs/PozadavkyUPS.pdf` (Czech).

## How to help

The developer writes the application code personally: the C++ server, to learn C++, which is new to them, and the Unity client, which they already know well.

- Do not write the implementation. Suggest the next small step and give hints.
- Review what was written and fix bugs in it, always explaining what was wrong and why.
- Write whole functions or files only when explicitly asked to.
- Snippets of a few lines are fine to illustrate a point.
- For C++, explain the language and socket concepts involved. For Unity, skip the basics.

Everything else (Makefiles, devcontainer, docs) is normal work.

## Layout

| Path | Contents |
|---|---|
| `server/` | Chess server (C++, built with `make`) |
| `client/` | Unity client (Unity 6000.6, 2D URP template, new Input System only) |
| `docs/` | Course assignment, protocol description, final documentation |

`server/server.cpp` is a single-file skeleton grown out of the course's bonus task: `poll()` loop, per-client receive buffer split into lines (`read_lines`), `send_line`, and an empty `handle_line` that answers every line with `ERROR`. The chess protocol does not exist yet.

## Build and run

Development happens in the devcontainer: Debian 13, g++ 14, make, gdb, valgrind, netcat, port 10000 forwarded. No Python in the container. The Unity Editor runs on the host, outside the container.

```sh
cd server
make                          # build ./server (-std=c++17 -Wall -Wextra -g)
./server 127.0.0.1 10000      # <listen address> <port>
nc 127.0.0.1 10000            # talk to it by hand
make clean
```

Reachable from the university network: a school PC or `ares.fav.zcu.cz` (147.228.63.10), where students may use ports 10000-10010 and have little disk space. There, bind to `0.0.0.0`.

### Unity client

The Editor runs on the host and the `unity-cli` skill can drive it (Pipeline package is installed;
project path `client/`). This only works from a host session, not inside the devcontainer.
Use it to inspect and verify: compile errors, Console, scene hierarchy, play mode, tests, screenshots.
Do not create or modify scenes, GameObjects, prefabs or scripts through it unless asked.

## Server rules

- C++17. No C++20 features: the compiler version on the demo machines is unknown.
- Built with `make`. Never a shell script, a hand-typed `g++` command or an IDE build.
- BSD sockets only. No networking or serialization libraries, no C++ networking TS.
- One thread and one `poll()` loop. Nothing may block while waiting on a single client.
- TCP is a stream: one `recv()` can return half a message or several at once. Keep a receive buffer per client and split it on `\n`.
- Listen address, port, room limit and total player limit come from arguments or a config file, never hard-coded.
- No crashes and no hangs. Check every syscall result, survive a client vanishing at any moment, and never `exit()` because of one bad client.
- Invalid input disconnects the peer (a small strike counter, e.g. 3, is allowed). That covers random bytes, well-formed messages with nonsense values, messages in the wrong state and moves that break the rules of chess.
- Log player and game state changes and errors.
- Split code into modules/classes and comment it reasonably.
- Identifiers, comments and log messages are in English.

## Semester project requirements

Summary of `docs/PozadavkyUPS.pdf`.

**Protocol**
- Text, over TCP, no encryption.
- Every request gets a reply.
- Nothing is sent while nothing happens, except an occasional ping.

**Game flow**
- A player is identified by nickname.
- After connecting, the player is in a lobby with rooms, picks one and joins it. When the game ends, players go back to the lobby.
- Rooms run in parallel and do not affect each other.
- Both apps keep running across many games without a restart.

**Outages**
- Short outage: the client reconnects by itself and the game state is restored.
- Long outage: the client tells the player and requires a manual reconnect. The server removes the player from the game, the room ends and the opponent returns to the lobby.
- The opponent is told about both kinds of outage.

**Client**
- GUI, with fields for server address (IP or hostname) and port.
- Built from the Unity editor. The instructor approved this as an exception to the build-tool rule.
- Networking through the standard library only (`System.Net.Sockets`), no networking or serialization packages.
- The UI never freezes on the network (connecting, sending, waiting for a reply).
- All user input is validated. Illegal moves are rejected on the client as well as on the server.
- Always shows the current state: board, nicknames, whose turn it is, whether the opponent is reachable.
- Visibly reports an unreachable server (at start, in lobby, in game) and an unreachable opponent (in game).
- Handles invalid messages from the server the same way the server handles them from the client, and keeps a log.

**Demo setup:** server on Linux, one client on Linux, one client on Windows.

**Documentation** must cover: the game variant, the protocol (message format, data types, meaning of codes, value limits, state diagram, error states), the implementation of both apps (modules, layers, libraries, parallelization method), build requirements and steps, and a conclusion.

## Test tricks from the assignment

```sh
cat /dev/urandom | nc 127.0.0.1 10000              # client that sends garbage
iptables -A INPUT -p tcp --dport 10000 -j DROP     # simulate an outage (remote connections only); also try REJECT
iptables -D INPUT -p tcp --dport 10000 -j DROP     # remove the rule
```
