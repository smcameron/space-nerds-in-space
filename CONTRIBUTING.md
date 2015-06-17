How to Contribute
=================

TODO: figure out how to help people know how to contribute

Jump in and start hacking... But!  Try to keep your commits sane, and make them change only one logical thing at a time (this does not mean only change one file, it is more like, make it add only one feature).  Each commit should build and work.  I have found [stacked git](http://www.procode.org/stgit/) very helpful for making sane commits, but other people have managed to get more or less the same functionality using git stashes, I think.  But, if you make a mess of it, but the code is still working and
good, you can still try to send it to me, and I might be able to fix up the patches.

Getting to know the code
------------------------

TODO: put a better explanation here.

There are two main pieces where most of the work is happening.  The snis_server is the place where the simulation of the universe, and all significant actions by game entities occurs.  The snis_client is what the players interact with directly, and where requests are made of the server (e.g. "please crank up the warp drive to warp 10.") and where responses from the server are accepted and interpreted and rendered ("warp drive cranked up to 7 because engineering deprived you a some amount of power").

On the server side, start at main() in snis_server.  The server contains a thread which communicates with the lobby (to let the lobby know it is still alive, etc.), a thread which listens for new connections from game clients, and two threads per connected game client, a reader thread, which accepts requests from the game clients, and a writer which writes updates to the game clients.  Important landmarks in snis_server.c:

Functions:

1. move_objects()
2. listener_thread()
3. per_client_read_thread()
4. per_client_write_thread()
5. process_instructions_from_client()
6. queue_up_client_updates
7. write_queued_updates_to_client()

Important variables:

1. client[] -- one element per game client
2. bridgelist[] -- one element per bridge (crew) in the game.
3. go[] -- one element per "game object" (struct snis_entity) 
4. bridgelist[].damcon -- sub-game for the damage control screen containing
   all the objects in the damage control sub-game.

On the client side, start at main in snis_client.c.  There is a lot of gtk stuff in there
which you can probably ignore.  There are a bunch of functions in there called init_blah_blah_ui
which set up the (custom made vector drawn not-really-gtk) widgets for the various screens in
the game.  Near the end, gtk_main() is called, after a timer is set up to call advance_game()
30 times per second, and after mouse events and button presses are hooked into various
callbacks.  Important landmarks in snis_client.c:

Functions:

1. advance_game()
2. main_da_expose() <-- this is what draws everything
3. ui_element_list_draw()
4. ...

Important variables:

1. uiobjs -- all the user interface objects for all the screens
2. go[] all the game objects 
3. dco[] all the damage control objects
4. ... 


Adding new features
-------------------

If you want to add a new feature to the ship, it is esp. important to make sure you do all the meaningful actions server side, and only make requests of the server from the client side, and do rendering client side.

Custom hardware device controls
-------------------------------

Maybe you want to build a physical starship bridge with toggle switches and dials
and sliders and buttons and flight yokes and other various gizmos and doodads to
control the game?   Have a look at snis-device-io.h and snis-device-io.c and
device-io-sample-1.c.  All you have to do is write a little program to monitor
and gather inputs from your hardware devices however you like, then feed them
to snis_client via the couple of functions in snis-device-io.o.

Coding Style
------------

My coding style is very much like [the Linux Kernel Coding Style](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/CodingStyle) except probably not quite as careful about it in some respects (esp. longer line lengths are more OK here than there, but still to be avoided.)

You can check your patch by running it through checkpatch.pl, for example:

	git diff | checkpatch.pl -

or
	stg show | checkpatch.pl -

The gist of it is:

1.  Indent with tabs not spaces.
2.  Tabs are 8 characters wide, and this is about as malleable as the value of PI.
3.  if either the "then" or "else" arm of the if statement requires braces, then both
    arms should use braces.  If neither arm requires braces, then no braces. 
4.  Variable names should_be_like_this, NotLikeThis, use lowercase with underscores
    not camel case.
5.  Macros all uppercase, try not to be too clever with macros.
6.  Avoid typedefs except for function pointers.
7.  No C++, not even C++ style comments.
8.  Functions should be static by default unless deliberately meant to be visible
    outside the module.
9.  For modules, header files should ideally declare only types and function prototypes
    and use the GLOBAL macro to make it so the implementation of the module can include
    the same header as the users of the module.  See, e.g: wwviaudio.h
10. If you submit a patch that violates these, I will probably fix it for you the
    first time or two.
11. Patches are probably best sent via email as a bzip2ed tarball of a [stacked git](http://www.procode.org/stgit/) patch series, but other methods (pull request) might be ok too (but harder to review).
12. You need to certify the following Developer's Certificate of Origin
    regarding your patch by adding a "Signed-off-by:" line in the patch description.

        Developer's Certificate of Origin 1.1

        By making a contribution to this project, I certify that:

        (a) The contribution was created in whole or in part by me and I
            have the right to submit it under the open source license
            indicated in the file; or

        (b) The contribution is based upon previous work that, to the best
            of my knowledge, is covered under an appropriate open source
            license and I have the right under that license to submit that
            work with modifications, whether created in whole or in part
            by me, under the same open source license (unless I am
            permitted to submit under a different license), as indicated
            in the file; or

        (c) The contribution was provided directly to me by some other
            person who certified (a), (b) or (c) and I have not modified
            it.

        (d) I understand and agree that this project and the contribution
	    are public and that a record of the contribution (including all
	    personal information I submit with it, including my sign-off) is
	    maintained indefinitely and may be redistributed consistent with
	    this project or the open source license(s) involved.

then you just add a line saying

	Signed-off-by: Random J Developer <random@developer.example.org>

