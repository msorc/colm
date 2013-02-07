/*
 *  Copyright 2007-2012 Adrian Thurston <thurston@complang.org>
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

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FSM_BUFSIZE 8192
//#define FSM_BUFSIZE 8

#define INPUT_DATA     1
/* This is for data sources to return, not for the wrapper. */
#define INPUT_EOD      2
#define INPUT_EOF      3
#define INPUT_EOS      4
#define INPUT_LANG_EL  5
#define INPUT_TREE     6
#define INPUT_IGNORE   7

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

struct LangEl;
struct Pattern;
struct PatternItem;
struct Constructor;
struct ConsItem;
struct _FsmRun;
struct ColmTree;

enum RunBufType {
	RunBufDataType = 0,
	RunBufTokenType,
	RunBufIgnoreType,
	RunBufSourceType
};

typedef struct _RunBuf
{
	enum RunBufType type;
	char data[FSM_BUFSIZE];
	long length;
	struct ColmTree *tree;
	long offset;
	struct _RunBuf *next, *prev;
} RunBuf;

RunBuf *newRunBuf();

typedef struct _StreamImpl StreamImpl;

struct StreamFuncs
{
	int (*getParseBlock)( struct _FsmRun *fsmRun, StreamImpl *ss,
			int skip, char **pdp, int *copied );

	int (*getData)( struct _FsmRun *fsmRun, StreamImpl *ss,
			int offset, char *dest, int length );

	int (*consumeData)( StreamImpl *ss, int length );
	int (*undoConsumeData)( struct _FsmRun *fsmRun, StreamImpl *ss, const char *data, int length );

	struct ColmTree *(*consumeTree)( StreamImpl *ss );
	void (*undoConsumeTree)( StreamImpl *ss, struct ColmTree *tree, int ignore );

	/* Language elments (compile-time). */
	struct LangEl *(*consumeLangEl)( StreamImpl *ss, long *bindId, char **data, long *length );
	void (*undoConsumeLangEl)( StreamImpl *ss );

	/* Private implmentation for some shared get data functions. */
	int (*getDataSource)( StreamImpl *ss, char *dest, int length );

	void (*setEof)( StreamImpl *is );
	void (*unsetEof)( StreamImpl *is );

	/* Prepending to a stream. */
	void (*prependData)( StreamImpl *in, const char *data, long len );
	void (*prependTree)( StreamImpl *is, struct ColmTree *tree, int ignore );
	void (*prependStream)( StreamImpl *in, struct ColmTree *tree );
	int (*undoPrependData)( StreamImpl *is, int length );
	struct ColmTree *(*undoPrependTree)( StreamImpl *is );
	struct ColmTree *(*undoPrependStream)( StreamImpl *in );

	/* Appending to a stream. */
	void (*appendData)( StreamImpl *in, const char *data, long len );
	void (*appendTree)( StreamImpl *in, struct ColmTree *tree );
	void (*appendStream)( StreamImpl *in, struct ColmTree *tree );
	struct ColmTree *(*undoAppendData)( StreamImpl *in, int length );
	struct ColmTree *(*undoAppendTree)( StreamImpl *in );
	struct ColmTree *(*undoAppendStream)( StreamImpl *in );
};

/* List of source streams. Enables streams to be pushed/popped. */
struct _StreamImpl
{
	struct StreamFuncs *funcs;

	char eofSent;
	char eof;
	char eosSent;

	RunBuf *queue;
	RunBuf *queueTail;

	const char *data;
	long dlen;
	int offset;

	long line;
	long column;
	long byte;

	FILE *file;
	long fd;

	struct Pattern *pattern;
	struct PatternItem *patItem;
	struct Constructor *constructor;
	struct ConsItem *consItem;
};

StreamImpl *newSourceStreamPat( struct Pattern *pattern );
StreamImpl *newSourceStreamCons( struct Constructor *constructor );
StreamImpl *newSourceStreamFile( FILE *file );
StreamImpl *newSourceStreamFd( long fd );

void initInputFuncs();
void initStaticFuncs();
void initPatFuncs();
void initConsFuncs();

/* The input stream interface. */

int _getParseBlock( struct _FsmRun *fsmRun, StreamImpl *in,
		int skip, char **pdp, int *copied );
int _consumeData( StreamImpl *in, int length );
int _undoConsumeData( struct _FsmRun *fsmRun, StreamImpl *is, const char *data, int length );

struct ColmTree *_consumeTree( StreamImpl *in );
void _undoConsumeTree( StreamImpl *in, struct ColmTree *tree, int ignore );

struct LangEl *_consumeLangEl( StreamImpl *in, long *bindId, char **data, long *length );
void _undoConsumeLangEl( StreamImpl *in );

void _setEof( StreamImpl *is );
void _unsetEof( StreamImpl *is );

void _prependData( StreamImpl *in, const char *data, long len );
void _prependTree( StreamImpl *is, struct ColmTree *tree, int ignore );
void _prependStream( StreamImpl *in, struct ColmTree *tree );
int _undoPrependData( StreamImpl *is, int length );
struct ColmTree *_undoPrependTree( StreamImpl *is );
struct ColmTree *_undoPrependStream( StreamImpl *in );

void _appendData( StreamImpl *in, const char *data, long len );
void _appendTree( StreamImpl *in, struct ColmTree *tree );
void _appendStream( StreamImpl *in, struct ColmTree *tree );
struct ColmTree *_undoAppendData( StreamImpl *in, int length );
struct ColmTree *_undoAppendTree( StreamImpl *in );
struct ColmTree *_undoAppendStream( StreamImpl *in );

#ifdef __cplusplus
}
#endif

#endif /* _INPUT_H */
