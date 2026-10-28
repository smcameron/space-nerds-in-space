#ifndef SNIS_ASSET_FETCH_H__
#define SNIS_ASSET_FETCH_H__
/*
	This file (snis_asset_fetch.h) is entirely machine-generated. To the extent
	possible under law, the author(s) have dedicated all copyright and related and
	neighboring rights to this software to the public domain worldwide.  This
	software is distributed without any warranty.

	You should have received a copy of the CC0 Public Domain Dedication along with
	this software. If not, see <http://creativecommons.org>.
*/

/*
 * Make sure a solarsystem's assets are in the asset directory before they
 * are read.  Returns 0 if they are (or may be), -1 if they could not be got.
 *
 * snis_asset_fetch.c does nothing: natively, every solarsystem's assets are
 * already on disk.  snis_asset_fetch_web.c fetches them from the web server
 * the first time a solarsystem is needed, so that the browser build need not
 * download every solarsystem before it starts.  Call it from a thread other
 * than the main one, since it blocks until the files have arrived.
 */

#ifdef DEFINE_SNIS_ASSET_FETCH_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL int snis_fetch_solarsystem_assets(const char *asset_dir, const char *solarsystem);

#undef GLOBAL
#endif
