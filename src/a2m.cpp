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

int Ca2mLoader::a2_read_patterns(int ver, char *src, int s) {

  const unsigned char convfx[16]   = { 0, 1, 2, 23, 24, 3, 5, 4, 6, 9, 17, 13, 11, 19, 7, 14 };
  const unsigned char convinf1[16] = { 0, 1, 2, 6, 7, 8, 9, 4, 5, 3, 10, 11, 12, 13, 14, 15 };
  const unsigned char newconvfx[]  = { 0, 1, 2, 3, 4, 5, 6, 23, 24, 21, 10, 11, 17, 13, 7, 19,
                                     255, 255, 22, 25, 255, 15, 255, 255, 255, 255, 255,
                                     255, 255, 255, 255, 255, 255, 255, 255, 14, 255 };

  int track_index;

  switch (ver) {
  case 1 ... 4: // [4][16][64][9][4]
    {
    tPATTERN_DATA_V1234 *old = (tPATTERN_DATA_V1234 *)malloc(sizeof(*old) * 16);

    for (int i = 0; i < 4; i++) {
      if (!at2_block_lengths[i+s]) continue;

      a2t_depack(src, at2_block_lengths[i+s], old);

      for (int p = 0; p < 16; p++) // pattern
      for (int r = 0; r < 64; r++) // row
      for (int c = 0; c < 9; c++) { // channel

        track_index = ((i * 16) + p) * 64 + c;
        // memcpy(&pattdata[i * 9 + p].ch[c].row[r].ev, &old[p].row[r].ch[c].ev, 4);
        // memcpy(&pattdata[i * 16 + p].ch[c].row[r].ev, &old[p].row[r].ch[c].ev, 4);
        tADTRACK2_EVENT_V1234 * ev = &old[p].row[r].ch[c].ev;

        struct Tracks *track = &tracks[track_index][r];
        track->note = ev->note == 255 ? 127 : ev->note;
        track->inst = ev->instr_def;
        track->command = convfx[ev->effect_def];
        // track->param1 = ev->effect >> 4;
        track->param2 = ev->effect & 0x0f;

        if (track->command != 14) {
          track->param1 = ev->effect >> 4;
        } else {
          track->param1 = convinf1[ev->effect >> 4];

          if (track->param1 == 15 && !track->param2) { // convert key-off
            track->command = 8;
            track->param1 = 0;
            track->param2 = 0;
          }
        }

        if (track->command == 14) {
          switch (track->param1) {
          case 2: // convert define waveform
            track->command = 25;
            track->param1 = track->param2;
            track->param2 = 0xf;
            break;

          case 8: // convert volume slide up
            track->command = 26;
            track->param1 = track->param2;
            track->param2 = 0;
            break;

          case 9: // convert volume slide down
            track->command = 26;
            track->param1 = 0;
            break;
          }
        }
      }

      src += at2_block_lengths[i+s];
    }

    free(old);
    break;
    }
  case 5 ... 8: // [8][8][18][64][4]
    {

    tPATTERN_DATA_V5678 *old = (tPATTERN_DATA_V5678 *)malloc(sizeof(*old) * 8);
    realloc_patterns(64, 64, 18); // pats, rows, chans

    for (int i = 0; i < 8; i++) {
      if (!at2_block_lengths[i+s]) continue;

      a2t_depack(src, at2_block_lengths[i+s], old);

      for (int p = 0; p < 8; p++) // pattern
      for (int c = 0; c < 18; c++) // channel
      for (int r = 0; r < 64; r++) { // row

        track_index = ((i * 8) + p) * 18 + c;
        // memcpy(&pattdata[i * 18 + p].ch[c].row[r].ev, &old[p].ch[c].row[r].ev, 4);
        // tADTRACK2_EVENT * ev = &pattdata[i * 18 + p].ch[c].row[r].ev;

        tADTRACK2_EVENT_V1234 * ev = &old[p].ch[c].row[r].ev;
        if (ev->note > 0) printf("[%d/%d] p: %d, c: %d, r: %d, n: %d, inst: %d\n", i, track_index, p, c, r, ev->note, ev->instr_def);

        struct Tracks *track = &tracks[track_index][r];
        track->note    = ev->note == 255 ? 127 : ev->note;
        track->inst    = ev->instr_def;
        track->command = newconvfx[ev->effect_def];
        track->param1  = ev->effect >> 4;
        track->param2  = ev->effect & 0x0f;

        // Convert '&' command
        if (ev->effect_def == 36) {
          switch (track->param1) {
            case 0: // pattern delay (frames)
              track->command = 29;
              track->param1 = 0;
              // param2 already set correctly
              break;

            case 1: // pattern delay (rows)
              track->command = 14;
              track->param1 = 8;
              // param2 already set correctly
              break;
          }
        }
      }

      src += at2_block_lengths[i+s];
    }

    free(old);
    break;
    }
  case 9 ... 11:  // [16][8][20][256][6]
    {

    tPATTERN_DATA *old = (tPATTERN_DATA *)malloc(sizeof(*old) * 8);
    realloc_patterns(64, 64, 20); // pats, rows, chans

    for (int i = 0; i < 16; i++) {
      if (!at2_block_lengths[i+1]) continue;

      printf( " -- processing block: %d: %d\n", i, at2_block_lengths[i+s]);
      a2t_depack(src, at2_block_lengths[i+s], old);

      for (int p = 0; p < 8; p++) // pattern
      for (int c = 0; c < 20; c++) // channel
      for (int r = 0; r < 256; r++) { // row

        track_index = ((i * 16) + p) * 20 + c;

        // memcpy(&pattdata[i * 16 + p].ch[c].row[r].ev, &old[p].ch[c].row[r].ev, 6);
        // tADTRACK2_EVENT * ev = &pattdata[i * 16 + p].ch[c].row[r].ev;
        tADTRACK2_EVENT * ev = &old[p].ch[c].row[r].ev;

        if (ev->note > 0) printf("[%d/%d] p: %d, c: %d, r: %d, n: %d, inst: %d\n", i, track_index, p, c, r, ev->note, ev->instr_def);

        struct Tracks *track = &tracks[track_index][r];
        track->note    = ev->note == 255 ? 127 : ev->note;
        track->inst    = ev->instr_def;
        track->command = newconvfx[ev->effect_def];
        track->param1  = ev->effect >> 4;
        track->param2  = ev->effect & 0x0f;

        // Convert '&' command
        if (ev->effect_def == 36) {
          switch (track->param1) {
            case 0: // pattern delay (frames)
              track->command = 29;
              track->param1 = 0;
              // param2 already set correctly
              break;

            case 1: // pattern delay (rows)
              track->command = 14;
              track->param1 = 8;
              // param2 already set correctly
              break;
          }
        }
      }

      src += at2_block_lengths[i+s];
    }

    free(old);
    break;
    }
  }

  return 0;
}

bool Ca2mLoader::load(const std::string &filename, const CFileProvider &fp) {

  char * bytes = file_load(filename.c_str());
  if (bytes == NULL) {
    printf("Error reading %s\n", filename.c_str());
    return false;
  }

  // AT2Song * song = (AT2Song *)malloc(sizeof(AT2Song));
  songStruct * song = a2m_import(bytes);
  free(bytes);

  if (!song) {
    printf("Error loading %s\n", filename.c_str());
    return false;
  }

  restartpos = 0;
  length     = 128;
  version    = song->version;
  nop        = song->num_patterns;
  bpm        = song->tempo; 
  initspeed  = song->speed;
  flags      = songdata->common_flag;
  memcpy(order, &songdata->pattern_order, 128);

  for (int i = 0; i < 128; i++) {
    if (order[i] != 128) printf(" + %d -> %d\n", i, order[i]);
  }

  int num_tracks  = song->num_tracks;
  int patt_length = song->patt_length;

  memcpy(songname, songdata->songname, 43);
  memcpy(author, songdata->composer, 43);

  printf("speed: %d\n", initspeed);
  printf("tempo: %d\n", bpm);

  printf("num pats: %d\n", nop);
  printf("num tracks: %d\n", num_tracks);

  printf("version: %d\n", version);
  printf("song: %s\n", songname);
  printf("author: %s\n", author);

  int i, instcount = version > 8 ? 255 : 250;
  for (i = 0; i < instcount; i++) {
    // inst[i].data = song_data->instr_data[i];

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

    if (inst[i].data[1]) {
      printf("inst %d: %s\n", i, songdata->instr_names[i]);
    }

    if (version < 5) {
      inst[i].misc = songdata->instr_data[i][11];
    } else {  // version >= 5 -> OPL3 format
      int pan = songdata->instr_data[i][11];

      if (pan)
        inst[i].data[0] |= (pan & 3) << 4;  // set pan
      else
        inst[i].data[0] |= 48;      // enable both speakers
    }

    inst[i].slide = songdata->instr_data[i][12];

    if (version > 8) {
      int type = songdata->instr_data[i][13];
      // printf("type: %d\n", type);
    }
  }

  a2_read_patterns(version, song->blockptr, 1);

/*
  if (version < 5) { // [4][16][64][9][4]
    for (int i = 0; i < 4; i++) {
      if (!at2_block_lengths[i+1]) continue;

      for (int p = 0; p < 16; p++) // pattern
      for (int r = 0; r < 64; r++) // row
      for (int c = 0; c < 9; c++) { // channel
        struct Tracks *track = &tracks[p * 9 + c][r];
        tADTRACK2_EVENT * ev = &pattdata[i * 16 + p].ch[c].row[r].ev;

        track->note = ev->note;
        track->inst = ev->instr_def;
        track->command = ev->effect_def;
        track->param1 = ev->effect >> 4;
        track->param2 = ev->effect & 0x0f;
      }
    }

  } else if (version < 9) { // [8][8][18][64][4]
    realloc_patterns(16, 64, 18); // pats, rows, chans

    for (int i = 0; i < 8; i++) {
      if (!at2_block_lengths[i+1]) continue;

      for (int p = 0; p < 8; p++) // pattern
      for (int c = 0; c < 18; c++) // channel
      for (int r = 0; r < 64; r++) { // row
        struct Tracks *track = &tracks[p * 18 + c][r];
        tADTRACK2_EVENT * ev = &pattdata[i * 16 + p].ch[c].row[r].ev;
        // if (ev->note > 0) printf("[%d] p: %d, c: %d, r: %d, n: %d, inst: %d\n", i, p, c, r, ev->note, ev->instr_def);

        track->note = ev->note;
        track->inst = ev->instr_def;
        track->command = ev->effect_def;
        track->param1 = ev->effect >> 4;
        track->param2 = ev->effect & 0x0f;
      }
    }

  } else { // [16][8][20][256][6]

    // pattdata is [128][20][256]

    // realloc_patterns(256, 256, 20);
    realloc_patterns(128, 256, 20); // pats, rows, chans

    for (int i = 0; i < 16; i++) { 
      if (!at2_block_lengths[i+1]) continue;
      printf( " -- processing block: %d: %d\n", i, at2_block_lengths[i+1]);

      for (int p = 0; p < 8; p++) // pattern
      for (int c = 0; c < 20; c++) // channel
      for (int r = 0; r < 256; r++) { // row
      // for (int r = 0; r < patt_length; r++) { // row
        
        tADTRACK2_EVENT * ev = &pattdata[i * 16 + p].ch[c].row[r].ev;
        // if (ev->note > 0) printf("[%d] p: %d, c: %d, r: %d, n: %d, inst: %d\n", i, p, c, r, ev->note, ev->instr_def);

        struct Tracks *track = &tracks[p * 16 + c][r];
        track->note    = ev->note;
        track->inst    = ev->instr_def;
        track->command = ev->effect_def;  // newconvfx[ev->effect_def];
        track->param1  = ev->effect >> 4;
        track->param2  = ev->effect & 0x0f;
      }
    }
  }
  */

  // printf("note at track 1, row 0: %d, %d\n", tracks[1][0].note, tracks[1][0].inst);
  // printf("note at track 80, row 0: %d, %d\n", tracks[80][0].note, tracks[80][0].inst);

  init_trackord();

  // Process flags
  if (version >= 5) {
    CmodPlayer::flags |= Opl3;        // All versions >= 5 are OPL3

    if (flags & 8) CmodPlayer::flags |= Tremolo;  // Tremolo depth
    if (flags & 16) CmodPlayer::flags |= Vibrato; // Vibrato depth
  }

  // free(song);
  // pattdata = NULL;
  // songdata = NULL;

  rewind(0);
  return true;
}

float Ca2mLoader::getrefresh() {
  if (tempo != 18)
    return (float) (tempo);
  else
    return 18.2f;
}
