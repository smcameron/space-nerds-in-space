#ifndef STL_FILE_H__
#define STL_FILE_H__
/*
        Copyright (C) 2010 Stephen M. Cameron
        Author: Stephen M. Cameron

        This file is part of Spacenerds In Space.

        Spacenerds in Space is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
        (at your option) any later version.

        Spacenerds in Space is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with Spacenerds in Space; if not, write to the Free Software
        Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifdef DEFINE_STL_FILE_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL struct mesh *read_mesh(char *file);
GLOBAL struct mesh *read_stl_file(char *file);
GLOBAL struct mesh *read_obj_file(char *file);
GLOBAL void free_mesh(struct mesh *m);

typedef char * (*asset_replacement_function)(char *asset);

GLOBAL void stl_parser_set_asset_replacement_function(asset_replacement_function fn);


#undef GLOBAL
#endif
