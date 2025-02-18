
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


# Build Instructions

Here's a quick preview of the build instructions detailed below:

0. Acquire necessary hardware (some linux machines, network switch, cables, projector, stereo, etc.)
1. Clone git repository or obtain source tarball and unpack it.
2. Install dependencies (run `util/install_dependencies` on apt-based systems)
3. Build the code (type "`make`")
4. Download additional art assets (type "`bin/snis_launcher`", then choose
option 1, "Check for asset updates and set up assets".)
5. Run the game (type "`bin/snis_launcher`")


[Here is a long, boring video demonstrating how to install](https://www.youtube.com/watch?v=tCokfUtZOqw)

## Step 0: Acquire Hardware and OS

This is a linux game.  You will need a computer running linux that has
a reasonably decent GPU.  A Raspberry Pi 4 (with 4Gb RAM) and heatsinks is
sufficiently powerful to run the game at 720p for <em>some</em> screens, namely NAVIGATION,
ENGINEERING, DAMAGE CONTROL, SCIENCE and COMMS, but is <em>not</em> really good enough
for the MAIN VIEW, WEAPONS, or DEMON screens (you'll see very low FPS).
[Here is a long boring youtube video demonstrating installation and running SNIS on a Raspberry Pi 4B](https://www.youtube.com/watch?v=wdy4ICZqc68)
if you want to see for yourself what it's like.
Older Raspberry Pis
are generally **not** powerful enough to run the game.

It is possible to run the game entirely on a single linux laptop, but for the full and
proper spaceship experience you will want six linux computers, a network switch,
a projector or large TV, and a stereo system for audio in a reasonably dark
room.

The game is currently developed using [Mint 20.3](https://www.linuxmint.com/)
It is within the realm of possibility that the game could be made to build
and work on Mac OSX (it has been done before, but not since 2014 or so.)
You're on your own if you want to try to run it on Mac, and my understanding
is that newer Macs don't support OpenGL, which means you're out of luck.

## Step 1: Download the Source Code

The source code is here:
[Space Nerds In Space github page](https://github.com/smcameron/space-nerds-in-space)

**NOTE: Do NOT perform these steps as root!**

To get the source code, there are three methods:

- If you are a registered github user, type (as a non-root user):

```
> git clone git@github.com:smcameron/space-nerds-in-space.git
```

- If not a registered github user, you can still use git with https.  Type (as a non-root user):

```
> git clone https://github.com/smcameron/space-nerds-in-space.git
```

- Finally, you can just download a snapshot zipfile without using git at all:
  - [https://github.com/smcameron/space-nerds-in-space/archive/master.zip](https://github.com/smcameron/space-nerds-in-space/archive/master.zip)

After downloading the zip file, you must unpack the zip file.  Type (as a non-root user):

```
> unzip space-nerds-in-space-master.zip
> cd space-nerds-in-space-master
```

## Step 2: Install Dependencies

There is a script, `util/install_dependencies`. In theory it can install dependencies
on apt, yum, zypper, or rpm based systems, but it has only been tried with apt based
systems. If you want to see what it will do without actually doing anything, it has
a `--dry-run` option.  Even on apt-based systems, package names are not consistent
across various linux distros so it might not do the right thing.  This is a Hard
Problem which there is no avoiding.

```
`util/install_dependencies`
```

If you don't want to run this script, or find that it does not work for you, you can
install the dependencies manually, as described below. If `util/install_dependencies`
performs its job satisfactorily, you can advance to Step 3.

```
> sudo apt-get install build-essential;
> sudo apt-get install portaudio19-dev;
> sudo apt-get install libpng-dev;
> sudo apt-get install libvorbis-dev;
> sudo apt-get install libsdl2-dev;
> sudo apt-get install libsdl2-2.0-0;
> sudo apt-get install liblua5.2-dev;
> sudo apt-get install libglew-dev;
> sudo apt-get install libttspico-utils; # optional, for text to speech
> sudo apt-get install sox; # for "play" command, used by text to speech
> sudo apt-get install libcrypt-dev; # used by bin/snis_update_assets
> sudo apt-get install libcurl-dev; # used by bin/snis_update_assets
> 
> You might also need these in addition or instead of some of the above:
>
> sudo apt-get install lib4curl-openssl-dev
> sudo apt-get install libssl-dev
>
> # Opus is needed for voice-chat, though you can compile without voice chat or,
> # you can have the Makefile download and compile opus for you instead of
> # using packages if you distro does not have Opus packages
> #
> sudo apt-get install libopus-dev; # used for voice-chat feature
> sudo apt-get install libopus0; # used for voice-chat feature
> 
> # (The following are optional)
> sudo apt-get install espeak; # optional alternative to libttspico-utils
> sudo apt-get install alsa-utils; # optional alternative to sox, for "aplay" command
> sudo apt-get install openscad; # optional, only needed if you intend to work on 3D models.
>                 # Also, it is recommended to get it from http://www.openscad.org/downloads.html
>                 # as the version in the repos tends to be out of date.
> sudo apt-get install git; # Version control, useful if you're hacking on the game
> sudo apt-get install stgit; # Useful for making patches if you're hacking on the game.
> It's like [quilt](https://en.wikipedia.org/wiki/Quilt_%28software%29) but on top of git.

```

If you want to try the optional pocketsphinx based local speech recognition,
you will want the following packages:

```
> sudo apt-get install pocketsphinx-utils;
> sudo apt-get install pocketsphinx-lm-en-hub4;
> sudo apt-get install pocketsphinx-lm-en-hub4;
> sudo apt-get install libpocketsphinx1;
```

The above list may be incomplete, and these are the package names on mint 17.3 / ubuntu,
so may be different on RPM based systems.

> NOTE: Later, the Makefile will download the Opus library source (
> https://archive.mozilla.org/pub/opus/opus-1.3.1.tar.gz).  This library is used
> for the voice chat feature. If you intend to build on a machine which does not
> have internet access, or for whatever reason do not want the makefile to download
> this library at build time, you can download it yourself beforehand. Or you can
> modify the Makefile and change "WITHVOICECHAT=yes" at the top of the file to
> "WITHVOICECHAT=no" and it will not download this library (and you won't have the
> voice chat feature.)

> NOTE: problems building on SuSE Leap 15.1. The following problems/solutions have
> been conveyed to me regarding SuSE Leap 15.1. This information is not very complete or
> accurate, but it is the best I have for now.  Please feel free to send me
> improvements to these instructions.
> See [Bug 222](https://github.com/smcameron/space-nerds-in-space/issues/222)


1. Need to install pkg-config and pkg-config_files.
2. Need to set pkg-config-path environment variable
3. All references to lua 5.2 in the Makefile need to be changed to lua 5.3.
   Note, we only compiled snis_client (via "make bin/snis_client") which shouldn't
   need lua at all. I don't know if the differences between lua 5.3 and lua 5.2 are
   significant enough to break things. I normally use lua 5.2 on my systems.
   [Changes to Lua 5.3](http://www.lua.org/manual/5.3/readme.html#changes)
   -- at a quick glance I didn't see anything that I think will be problematic.

## Step 3: Build the Code

To build the code, make sure you are in the top level
directory for the game ("space-nerds-in-space" if you
got the source using git, or "space-nerds-in-space-master" if you
downloaded the zip file), and type (as a non-root user):

```
> make
```

You should see quite a lot of output, like this:

```
>  COMPILE mathutils.c
>  COMPILE snis_alloc.c
>  COMPILE snis_socket_io.c
>
> ... many steps omitted here ...
> 
>  LINK bin/snis_server
>  LINK bin/snis_client
>  LINK bin/snis_limited_client
>  LINK bin/snis_multiverse
```

If you have problems building the code, it likely means there is some
missing dependency.  Double check that you have all the required dependencies
installed.  If the missing dependencies have to do with opus libraries, and your
distro does not have opus packages, you can run "make DOWNLOAD_OPUS=yes", and the
makefile will download the opus library source from mozilla.org and compile it
for you.

You can also file a bug report if you think you have discovered a problem
with the build process, or the instructions here.  I believe you will need a github
account to file a bug report.

- [Click here to file a bug report](https://github.com/smcameron/space-nerds-in-space/issues)


## Step 4: build openscad models (optional, not recommended)

Or you can skip to step 5 and just download the models instead (recommended).
This step will take a long time and requires that you installed OpenSCAD.
In general, unless you are working on the models, you can and should skip this step.
(Again, as a non-root user):

```
> make models
```

Note: If you intend to run the game without downloading aditional assets
(step 5, below), with just the default assets, this can be done, but
requires the additional step of running:

```
>  bin/snis_update_assets --force --destdir ~/.local/share/space-nerds-in-space --srcdir ./share/snis
```

to copy assets into ~/.local/share/space-nerds-in-space.

## Step 5: Download additional assets

If you skipped step 4 and didn't build the openscad models, they will be downloaded
in this step, along with some other things. This step requires an internet connection.
If you performed step 4, you may skip this step though it is not recommended,
as you will be missing some additional solarsystem assets. As a non-root user:

```
>  bin/snis_launcher
```

Then choose option 1, "Check for asset updates and set up assets".  This will copy assets
to (by default) into $HOME/.local/share/space-nerds-in-space/ and download additional assets
over the internet.

