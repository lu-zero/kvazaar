#ifndef BITSTREAM_H_
#define BITSTREAM_H_
/*****************************************************************************
 * This file is part of Kvazaar HEVC encoder.
 * 
 * Copyright (C) 2013-2014 Tampere University of Technology and others (see 
 * COPYING file).
 *
 * Kvazaar is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Kvazaar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kvazaar.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

/*
 * \file
 * \brief Bitstream can be written to one or several bits at a time.
 */
 
#include "global.h"

 
typedef struct
{
    uint32_t data[32];
    uint8_t  cur_byte;
    uint8_t  cur_bit;
    FILE*    output;
    uint8_t* buffer;
    uint32_t buffer_pos;
    uint32_t bufferlen;
} bitstream;

typedef struct
{
  uint8_t len;
  uint32_t value;
} bit_table;

extern bit_table *g_exp_table;

int floor_log2(unsigned int n);

bitstream *create_bitstream(int32_t width);
void bitstream_alloc(bitstream* stream, uint32_t alloc);
void bitstream_free(bitstream *stream);
void bitstream_clear_buffer(bitstream* stream);
void bitstream_reinit(bitstream *stream); 
void bitstream_put(bitstream* stream, uint32_t data, uint8_t bits); 

/* Use macros to force inlining */
#define bitstream_put_ue(stream, data) { bitstream_put(stream,g_exp_table[data].value,g_exp_table[data].len); }
#define bitstream_put_se(stream, data) { uint32_t index=(uint32_t)(((data)<=0)?(-(data))<<1:((data)<<1)-1);    \
                                         bitstream_put(stream,g_exp_table[index].value,g_exp_table[index].len); }

void bitstream_align(bitstream* stream); 
void bitstream_align_zero(bitstream* stream);
void bitstream_flush(bitstream* stream);
int init_exp_golomb(uint32_t len);


/* In debug mode print out some extra info */
#ifdef NOTDEFINED//_DEBUG
/* Counter to keep up with bits written */
static int WRITE_VALUE = 0;
#define WRITE_U(stream, data, bits, name) { printf("%8d  %-40s u(%d) : %d\n",WRITE_VALUE, name,bits,data); bitstream_put(stream,data,bits); WRITE_VALUE++;}
#define WRITE_UE(stream, data, name) { printf("%8d  %-40s ue(v): %d\n",WRITE_VALUE, name,data); bitstream_put_ue(stream,data); WRITE_VALUE++;}
#define WRITE_SE(stream, data, name) { printf("%8d  %-40s se(v): %d\n",WRITE_VALUE, name,data); bitstream_put_se(stream,(data)); WRITE_VALUE++;}
#else
#define WRITE_U(stream, data, bits, name) { bitstream_put(stream,data,bits); }
#define WRITE_UE(stream, data, name) { bitstream_put_ue(stream,data); }
#define WRITE_SE(stream, data, name) { bitstream_put_se(stream,data); }
#endif

 
#endif
