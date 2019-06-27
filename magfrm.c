/* Copyright (C) 2017 Lars Brinkhoff <lars@nocrew.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <string.h>

#include "dis.h"

extern word_t get_its_word (FILE *f);
extern void write_core_word (FILE *f, word_t);
extern word_t ascii_to_sixbit (char *);

static int write_reclen (FILE *f, int reclen)
{
  /* A SIMH tape image record length is 32-bit little endian. */
  fputc ( reclen        & 0xFF, f);
  fputc ((reclen >>  8) & 0xFF, f);
  fputc ((reclen >> 16) & 0xFF, f);
  fputc ((reclen >> 24) & 0xFF, f);
}

static int write_record (FILE *f, int reclen, void *buffer)
{
  word_t *x = buffer;
  int i;

  /* To write a tape record in the SIMH tape image format, first write
     a 32-bit record length, then data frames, then the length again.
     For PDP-10 36-bit data, the data words are written in the "core
     dump" format.  One word is written as five 8-bit frames, with
     four bits unused in the last frame. */

  fprintf (stderr, "Record, %d words\n", reclen);
  write_reclen (f, 5 * reclen);

  for (i = 0; i < reclen; i++)
    write_core_word (f, *x++);

  /* Pad out to make the record data an even number of octets. */
  if ((reclen * 5) & 1)
    fputc (0, f);

  /* A record of length zero is a tape mark, and the length is only
     written once. */
  if (reclen > 0)
    write_reclen (f, 5 * reclen);
}

word_t buffer[5 * 1024];

static void
write_header (FILE *f, char *name)
{
  char *fn1, *fn2;

  /* The MAGDMP header is three words of SIXBIT text:
     file name 1, file name 2, and version.  Use the input
     file name split on "." and version 001 for all files. */

  fprintf (stderr, "File %s -> ", name);

  fn1 = name;
  fn2 = strchr (name, '.');

  if (fn2)
    *fn2++ = 0;
  else
    {
      fn2 = fn1;
      fn1 = "@";
    }

  fprintf (stderr, "%s %s\n", fn1, fn2);

  buffer[0] = ascii_to_sixbit (fn1);
  buffer[1] = ascii_to_sixbit (fn2);
  buffer[2] = ascii_to_sixbit ("001");
  write_record (f, 3, buffer);
  write_record (f, 0, buffer);
}

static void
write_file (FILE *f, char *name)
{
  FILE *in;
  int n;
  word_t *x = buffer;

  in = fopen (name, "rb");
  if (in == NULL)
    {
      fprintf (stderr, "File not found: %s\n", name);
      exit (1);
    }
  n = 0;

  /* One record with the file name, and then a tape mark. */
  write_header (f, name);

  /* Then file data in 1024 word records, terminated by a tape mark. */
  for (;;)
    {
      word_t word = get_its_word (in);

      if (word != -1)
	{
	  *x++ = word;
	  n++;
	}

      if (n == 1024 || word == -1)
	{
          /* If we have a full record, or no more data in the input
             file, write a record. */
	  if (n > 0)
	    write_record (f, n, buffer);
	  x = buffer;
	  n = 0;
	}

      if (word == -1)
	break;
    }

  /* Tape mark. */
  write_record (f, 0, buffer);
}

int
main (int argc, char **argv)
{
  word_t word;
  FILE *f;
  int i;
  int eof = 0;

  if (argc < 2)
    {
      fprintf (stderr, "Usage: %s <magdmp> <files...>\n\n", argv[0]);
      fprintf (stderr, "Writes a MAGDMP bootable tape image file in SIMH format.\n");
      fprintf (stderr, "The first argument specifies the MAGDMP program.\n");
      fprintf (stderr, "The following arguments specify files put on the tape.\n");
      fprintf (stderr, "The inputs must be in ITS evacuate format.\n");
      fprintf (stderr, "The output is written to stdout.\n");
      exit (1);
    }

  memset (buffer, 0, sizeof buffer);

  /* The first tape record should be MAGDMP, ended by a tape mark.
     Just a placeholder for now. */
  write_record (stdout, 2, buffer);
  write_record (stdout, 0, buffer);

  for (i = 1; i < argc; i++)
    write_file (stdout, argv[i]);

  return 0;
}
