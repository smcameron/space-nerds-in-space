
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
* And of course, fly around and shoot stuff.
* Lua scripting API for mission scenarios.

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


BUILD INSTRUCTIONS
==================

These instructions are a duplicate of what is found here:
https://smcameron.github.io/space-nerds-in-space/#buildinstructions

Step 0: Acquire Hardware and OS
-------------------------------

This is a linux game. You will need a computer running linux. A raspberry pi doesn't count, it's not powerful enough. The game is currently developed using Mint 17.3 It is within the realm of possibility that the game could be made to build and work on Mac OSX (it has been done before, but not within the last couple years.) You're on your own if you want to try to run it on Mac.

Step 1: Install Dependencies
----------------------------

Build dependencies

Perform these steps as root:

```
    apt-get install build-essential;
    apt-get install portaudio19-dev;
    apt-get install libvorbis-dev;
    apt-get install libgtk2.0-dev;
    apt-get install libgtkglext1-dev;
    apt-get install liblua5.2-dev;
    apt-get install libglew1.5-dev;
    apt-get install libssl-dev;
    apt-get install libttspico-utils; # for text to speech
    apt-get install sox; # for "play" command, used by text to speech

    # (The following are optional)

    apt-get install espeak; # optional alternative to libttspico-utils
    apt-get install alsa-utils; # optional alternative to sox, for "aplay" command
    apt-get-install libsdl1.2-dev; # optional, only needed for mesh_viewer
    apt-get install openscad; # optional, only needed if you intend to work on 3D models.
                    # Also, it is recommended to get it from http://www.openscad.org/downloads.html
                    # as the version in the repos tends to be out of date.
    apt-get install git; # Version control, useful if you're hacking on the game
    apt-get install stgit; # Useful for making patches if you're hacking on the game. It's like "quilt" but on top of git.
```


Note: SDL should only be required if you want to build mesh_viewer, which is a utility program for viewing 3D models, and which is not required to run the game. mesh_viewer uses SDL 1.2, not SDL 2.x

If you want to try the optional pocketsphinx based local speech recognition, you will want the following packages:

```
    apt-get install pocketsphinx-utils;
    apt-get install pocketsphinx-lm-en-hub4;
    apt-get install pocketsphinx-lm-en-hub4;
    apt-get install libpocketsphinx1;
```

The above list may be incomplete, and these are the package names on mint 17.3 / ubuntu, so may be different on RPM based systems.

NOTE: problems building on SuSE Leap 15.1. The following problems/solutions have been conveyed to me regarding SuSE Leap 15.1. This information is not very complete or accurate, but it is the best I have for now. Please feel free to send me improvements to these instructions. See Bug 222.

* Need to install pkg-config and pkg-config_files.
* Need to set pkg-config-path environment variable
* All references to lua 5.2 in the Makefile need to be changed to lua 5.3. Note, we only compiled snis_client (via "make bin/snis_client") which shouldn't need lua at all. I don't know if the differences between lua 5.3 and lua 5.2 are significant enough to break things. I normally use lua 5.2 on my systems. Changes to Lua 5.3 -- at a quick glance I didn't see anything that I think will be problematic.

Step 2: Download the Source Code
--------------------------------

The source code is here: https://github.com/smcameron/space-nerds-in-space

NOTE: Do NOT perform these steps as root!

To get the source code, there are three methods:

    If you are a registered github user, type (as a non-root user):


```
        git clone git@github.com:smcameron/space-nerds-in-space.git
```


    If not a registered github user, you can still use git with https. Type (as a non-root user):


```
        git clone https://github.com/smcameron/space-nerds-in-space.git
```


    Finally, you can just download a snapshot zipfile without using git at all:
        https://github.com/smcameron/space-nerds-in-space/archive/master.zip

    After downloading the zip file, you must unpack the zip file. Type (as a non-root user):

```
        unzip space-nerds-in-space-master.zip
        cd space-nerds-in-space-master
```


Step 3: Build the Code
----------------------

To build the code, make sure you are in the top level directory for the game ("space-nerds-in-space" if you got the source using git, or "space-nerds-in-space-master" if you downloaded the zip file), and type (as a non-root user):

```
    make
```

You should see quite a lot of output, like this:

```
      COMPILE mathutils.c
      COMPILE snis_alloc.c
      COMPILE snis_socket_io.c

    ... many steps omitted here ...

      LINK bin/snis_server
      LINK bin/snis_client
      LINK bin/snis_limited_client
      LINK bin/snis_multiverse
```

If you have problems building the code, it likely means there is some missing dependency. Double check that you have all the required dependencies installed.

You can also file a bug report if you think you have discovered a problem with the build process, or the instructions here. I believe you will need a github account to file a bug report.

    [Click here to file a bug report](https://github.com/smcameron/space-nerds-in-space/issues).

Step 4: build openscad models (optional)
----------------------------------------

Or you can skip to step 5 and download them (recommended). This step will take a long time and requires that you installed OpenSCAD. In general, unless you are working on the models, you can skip this step. (Again, as a non-root user):

```
      make models
```

Step 5: Download additional assets
----------------------------------

If you skipped step 4 and didn't build the openscad models, they will be downloaded in this step, along with some other things. This step requires an internet connection. If you performed step 4, you may skip this step though it is not recommended, as you will be missing some additional solarsystem assets. As a non-root user:

```
      make update-assets
```

Step 6. Run the Game on a single system
---------------------------------------

(Try this before trying multiplayer)

Type (as a non-root user):

```
        $ ./snis_launcher

             Welcome to Space Nerds In Space

        ------------------------------------------------------------
        No SNIS processes are currently running.
        ------------------------------------------------------------

           1. Launch SNIS lobby server
              The lobby server allows clients to find servers
              There should be one lobby server total.
           2. Launch SNIS multiverse server
              The multiverse server stores player ship data
              There should be one multiverse server total
           3. Launch SNIS server
              There should be one snis server per solarsystem.
              There should be at least one instance of snis_server.
           4. Launch SNIS client process
              There should be one snis client process per player
              plus one more per ship for the main screen.
           5. Launch limited SNIS client process (no Open GL required).
           6. Stop all SNIS processes
           7. Stop all SNIS clients
           8. Stop all SNIS servers
           9. Check for asset updates
           10. Enter Lobby Host IP addr (currently localhost)
           0. Quit
           Choose [0-10]: _
```

    Choose option 1, then option 2, then option 3, then option 4 (taking defaults for any questions you might be asked.)

