/*
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999, 2000, 2001 Simon Peter, <dn.tlp@gmx.net>, et al.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * mid.h - LAA & MID & CMF Player by Philip Hassey (philhassey@hotmail.com)
 */

#include "player.h"

class CmidPlayer: public CPlayer
{
public:
	CmidPlayer(Copl *newopl)
		: CPlayer(newopl), author(&emptystr), title(&emptystr), remarks(&emptystr), emptystr('\0'), insfile(0), flen(0), data(0)
	{ };
	~CmidPlayer()
	{ if(data) delete [] data; if(insfile) delete [] insfile; };

	bool load(istream &f);
	bool update();
	void rewind(unsigned int subsong);
	float getrefresh();

	std::string gettype();
	std::string gettitle()
	{ return std::string(title); };
	std::string getauthor()
	{ return std::string(author); };
	std::string getdesc()
	{ return std::string(remarks); };
	unsigned int getinstruments()
	{ return tins; };
	unsigned int getsubsongs()
	{ return subsongs; };

	void set_sierra_insfile(char *ifile)
	{ insfile = new char [strlen(ifile)+1]; strcpy(insfile,ifile); };

protected:
	struct midi_channel {
		int inum;
		unsigned char ins[11];
		int vol;
		int nshift;
		int on;
	};

	struct midi_track {
		unsigned long tend;
		unsigned long spos;
		unsigned long pos;
		unsigned long iwait;
		int on;
		unsigned char pv;
	};

	char *author,*title,*remarks,emptystr,*insfile;
    long flen;
    unsigned long pos;
    unsigned long sierra_pos; //sierras gotta be special.. :>
    unsigned int subsongs;
    unsigned char *data;

    unsigned char adlib_data[256];
    int adlib_style;
    int adlib_mode;
    unsigned char myinsbank[128][16];
    midi_channel ch[16];
    int chp[9][3];

    long deltas;
    long msqtr;

    midi_track track[16];
    unsigned int curtrack;

    float fwait;
    unsigned long iwait;
    int doing;

    int type,tins;

private:
	bool load_sierra_ins();
	void midiprintf(char *format, ...);
	unsigned char datalook(long pos);
	unsigned long getnexti(unsigned long num);
	unsigned long getnext(unsigned long num);
	unsigned long getval();
	void sierra_next_section();
	unsigned long filelength(istream &f);
	void midi_write_adlib(unsigned int r, unsigned char v);
	void midi_fm_instrument(int voice, unsigned char *inst);
	void midi_fm_volume(int voice, int volume);
	void midi_fm_playnote(int voice, int note, int volume);
	void midi_fm_endnote(int voice);
	void midi_fm_reset();
};