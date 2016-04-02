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
#include "depack.h"
#include "sixpack.h"

static int ffver;

static inline int depack(void *src, void *dst, int srcsize) {
  switch (ffver) {
  case 1:
  case 5:   // sixpack
    return sixdepak((short unsigned int *)src, (unsigned char*)dst, srcsize);
    break;
  case 2:
  case 6:   // FIXME: lzw
    break;
  case 3:
  case 7:   // FIXME: lzss
    break;
  case 4:
  case 8:   // unpacked
    memcpy(dst, src, srcsize);
    return srcsize;
    break;
  case 9 ... 11:  // apack (aPlib)
    int res = aP_depack(src, dst);
    printf("depacked: %d\n", res);
    // return srcsize;
    return res;
    break;
  }
}

CPlayer *Ca2mLoader::factory(Copl *newopl) {
  return new Ca2mLoader(newopl);
}

bool Ca2mLoader::load(const std::string &filename, const CFileProvider &fp) {
  binistream *f = fp.open(filename); if (!f) return false;
  char id[10];
  int i, j, k, t;
  unsigned int l;
  unsigned char *org, *orgptr, flags = 0, numpats, version;
  unsigned long crc, alength;
  unsigned short len[17], *packeddata, *packedptr;
  const unsigned char convfx[16] = {0, 1, 2, 23, 24, 3, 5, 4, 6, 9, 17, 13, 11, 19, 7, 14};
  const unsigned char convinf1[16] = {0, 1, 2, 6, 7, 8, 9, 4, 5, 3, 10, 11, 12, 13, 14, 15};
  const unsigned char newconvfx[] = {0, 1, 2, 3, 4, 5, 6, 23, 24, 21, 10, 11, 17, 13, 7, 19,
                                     255, 255, 22, 25, 255, 15, 255, 255, 255, 255, 255,
                                     255, 255, 255, 255, 255, 255, 255, 255, 14, 255
                                    };

  // read header
  f->readString(id, 10); crc = f->readInt(4);
  version = f->readInt(1); numpats = f->readInt(1);

  // file validation section
  if (strncmp(id, "_A2module_", 10) || (version != 1 && version != 5 && version != 4 && version != 8 && version != 9 && version != 11)) {
    fp.close(f);

    printf("Invalid A2M version: %d\n", version);
    return false;
  }

  ffver = version;
  printf(" -> Loading A2M file format: %d\n", version);

  // load, depack & convert section
  nop = numpats; length = 128; restartpos = 0;
  int maxpats;

  if (version < 5) {
    maxpats = 4;
    for (i = 0; i < (maxpats+1); i++) len[i] = f->readInt(2);
    t = 9;
  } else if (version < 9) {  
    maxpats = 8;
    for (i = 0; i < (maxpats+1); i++) len[i] = f->readInt(2);
    t = 18;
  } else {
    maxpats = 16;
    for (i = 0; i < (maxpats+1); i++) { 
      len[i] = f->readInt(4);
      // printf("len: %d\n", len[i]);
    }
    t = 68;
  }

  // block 0
  packeddata = new unsigned short [len[0] / 2];

  if (version == 1 || version == 5) {
    for (i = 0; i < len[0] / 2; i++) packeddata[i] = f->readInt(2);

    org = new unsigned char [MAXBUF]; 
    orgptr = org;
    sixdepak(packeddata, org, len[0]);

  } else if (version > 8) {

    for (i = 0; i < len[0] / 2; i++) packeddata[i] = f->readInt(2);

    org = new unsigned char [100000]; 
    orgptr = org;
    depack(packeddata, org, len[0]);

  } else {
    orgptr = (unsigned char *)packeddata;

    for (i = 0; i < len[0]; i++) orgptr[i] = f->readInt(1);
  }

  memcpy(songname, orgptr, 43); orgptr += 43;
  memcpy(author, orgptr, 43); orgptr += 43;

  int instsize   = (version < 9 ? 13 : 14);
  int instcount  = (version < 9 ? 250 : 255);
  int namelength = (version < 10) ? 33 : 43;
  // int dstsize   = instcount * instsize;
  // char *dst = (char *)malloc(dstsize);
  // memset(dst, 0, dstsize);
  // depack(src, len[0], dst);

  printf("songname: %s\n", songname);
  printf("author: %s\n", author);

  char instnames[instcount][namelength];

  memcpy(instnames, orgptr, instcount * namelength); 
  orgptr += (instcount * namelength);

  for (i = 0; i < instcount; i++) { // instruments
    printf("instrument %i: %s\n", i, instnames[i]);

    inst[i].data[0] = *(orgptr + i * instsize + 10);
    inst[i].data[1] = *(orgptr + i * instsize);
    inst[i].data[2] = *(orgptr + i * instsize + 1);
    inst[i].data[3] = *(orgptr + i * instsize + 4);
    inst[i].data[4] = *(orgptr + i * instsize + 5);
    inst[i].data[5] = *(orgptr + i * instsize + 6);
    inst[i].data[6] = *(orgptr + i * instsize + 7);
    inst[i].data[7] = *(orgptr + i * instsize + 8);
    inst[i].data[8] = *(orgptr + i * instsize + 9);
    inst[i].data[9] = *(orgptr + i * instsize + 2);
    inst[i].data[10] = *(orgptr + i * instsize + 3);

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
  }

  orgptr += instcount * instsize;

  if (version > 8) {
    // skip instrument macro defs
    orgptr += 255 * 3831;

    // skip arpeggio/vibrato macro defs
    orgptr += 255 * 521;
  }

  memcpy(order, orgptr, 128); orgptr += 128;
  bpm = *orgptr; orgptr++;
  initspeed = *orgptr; orgptr++;
  if (version >= 5) flags = *orgptr;

  printf("tempo: %d\n", bpm);
  printf("init speed: %d\n", initspeed);

  if (version > 8) {
    int first, last;
    orgptr++; first = *orgptr;
    orgptr++; last  = *orgptr;
    unsigned short pattern_len = (first << 8) | last;
    printf("pattern length: %d\n", pattern_len);

    orgptr++;
    int ntracks = *orgptr;
    printf("number of tracks: %d\n", ntracks);

    orgptr++; first = *orgptr;
    orgptr++; last  = *orgptr;
    unsigned short macro_speedup = (first << 8) | last;
    printf("macro_speedup: %d\n", macro_speedup);

    if (version > 9) {
      orgptr++;
      int track_extension = *orgptr;
      char lock_flags[20];
      memcpy(lock_flags, orgptr, 20);
      orgptr += 20;
    }

    if (version > 10) {
      char patt_names[128][43];
      memcpy(patt_names, orgptr, 128 * 43);
      orgptr += 128 * 43;

      char disabled_fm_regs[255][28];
      memcpy(disabled_fm_regs, orgptr, 255 * 28);
      orgptr += 255 * 28;
    }
  }

  if (version == 1 || version == 5 || version > 8) delete [] org;
  delete [] packeddata;

  // blocks 1-4 or 1-8
  alength = len[1];
  for (i = 0; i < (maxpats - 2); i++) {
    if (len[i + 2] == 0) break; // stop if zero

    alength += len[i + 2];
    printf("added: %d, now: %ld\n", len[i + 2], alength);
  }

  packeddata = new unsigned short [alength / 2];

  if (version == 1 || version == 5 || version > 8) {
    for (l = 0; l < alength / 2; l++) 
      packeddata[l] = f->readInt(2);

    org = new unsigned char [MAXBUF * (numpats / (version == 1 ? 16 : 8) + 1)];
    // org = new unsigned char [MAXBUF * (numpats/8) + 1];
    orgptr = org; packedptr = packeddata;

    if (version < 9) {
      orgptr += sixdepak(packedptr, orgptr, len[1]); packedptr += len[1] / 2;
    } else {
      orgptr += depack(packedptr, orgptr, len[1]); packedptr += len[1] / 2;
    }

    if (version == 1) {
      if (numpats > 16)
        orgptr += sixdepak(packedptr, orgptr, len[2]); packedptr += len[2] / 2;

      if (numpats > 32)
        orgptr += sixdepak(packedptr, orgptr, len[3]); packedptr += len[3] / 2;

      if (numpats > 48)
        sixdepak(packedptr, orgptr, len[4]);

    } else if (version == 5) {
      if (numpats > 8)
        orgptr += sixdepak(packedptr, orgptr, len[2]); packedptr += len[2] / 2;

      if (numpats > 16)
        orgptr += sixdepak(packedptr, orgptr, len[3]); packedptr += len[3] / 2;

      if (numpats > 24)
        orgptr += sixdepak(packedptr, orgptr, len[4]); packedptr += len[4] / 2;

      if (numpats > 32)
        orgptr += sixdepak(packedptr, orgptr, len[5]); packedptr += len[5] / 2;

      if (numpats > 40)
        orgptr += sixdepak(packedptr, orgptr, len[6]); packedptr += len[6] / 2;

      if (numpats > 48)
        orgptr += sixdepak(packedptr, orgptr, len[7]); packedptr += len[7] / 2;

      if (numpats > 56)
        sixdepak(packedptr, orgptr, len[8]);

    } else if (version > 8) {
      if (numpats > 8)
        orgptr += depack(packedptr, orgptr, len[2]); packedptr += len[2] / 2;

      if (numpats > 16)
        orgptr += depack(packedptr, orgptr, len[3]); packedptr += len[3] / 2;

      if (numpats > 24)
        orgptr += depack(packedptr, orgptr, len[4]); packedptr += len[4] / 2;

      if (numpats > 32)
        orgptr += depack(packedptr, orgptr, len[5]); packedptr += len[5] / 2;

      if (numpats > 40)
        orgptr += depack(packedptr, orgptr, len[6]); packedptr += len[6] / 2;

      if (numpats > 48)
        orgptr += depack(packedptr, orgptr, len[7]); packedptr += len[7] / 2;

      if (numpats > 56)
        depack(packedptr, orgptr, len[8]);
    }

    delete [] packeddata;

  } else {
    org = (unsigned char *)packeddata;

    for (l = 0; l < alength; l++) org[l] = f->readInt(1);
  }

  if (version < 5) {
    for (i = 0; i < numpats; i++) {
      for (j = 0; j < 64; j++) {
        for (k = 0; k < 9; k++) {
          struct Tracks *track = &tracks[i * 9 + k][j];
          unsigned char *o = &org[i * 64 * t * 4 + j * t * 4 + k * 4];

          track->note = o[0] == 255 ? 127 : o[0];
          track->inst = o[1];
          track->command = convfx[o[2]];
          track->param2 = o[3] & 0x0f;

          if (track->command != 14)
            track->param1 = o[3] >> 4;
          else {
            track->param1 = convinf1[o[3] >> 4];

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
      }
    }

  } else if (version < 9) { // 5-8
    // realloc_patterns(64, 64, 18);
    realloc_patterns(16, 64, 18); // pats, rows, chans

    for (i = 0; i < numpats; i++) {
      for (j = 0; j < 18; j++) {
        for (k = 0; k < 64; k++) {
          struct Tracks *track = &tracks[i * 18 + j][k];
          unsigned char *o = &org[i * 64 * t * 4 + j * 64 * 4 + k * 4];

          track->note = o[0] == 255 ? 127 : o[0];
          track->inst = o[1];
          track->command = newconvfx[o[2]];
          track->param1 = o[3] >> 4;
          track->param2 = o[3] & 0x0f;

          // Convert '&' command
          if (o[2] == 36) {
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
      }
    }
  } else { // 9,10,11 -- [16][8][20][256][6]
    realloc_patterns(16, 256, 20); // pats, rows, chans

    for (i = 0; i < numpats; i++) {
      for (j = 0; j < 20; j++) { // channel
        for (k = 0; k < 256; k++) { // row
          struct Tracks *track = &tracks[i * 20 + j][k];
          unsigned char *o = &org[i * 256 * t * 6 + j * 256 * 6 + k * 6];

          track->note = o[0] == 255 ? 127 : o[0];
          track->inst = o[1];
          track->command = newconvfx[o[2]];
          track->param1 = o[3] >> 4;
          track->param2 = o[3] & 0x0f;

          // if (track->note > 0) printf("patt: %d, note: %d\n", i, track->note);
          // track->command2 = newconvfx[o[4]]
          // track->param3 = o[5] >> 4;
          // track->param4 = o[5] & 0x0f;

          // Convert '&' command
          if (o[2] == 36) {
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
      }
    }
  }

  init_trackord();

  if (version == 1 || version == 5 || version > 8)
    delete [] org;
  else
    delete [] packeddata;

  // Process flags
  if (version >= 5) {
    CmodPlayer::flags |= Opl3;        // All versions >= 5 are OPL3

    if (flags & 8) CmodPlayer::flags |= Tremolo;  // Tremolo depth

    if (flags & 16) CmodPlayer::flags |= Vibrato; // Vibrato depth
  }

  fp.close(f);
  rewind(0);
  return true;
}

float Ca2mLoader::getrefresh() {
  if (tempo != 18)
    return (float) (tempo);
  else
    return 18.2f;
}
