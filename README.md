
[Space Nerds In Space](https://smcameron.github.io/space-nerds-in-space/ "Space Nerds In Space website")
is an open source multiplayer networked spaceship bridge
simulator game inspired by another game called "Artemis Spaceship Bridge Simulator"
It is still a work in progress, but it is substantially far along, and reasonably
free of bugs as far as I know (the game should not crash, in other words.)

Here is some video from February 2019, at HackRVA in Richmond, Virginia:

[![Watch the video](https://img.youtube.com/vi/3fFl0VH-4zA/hqdefault.jpg)](https://www.youtube.com/watch?v=3fFl0VH-4zA)

Features:

* Terminals for Navigation, Weapons, Engineering, Damage Control,
  Comms, Science, and Game Master.
* Multi-crew (multiple bridges) supported
* Terminals may join/leave/re-join games at any time.
* Asteroid mining
* Bounty hunting
* Travel between instances of the game via warp gates in an arbitrarily
  large universe of solar systems.
* Dock at starbases to repair your ship, buy/sell various commodities,
  sell mined ore, obtain warp gate tickets, etc.
* Lua scripting API for mission scenarios.
* Voice chat
* And of course, fly around and shoot stuff.

Note: This game is meant to be played on a LAN with all players in the same room.
Bandwidth requirements are on the order of 100k/s per client.

1. How to build the game from source (or see below):

	https://smcameron.github.io/space-nerds-in-space/#buildinstructions

2. How to run the game on a single machine (single player):

	https://smcameron.github.io/space-nerds-in-space/#singlemachineinstructions

3. How to run with a multi-player LAN setup:

	https://smcameron.github.io/space-nerds-in-space/#multiplayerinstructions

If you would like to help work on this project, see [CONTRIBUTING.md](https://github.com/smcameron/space-nerds-in-space/blob/master/CONTRIBUTING.md).
There is also a guide to the code here:
[Hacking Space Nerds In Space](http://htmlpreview.github.io/?https://github.com/smcameron/space-nerds-in-space/blob/master/doc/hacking-space-nerds-in-space.html).
Here is documentation for the
[Lua scripting API](https://github.com/smcameron/space-nerds-in-space/blob/master/doc/lua-api.txt)
intended to be used for creating "mission scripts".

Build instructions may be found here: [BUILD INSTRUCTIONS](https://smcameron.github.io/space-nerds-in-space/#buildinstructions)

