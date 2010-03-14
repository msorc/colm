/*
 *  Copyright 2007, 2008 Adrian Thurston <thurston@complang.org>
 */

/*  This file is part of Colm.
 *
 *  Colm is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  Colm is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with Colm; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#ifndef _INPUT_H
#define _INPUT_H

#include <assert.h>
#include <stdio.h>
#include <iostream>

#define FSM_BUFSIZE 8192
//#define FSM_BUFSIZE 8

/*
 * pdaRun <- fsmRun <- stream 
 *
 * Activities we need to support:
 *
 * 1. Stuff data into an input stream each time we <<
 * 2. Detach an input stream, and attach another when we include
 * 3. Send data back to an input stream when the parser backtracks
 * 4. Temporarily stop parsing due to a lack of input.
 *
 * At any given time, the fsmRun struct may have a prefix of the stream's
 * input. If getting data we first get what we can out of the fsmRun, then
 * consult the stream. If sending data back, we first shift pointers in the
 * fsmRun, then ship to the stream. If changing streams the old stream needs to
 * take back unprocessed data from the fsmRun.
 */

struct KlangEl;
struct Pattern;
struct PatternItem;
struct Replacement;
struct ReplItem;
struct RunBuf;
struct FsmRun;
struct Tree;

struct RunBuf
{
	enum Type {
		DataType,
		TokenType,
		IgnoreType
	};

	RunBuf()
	:
		type(DataType),
		length(0),
		tree(0),
		offset(0),
		next(0),
		prev(0)
	{}

	Type type;
	char data[FSM_BUFSIZE];
	long length;
	Tree *tree;
	long offset;
	RunBuf *next, *prev;
};

//typedef RunBuf AccumData;
struct AccumData
{
	enum Type {
		DataType,
		TokenType
	};

	AccumData()
		: type(DataType) {}

	Type type;
	char *data;
	long length;
	Tree *tree;

	AccumData *next;
};


struct exit_object { };
extern exit_object endp;
void operator<<( std::ostream &out, exit_object & );

struct InputStream
{
	InputStream( bool handlesLine ) :
		hasData(0),
		eofSent(false),
		flush(false),
		eof(false),
		line(1),
		column(1),
		byte(0),
		handlesLine(handlesLine),
		later(false),
		queue(0), queueTail(0),
		_adHead(0), _adTail(0)
	{}

	virtual ~InputStream() {}

	/* Basic functions. */
	virtual int getData( char *dest, int length ) = 0;
	virtual int isEOF() = 0;
	virtual int needFlush() = 0;
	virtual void pushBackBuf( RunBuf *runBuf ) = 0;
	virtual void append( const char *data, long len ) = 0;
	virtual void append( Tree *tree ) = 0;
	virtual bool tryAgainLater();

	/* Named language elements for patterns and replacements. */
	virtual bool isTree();
	virtual Tree *getTree();
	virtual bool isIgnore();
	virtual bool isLangEl() { return false; }
	virtual KlangEl *getLangEl( long &bindId, char *&data, long &length )
		{ assert( false ); return 0; }
	virtual void pushBackNamed()
		{ assert( false ); }
	
	FsmRun *hasData;

	bool eofSent;
	bool flush;
	bool eof;

	long line;
	long column;
	long byte;

	/* This is set true for input streams that do their own line counting.
	 * Causes FsmRun to ignore NLs. */
	bool handlesLine;
	bool later;

	RunBuf *queue;
	RunBuf *queueTail;

	AccumData *_adHead, *_adTail;

	AccumData *adHead() { return _adHead; }
	AccumData *adTail() { return _adTail; }

	void consumeAd()
	{
		_adHead = _adHead->next;
		if ( _adHead == 0 )
			_adTail = 0;
	}

	void appendAd( AccumData *ad )
	{
		if ( _adHead == 0 ) {
			_adHead = _adTail = ad;
			ad->next = 0;
		}
		else {
			_adTail->next = ad;
			ad->next = 0;
			_adTail = ad;
		}
	}

	RunBuf *head()
	{
		return queue;
	}

	RunBuf *popHead()
	{
		RunBuf *ret = queue;
		queue = queue->next;
		if ( queue == 0 )
			queueTail = 0;
		return ret;
	}

	void prepend( RunBuf *runBuf )
	{
		runBuf->next = queue;
		queue = runBuf;

		if ( queueTail == 0 )
			queueTail = queue;
	}

	void reverseQueue()
	{
		RunBuf *last = 0, *cur = queue;
		queueTail = queue;

		while ( cur != 0 ) {
			/* Save for moving ahead. */
			RunBuf *next = cur->next;

			/* Reverse. */
			cur->next = last;
			cur->prev = next;
			
			/* Move ahead. */
			last = cur;
			cur = next;
		}

		queue = last;
	}
};

struct InputStreamString : public InputStream
{
	InputStreamString( const char *data, long dlen ) :
		InputStream(false), 
		data(data), dlen(dlen), offset(0) {}

	int getData( char *dest, int length );
	int isEOF() { return eof; }
	int needFlush() { return eof; }
	void pushBackBuf( RunBuf *runBuf );
	void append( const char *data, long len ) {}
	void append( Tree *tree ) {}

	const char *data;
	long dlen;
	int offset;
};

struct InputStreamFile : public InputStream
{
	InputStreamFile( FILE *file ) :
		InputStream(false), 
		file(file)
	{}

	int getData( char *dest, int length );
	int isEOF();
	int needFlush();
	void pushBackBuf( RunBuf *runBuf );
	void append( const char *data, long len ) {}
	void append( Tree *tree ) {}

	FILE *file;
};

struct InputStreamFd : public InputStream
{
	InputStreamFd( long fd ) :
		InputStream(false), 
		fd(fd)
	{}

	int isEOF();
	int needFlush();
	int getData( char *dest, int length );
	void pushBackBuf( RunBuf *runBuf );
	void append( const char *data, long len ) {}
	void append( Tree *tree ) {}

	long fd;
};

struct InputStreamAccum : public InputStream
{
	InputStreamAccum()
	:
		InputStream(false), 
		offset(0)
	{}

	int isEOF();
	int needFlush();
	int getData( char *dest, int length );
	void pushBackBuf( RunBuf *runBuf );
	void append( const char *data, long len );
	void append( Tree *tree );
	bool isTree();
	Tree *getTree();

	bool tryAgainLater();


	long offset;
};


struct InputStreamPattern : public InputStream
{
	InputStreamPattern( Pattern *pattern );

	int getData( char *dest, int length );
	int isEOF();
	int needFlush();
	void pushBackBuf( RunBuf *runBuf );
	void append( const char *data, long len ) {}
	virtual void append( Tree *tree ) {}

	bool isLangEl();
	KlangEl *getLangEl( long &bindId, char *&data, long &length );

	void pushBackNamed();

	void backup();
	int shouldFlush();

	Pattern *pattern;
	PatternItem *patItem;
	int offset;
};

struct InputStreamRepl : public InputStream
{
	InputStreamRepl( Replacement *replacement );

	bool isLangEl();
	int getData( char *dest, int length );
	KlangEl *getLangEl( long &bindId, char *&data, long &length );
	int isEOF();
	int needFlush();
	void pushBackBuf( RunBuf *runBuf );
	void append( const char *data, long len ) {}
	virtual void append( Tree *tree ) {}

	void pushBackNamed();

	void backup();
	int shouldFlush();

	Replacement *replacement;
	ReplItem *replItem;
	int offset;
};

#endif /* _INPUT_H */

