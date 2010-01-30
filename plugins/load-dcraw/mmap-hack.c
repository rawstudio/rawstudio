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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include "mmap-hack.h"
#include <unistd.h>

RS_FILE *
rs_fopen(const char *path, const char *mode)
{
	RS_FILE *file;
	int fd;
	struct stat st;

	if(stat(path, &st))
		return(NULL);

	if ((fd = open(path, O_RDONLY)) == -1)
		return(NULL);

	file = (RS_FILE *) malloc(sizeof(RS_FILE));
	file->fd = fd;
	file->size = st.st_size;
	file->map = (unsigned char *) mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, file->fd, 0);
	file->offset = 0;
	return file;
}

int
rs_fclose(RS_FILE *RS_FILE)
{
	munmap(RS_FILE->map, RS_FILE->size);
	close(RS_FILE->fd);
	free(RS_FILE);
	return 0;
}

inline int
rs_fseek(RS_FILE *stream, long offset, int whence)
{
	switch(whence)
	{
		case SEEK_SET:
			stream->offset = offset;
			break;
		case SEEK_CUR:
			stream->offset += offset;
			break;
		case SEEK_END:
			stream->offset = stream->size + offset;
	}

	/* clamp */
	stream->offset = (stream->offset > stream->size) ? stream->size : ((stream->offset < 0) ? 0 : stream->offset);
	return(0);
}

inline size_t
rs_fread(void *ptr, size_t size, size_t nmemb, RS_FILE *stream) 
{
	if (stream->offset + size*nmemb <= stream->size)
	{
		memcpy(ptr, &stream->map[stream->offset], size*nmemb);
		stream->offset+=size*nmemb;
		return nmemb;
	}
	int bytes = stream->size - stream->offset;
	memcpy(ptr, &stream->map[stream->offset], bytes);
	stream->offset+=bytes;
	return(bytes / size);
}

char *
rs_fgets(char *s, int size, RS_FILE *stream)
{
	int destoff = 0;
	while (destoff < size)
	{
		if (stream->offset >= stream->size) return 0;
		s[destoff] = stream->map[stream->offset++];
		if (s[destoff] == 0 || s[destoff] == '\n')
			return s;			
		destoff++;		
	}
	return(NULL);
}

int
rs_fscanf(RS_FILE *stream, const char *format, void* dst)
{
	int scanned = scanf(format, &stream->map[stream->offset], dst);
	stream->offset+= scanned;
	return(scanned);
}
