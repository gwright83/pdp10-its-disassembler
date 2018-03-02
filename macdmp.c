/* Copyright (C) 2018 Lars Brinkhoff <lars@nocrew.org>

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

#define BLOCK_WORDS 128

/* UFD, URNDM */
#define UNLINK 0000001000000LL
#define UNREAP 0000002000000LL
#define UNWRIT 0000004000000LL
#define UNMARK 0000010000000LL
#define DELBTS 0000020000000LL
#define UNIGFL 0000024000000LL
#define UNDUMP 0400000000000LL

word_t image[580 * 128];
int blocks;
int extract;

word_t directory[027][2];
char filename[027][14];
int mode[027];

/* Mode 0 = ASCII, written by TECO.
 * Mode 1 = DUMP, written by MACDMP.
 * Mode 2 = SBLK, written by MIDAS.
 * Mode 3 = RELOC, written by MIDAS. */

char *type = " !\"#";

static word_t *
get_block (int block)
{
  return &image[block * 128];
}

extern word_t get_dta_word (FILE *f);

static int read_block (FILE *f, word_t *buffer)
{
  int i;

  for (i = 0; i < BLOCK_WORDS; i++)
    {
      buffer[i] = get_dta_word (f);
      if (buffer[i] == -1)
	return -1;
    }

  return 0;
}

static void
process (void)
{
  word_t *dir = get_block (0100);
  char fn1[7], fn2[7];
  int i;

  for (i = 0; i < 027; i++)
    {
      directory[i][0] = dir[0];
      directory[i][1] = dir[1];
      sixbit_to_ascii (dir[0], fn1);
      sixbit_to_ascii (dir[1], fn2);
      sprintf (filename[i], "%s %s", fn1, fn2);
      dir += 2;
    }

  memset (mode, 0, sizeof mode);

  for (i = 0; i < 027; i++)
    {
      if (*dir & 1)
	mode[i] |= 1;
      dir++;
    }
  for (i = 0; i < 027; i++)
    {
      if (*dir & 1)
	mode[i] |= 2;
      dir++;
    }
}

static void (*write_word) (FILE *, word_t);
static void (*flush_word) (FILE *);

static void
write_aa_word (FILE *f, word_t word)
{
  fputc ((word >> 29) & 0177, f);
  fputc ((word >> 22) & 0177, f);
  fputc ((word >> 15) & 0177, f);
  fputc ((word >>  8) & 0177, f);
  fputc (((word >> 1) & 0177) +
	 ((word << 7) & 0200), f);
}

static void
flush_aa_word (FILE *f)
{
  (void)f;
}

static void
write_block (FILE *f, int n)
{
  int i;
  word_t *x = get_block (n);
  for (i = 0; i < 128; i++)
    write_word (f, *x++);
}

static void
write_file (int n, FILE *f)
{
  word_t *dir = get_block (0100);
  int i, j;

  for (i = 0; i < 128 - 2*027; i++)
    {
      word_t x = dir[i + 2*027];
      for (j = 0; j < 7; j++)
	{
	  if (((x >> ((5 * (6-j)) + 1)) & 037) == n)
	    write_block (f, 7*i + j + 1);
	}
    }
}

static void
extract_file (int i, char *name)
{
  if (!extract)
    return;

  FILE *f = fopen (name, "wb");
  write_file (i, f);
  flush_word (f);
  fclose (f);
}

static void
massage (char *filename)
{
  char *x;

  filename[6] = ' ';
  x = filename + 12;
  while (*x == ' ')
    {
      *x = 0;
      x--;
    }

  x = filename;
  while (*x)
    {
      if (*x == '/')
	*x = '|';
      x++;
    }
}

static void
show_files ()
{
  int i;

  for (i = 0; i < 027; i++)
    {
      if (directory[i][0] == 0)
	{
	  if (directory[i][1] == 0)
	    continue;
	  else
	    {
	      printf ("File %d extension %llo\n", i+1, directory[i][1]);
	      continue;
	    }
	}

      printf ("%2d. %s  %c\n", i+1, filename[i], type[mode[i]]);
      massage (filename[i]);
      extract_file (i+1, filename[i]);
    }

  extract_file (0, "free-blocks");
}

static void
usage (const char *x)
{
  fprintf (stderr, "Usage: %s -x|-t <file>\n", x);
  exit (1);
}

int
main (int argc, char **argv)
{
  word_t *buffer;
  FILE *f;
  int i;

  file_36bit_format = FORMAT_DTA;

  if (argc != 3)
    usage (argv[0]);

  if (argv[1][0] != '-')
    usage (argv[0]);

  switch (argv[1][1])
    {
    case 't':
      extract = 0;
      break;
    case 'x':
      extract = 1;
      break;
    default:
      usage (argv[0]);
      break;
    }

  /* Output format. */
  write_word = write_its_word;
  flush_word = flush_its_word;

  f = fopen (argv[2], "rb");
  if (f == NULL)
    {
      fprintf (stderr, "error\n");
      exit (1);
    }

  buffer = image;
  blocks = 0;
  for (;;)
    {
      int n = read_block (f, buffer);
      if (n == -1)
	break;
      buffer += BLOCK_WORDS;
      blocks++;
    }

  process ();
  show_files ();

  return 0;
}
