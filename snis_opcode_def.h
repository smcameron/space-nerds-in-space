#ifndef SNIS_OPCODE_DEF_H__
#define SNIS_OPCODE_DEF_H__
/*
	Copyright (C) 2017 Stephen M. Cameron
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

#include <stdint.h>

/* Must call this once, and first.  Initializes opcode definitions */
int snis_opcode_def_init(void);

/* Returns payload size of an opcode (that has no subcode) */
int snis_opcode_payload_size(uint8_t opcode);
/* Returns payload size of an opcode, subcode pair */
int snis_opcode_subcode_payload_size(uint8_t opcode, uint8_t subcode);
/* Returns format of opcode that has no subcode */
const char *snis_opcode_format(uint8_t opcode);
/* Returns format of an opcode, subcode pair */
const char *snis_opcode_subcode_format(uint8_t opcode, uint8_t subcode);

/* snis_opcode_pkt() is like packed_buffer_new() from snis_marshal.h,
 * except it checks correctness of opcode format for the opcode.
 */
struct packed_buffer *snis_opcode_pkt(const char *format, ...);
struct packed_buffer *snis_opcode_subcode_pkt(const char *format, ...);

/* Functions to allow iterating over all opcodes */
uint8_t snis_first_opcode(void);
uint8_t snis_next_opcode(uint8_t opcode);
uint8_t snis_last_opcode(void);

#define SNIS_OPCODE(x) snis_opcode_format((x)), x

#endif
