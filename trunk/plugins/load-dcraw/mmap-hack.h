/*
 * * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>, 
 * * Anders Kvist <akv@lnxbx.dk> and Klaus Post <klauspost@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef MMAP_HACK_H
#define MMAP_HACK_H

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct RS_FILE {
	int fd;
	unsigned char *map;
	unsigned int offset;
	unsigned int size;
} RS_FILE;

#define RS_FILE(stream) ((RS_FILE *)(stream))

extern RS_FILE *rs_fopen(const char *path, const char *mode);
extern int rs_fclose(RS_FILE *fp);
extern int rs_fseek(RS_FILE *stream, long offset, int whence);
extern long rs_ftell(RS_FILE *stream);
extern void rs_rewind(RS_FILE *stream);
extern int rs_fscanf(RS_FILE *stream, const char *format, void* dst);
extern int rs_fgetc(RS_FILE *stream);
extern size_t rs_fread(void *ptr, size_t size, size_t nmemb, RS_FILE *stream);
extern char *rs_fgets(char *s, int size, RS_FILE *stream);

#ifdef  __cplusplus
}
#endif

#define fopen(a,b) (FILE *) rs_fopen(a,b)
#define fclose(a) rs_fclose(RS_FILE(a))
#define fseek(a, b, c) rs_fseek(RS_FILE(a),b,c)
#define fread(a,b,c,d) rs_fread(a,b,c,RS_FILE(d))
#define fgets(a,b,c) rs_fgets(a,b,RS_FILE(c))
#define fscanf(a,b,c) rs_fscanf(RS_FILE(a),b,c)

#define fgetc(stream) (int) (RS_FILE(stream)->map[RS_FILE(stream)->offset++])
#define ftell(stream) (long) (RS_FILE(stream)->offset)
#define rewind(stream) do {RS_FILE(stream)->offset = 0; } while(0)
#define feof(stream) (RS_FILE(stream)->offset >= RS_FILE(stream)->size)

#ifdef getc
#undef getc
#endif
#define getc(stream) fgetc(stream)

#endif /* MMAP_HACK_H */
