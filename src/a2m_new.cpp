/*
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2007 Simon Peter, <dn.tlp@gmx.net>, et al.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * a2m.cpp - A2M Loader by Simon Peter <dn.tlp@gmx.net>
 *
 * NOTES:
 * This loader detects and loads version 1, 4, 5 & 8 files.
 *
 * version 1-4 files:
 * Following commands are ignored: FF1 - FF9, FAx - FEx
 *
 * version 5-8 files:
 * Instrument panning is ignored. Flags byte is ignored.
 * Following commands are ignored: Gxy, Hxy, Kxy - &xy
 */

#include <cstring>
#include "a2m.h"
#include "a2m_loader.h"

CPlayer *Ca2mLoader::factory(Copl *newopl) {
  return new Ca2mLoader(newopl);
}

char * file_load(const char *name) {
  FILE *fh;
  int fsize;
  void *p;

  fh = fopen(name, "rb");
  if (!fh) return NULL;

  fseek(fh, 0, SEEK_END);
  fsize = ftell(fh) ;
  fseek(fh, 0, SEEK_SET);
  p = (void *)malloc(fsize);
  fread(p, 1, fsize, fh);
  fclose(fh);

  return (char *)p;
}

bool Ca2mLoader::load(const std::string &filename, const CFileProvider &fp) {

  char * bytes = file_load(filename.c_str());
  if (bytes == NULL) {
    printf("Error rexading %s\n", filename.c_str());
    return false;
  }

  // AT2Song * song = (AT2Song *)malloc(sizeof(AT2Song));

  songStruct * song = a2m_import(bytes);
  // bool song = a2m_import(bytes);

  // a2t_import(song);
  if (!song) {
    printf("Error loading %s\n", filename.c_str());
    return false;
  }

  restartpos = 0;
  length     = 128;
  version    = song->version;
  nop        = song->num_patterns;
  tempo      = song->tempo; 
  initspeed  = song->speed;
  flags      = songdata->common_flag;
  order      = songdata->pattern_order;

  // tFIXED_SONGDATA data = song->songdata;
  memcpy(songname, songdata->songname, 43);
  memcpy(author, songdata->composer, 43);

  printf("%s\n" ,songdata->songname);
  printf("version: %d\n", song->version);
  printf("song: %s\n", songname);
  printf("author: %s\n", author);

  int i, instcount = version > 8 ? 255 : 250;
  for (i = 0; i < instcount; i++) {
    // inst[i].data = song_data->instr_data[i];

    printf("inst %d: %s\n", i, songdata->instr_names[i]);
    printf("first: %d\n", songdata->instr_data[i][0]);
    printf("fm   : %d\n", songdata->instr_data[i][10]);

    inst[i].data[0]  = songdata->instr_data[i][10];
    inst[i].data[1]  = songdata->instr_data[i][0];
    inst[i].data[2]  = songdata->instr_data[i][1];
    inst[i].data[3]  = songdata->instr_data[i][4];
    inst[i].data[4]  = songdata->instr_data[i][5];
    inst[i].data[5]  = songdata->instr_data[i][6];
    inst[i].data[6]  = songdata->instr_data[i][7];
    inst[i].data[7]  = songdata->instr_data[i][8];
    inst[i].data[8]  = songdata->instr_data[i][9];
    inst[i].data[9]  = songdata->instr_data[i][2];
    inst[i].data[10] = songdata->instr_data[i][3];

/*
    if (version < 5) {
      inst[i].misc = *(orgptr + i * instsize + 11);
    } else {  // version >= 5 -> OPL3 format
      int pan = *(orgptr + i * instsize + 11);

      if (pan)
        inst[i].data[0] |= (pan & 3) << 4;  // set pan
      else
        inst[i].data[0] |= 48;      // enable both speakers
    }

    inst[i].slide = *(orgptr + i * instsize + 12);

    if (version > 8) {
      int type = *(orgptr + i * instsize + 13);
      // printf("type: %d\n", type);
    }
*/

  }

  if (version < 5) {
    for (int i = 0; i < 4; i++) {
      if (!len[i+1]) continue;

      for (int p = 0; p < 16; p++) // pattern
      for (int r = 0; r < 64; r++) // row
      for (int c = 0; c < 9; c++) { // channel
        struct Tracks *track = &tracks[p * 9 + c][r];
        tADTRACK2_EVENT * ev = &pattdata[i * 16 + p].ch[c].row[r].ev;

        track->note = ev->note;
        track->inst = ev->instr_def;
        track->command = ev->effect_def;
        // track->param1 = ev->effect;
        // track->param2 = ev->effect;
      }
    }

  } else if (version < 9) {
    realloc_patterns(64, 64, 18);

    for (int i = 0; i < 8; i++) {
      if (!len[i+1]) continue;

      for (int p = 0; p < 8; p++) // pattern
      for (int c = 0; c < 18; c++) // channel
      for (int r = 0; r < 64; r++) { // row
        struct Tracks *track = &tracks[p * 18 + c][r];
        tADTRACK2_EVENT * ev = &pattdata[i * 8 + p].ch[c].row[r].ev;

        track->note = ev->note;
        track->inst = ev->instr_def;
        track->command = ev->effect_def;
      }
    }

  } else { // 9, 10, 11 [16][8][20][256][6]
    realloc_patterns(256, 256, 20);

    for (int i = 0; i < 16; i++) { 
      if (!len[i+1]) continue;

      for (int p = 0; p < 8; p++) // pattern
      for (int c = 0; c < 20; c++) // channel
      for (int r = 0; r < 256; r++) { // row
        struct Tracks *track = &tracks[p * 20 + c][r];
        tADTRACK2_EVENT * ev = &pattdata[i * 8 + p].ch[c].row[r].ev;

        track->note = ev->note;
        track->inst = ev->instr_def;
        track->command = ev->effect_def;
      }
    }

  }

  init_trackord();

  // Process flags
  if (version >= 5) {
    CmodPlayer::flags |= Opl3;        // All versions >= 5 are OPL3

    if (flags & 8) CmodPlayer::flags |= Tremolo;  // Tremolo depth
    if (flags & 16) CmodPlayer::flags |= Vibrato; // Vibrato depth
  }

  free(song);
  free(songdata);
  free(pattdata);

  // fp.close(f);
  rewind(0);
  return true;
}

float Ca2mLoader::getrefresh() {
  if (tempo != 18)
    return (float) (tempo);
  else
    return 18.2f;
}
