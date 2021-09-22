#ifndef SNIS_TWEAK_H__
#define SNIS_TWEAK_H__
/*
	Copyright (C) 2018 Stephen M. Cameron
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

/* The purpose of this is to allow the user to "tweak" various global variables.
 *
 * The basic idea is to have an array of structs which describes the name, address
 * type, and min, max and default values of all the global variables you wish to
 * be tweakable, then given a name and new value, generic code can update the
 * value of the variable.
 */

/* All the information you need about one global variable so that given
 * a name, and new value, it can be changed.
 */
struct tweakable_var_descriptor {
	char *name;		/* name of the variable */
	char *description;	/* A description of the variable's purpose, what it does */
	void *address;		/* pointer to the variable */
	char type;		/* type values are 'f', 'i', or 'b' for float, integer, byte */
	float minf, maxf, defaultf;	/* min, max and default for 'f' float variables */
	int mini, maxi, defaulti;	/* min, max and default for 'i' integer variables */
};

/* Look up the tweakable variable descriptor by name
 * desc: array of tweakable variable descriptors
 * count: number of elements in the desc array
 * name: name of element to lookup
 * return value: the index into desc[] of the variable with the specified name is returned
 *	unless it is not found, in which case -1 is returned.
 */
int find_tweakable_var_descriptor(struct tweakable_var_descriptor *desc, int count, char *name);

/* tweak_variable() updates a tweakable global with a new value, or returns the reason
 * why it could not be done.
 *
 * tweak: array of tweakable_var_descriptor
 * count: number of elements in tweak array
 * cmd: string containing command to perform, should be of the form "SET variable = value"
 *      where variable is the name of the variable to tweak, and value is the new value.
 * msg: string buffer into which an error message may be returned.
 * msgsize: size of msg in bytes that may be used for storing messsages.
 * return value:	 0 : successful.
 *			-2 : unknown variable
 *			-3 : value out of range for variable
 *			-4 : bad syntax
 *			-5 : variable has unknown type
 */
#define TWEAK_UNKNOWN_VARIABLE (-2)
#define TWEAK_OUT_OF_RANGE (-3)
#define TWEAK_BAD_SYNTAX (-4)
#define TWEAK_UNKNOWN_TYPE (-5)
int tweak_variable(struct tweakable_var_descriptor *tweak, int count, char *cmd,
			char *msg, int msgsize);
/* Prints out a list of tweakable variables within the tweak array that match
 * the given regex (or all of them if regex is NULL) using the given printfn()
 * tweak: array of tweakable variable descripters to search
 * regex_pattern: regular expression to match entries in tweak[] (can be NULL).
 * count: number of elements in tweak[].
 * printfn: pointer to a printf() style function to be used for the printing of
 *          output.
 * return value: 0 on success, or TWEAK_UNKNOWN_VARIABLE, TWEAK_OUT_OF_RANGE,
 *	TWEAK_BAD_SYNTAX, or TWEAK_UNKNOWN_TYPE.
 */
void tweakable_vars_list(struct tweakable_var_descriptor *tweak, char *regex_pattern,
				int count, void (*printfn)(const char *, ...));
/* Prints out description of a tweakable variable that includes the name,
 * description, current value, and min, max and default values, using the given
 * printf-style printfn() to do the printing.
 *
 * tweak: array of tweakable variable descriptors to search
 * count: number of elements in tweak[].
 * cmd: a string of the form "DESCRIBE variable", where variable is the name of
 *	the variable to describe.
 * printfn: pointer to a printf() style function to be used for the printing of
 *          output.
 * suppress_unknown_var: if non-zero, no error message will be printed if the
 *	variable is not found within the tweak array.  This allows multiple sets
 *	of tweakable variables, so you can call this function multiple times
 *	with different tweak[] arrays, with all but the last call having
 *	suppress_unknown_var = 1, and the last call having suppress_unknown_var = 0,
 *	so that at most one message about unknown variables will be printed.
 *	(In SNIS, there are two sets of tweakable variables, one within snis_client,
 *	and one within snis_server.)
 * return value: 0 on success, or TWEAK_BAD_SYNTAX, or TWEAK_UNKNOWN_VARIABLE
 */
int tweakable_var_describe(struct tweakable_var_descriptor *tweak, int count, char *cmd,
				void (*printfn)(const char *, ...), int suppress_unknown_var);
#endif
