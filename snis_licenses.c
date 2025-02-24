#include <stdio.h>

#include "snis_licenses.h"

char *snis_credits_text[] = {
	"S P A C E   N E R D S   I N   S P A C E",
	"",
	"*   *   *",
	"",
	"HTTPS://SMCAMERON.GITHUB.IO/SPACE-NERDS-IN-SPACE/",
	"HTTPS://WWW.PATREON.COM/user?u=9573826",
	"",
	"CREATED BY",
	"STEPHEN M. CAMERON",
	"AND",
	"JEREMY VAN GRINSVEN",
	"",
	"INSPIRED BY",
	"THOM ROBERTSON AND",
	"ARTEMIS: STARSHIP BRIDGE SIMULATOR",
	"",
	"SPECIAL THANKS TO",
	"",
	"TX/RX LABS IN HOUSTON, TX",
	"WITHOUT WHICH THIS GAME",
	"WOULD NOT EXIST",
	"",
	"HACKRVA IN RICHMOND, VA",
	"",
	"QUBODUP & FREEGAMEDEV.NET",
	"",
	"KWADROKE & BRIDGESIM.NET",
	"",
	"*   *   *",
	"",
	"OTHER CONTRIBUTORS",
	"",
	"ANDY CONRAD (HER001)",
	"ANTHONY J. BENTLEY",
	"BYRON ROOSA",
	"CHRISTIAN ROBERTS",
	"CHRISTOPHER WELLONS",
	"JIMMY (DUSTEDDK)",
	"EMMANOUEL KAPERNAROS",
	"HARRISON TOTTY",
	"IOAN LOOSLEY",
	"IVAN SANCHEZ ORTEGA",
	"JUSTIN WARWICK",
	"KYLE ROBBERTZE",
	"LUCKI",
	"MCMic",
	"MICHAEL ALDRIDGE",
	"MICHAEL T DEGUZIS",
	"REMI VERSCHELDE",
	"SCOTT BENESH",
	"STEFAN GUSTAVSON",
	"THOMAS GLAMSCH",
	"TOBIAS SIMON",
	"VINCE PELSS",
	"VIKTOR HAHN",
	"ZACHARY SCHULTZ",
	"",
	"SPECIAL THANKS TO THE FOLLOWING PATRONS",
	"OF SPACE NERDS IN SPACE",
	"",
	"ALEC SLOMAN",
	"ANDREW ROACH",
	"BILL LEMMOND",
	"BILLY",
	"CALEB COHOON",
	"ELI ROSS",
	"MARTIN HAUKELI",
	"DAN HUNSAKER",
	"EMMANOUIL KAPERNAROS",
	"JUSTIN ROKISKY",
	"MCMic",
	"Q'S LAB, A RIDGECREST MAKERSPACE",
	"RAYMOND D MERIDETH",
	"RINZLER",
	"STEPHEN WARD",
	"TALAS",
	"",
	"*   *   *",
	"",
	NULL,
};

static char snis_license[] =
	"        Copyright (C) 2010 Stephen M. Cameron\n"
	"        Author: Stephen M. Cameron\n"
	"\n"
	"        This file is part of Spacenerds In Space.\n"
	"\n"
	"        Spacenerds in Space is free software; you can redistribute it and/or modify\n"
	"        it under the terms of the GNU General Public License as published by\n"
	"        the Free Software Foundation; either version 2 of the License, or\n"
	"        (at your option) any later version.\n"
	"\n"
	"        Spacenerds in Space is distributed in the hope that it will be useful,\n"
	"        but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	"        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	"        GNU General Public License for more details.\n"
	"\n"
	"        You should have received a copy of the GNU General Public License\n"
	"        along with Spacenerds in Space; if not, write to the Free Software\n"
	"        Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA\n";

static char ssgl_license[] =
	"Copyright (c) 2010 Stephen M. Cameron\n"
	"\n"
	"Permission is hereby granted, free of charge, to any person\n"
	"obtaining a copy of this software and associated documentation\n"
	"files (the \"Software\"), to deal in the Software without\n"
	"restriction, including without limitation the rights to use,\n"
	"copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
	"copies of the Software, and to permit persons to whom the\n"
	"Software is furnished to do so, subject to the following\n"
	"conditions:\n"
	"\n"
	"The above copyright notice and this permission notice shall be\n"
	"included in all copies or substantial portions of the Software.\n"
	"\n"
	"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,\n"
	"EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES\n"
	"OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND\n"
	"NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT\n"
	"HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,\n"
	"WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING\n"
	"FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR\n"
	"OTHER DEALINGS IN THE SOFTWARE.\n";

static char quat_library_license[] =
	"   quaternion library - implementation\n"
	"\n"
	"   Copyright (C) 2013 Tobias Simon\n"
	"   Portions Copyright (C) 2014 Jeremy Van Grinsven\n"
	"   Portions Copyright (C) 2018 Stephen M. Cameron\n"
	"\n"
	"   This program is free software; you can redistribute it and/or modify\n"
	"   it under the terms of the GNU General Public License as published by\n"
	"   the Free Software Foundation; either version 2 of the License, or\n"
	"   (at your option) any later version.\n"
	"\n"
	"   This program is distributed in the hope that it will be useful,\n"
	"   but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	"   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	"   GNU General Public License for more details.\n"
	"\n";

static char frustrum_functions_license[] =
	"Two functions in entity.c, sphere_in_frustrum() and extract_frustrum_planes()\n"
	"were taken from http://www.crownandcutlass.com/features/technicaldetails/frustum.html\n"
	"which stated:\n"
	"\n"
	"   This page and its contents are Copyright 2000 by Mark Morley\n"
	"   Unless otherwise noted, you may use any and all code examples\n"
	"   provided herein in any way you want.\n"
	"\n";

static char ogg_to_pcm_license[] =
	"/* OggDec\n"
	" *\n"
	" * This program is distributed under the GNU General Public License, version 2.\n"
	" * A copy of this license is included with this source.\n"
	" *\n"
	" * Copyright 2002, Michael Smith <msmith@xiph.org>\n"
	" *\n"
	" */\n"
	"\n"
	"/*\n"
	" *\n"
	" * This code was hacked off of the carcass of oggdec.c, from\n"
	" * the vorbistools-1.2.0 package, and is copyrighted as above,\n"
	" * with the modifications made by me,\n"
	" * (c) Copyright Stephen M. Cameron, 2008,\n"
	" * (and of course also released under the GNU General Public License, version 2.)\n"
	" *\n"
	" */\n";

static char mikktspace_license[] =
	"/**\n"
	" *  Copyright (C) 2011 by Morten S. Mikkelsen\n"
	" *\n"
	" *  This software is provided 'as-is', without any express or implied\n"
	" *  warranty.  In no event will the authors be held liable for any damages\n"
	" *  arising from the use of this software.\n"
	" *\n"
	" *  Permission is granted to anyone to use this software for any purpose,\n"
	" *  including commercial applications, and to alter it and redistribute it\n"
	" *  freely, subject to the following restrictions:\n"
	" *\n"
	" *  1. The origin of this software must not be misrepresented; you must not\n"
	" *     claim that you wrote the original software. If you use this software\n"
	" *     in a product, an acknowledgment in the product documentation would be\n"
	" *     appreciated but is not required.\n"
	" *  2. Altered source versions must be plainly marked as such, and must not be\n"
	" *     misrepresented as being the original software.\n"
	" *  3. This notice may not be removed or altered from any source distribution.\n"
	" */\n";

struct license {
	char *item;
	char *license_text;
};

static struct license license[] = {
	{ "Space Nerds in Space", snis_license, },
	{ "SSGL (Super Simple Game Lobby)", ssgl_license, },
	{ "Quaternion library", quat_library_license, },
	{ "ogg_to_pcm library", ogg_to_pcm_license, },
	{ "frustum functions in entity.c", frustrum_functions_license, },
	{ "mikktspace code for computing tangent and bitangent\n"
		"vectors for purposes of normal mapping", mikktspace_license, },
	{ NULL, NULL },
};

void print_snis_credits_text(void)
{
	int i;

	for (i = 0; snis_credits_text[i] != NULL; i++)
		printf("%s\n", snis_credits_text[i]);
}

void print_snis_licenses(void)
{
	int i;

	for (i = 0; license[i].item != NULL; i++) {
		printf("-------------------------------------------------\n");
		printf("Information about the license for %s follows:\n\n", license[i].item);
		printf("%s\n", license[i].license_text);
		printf("-------------------------------------------------\n\n");
	}
}

