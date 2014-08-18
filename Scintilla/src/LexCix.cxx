// Scintilla source code edit control
/** @file LexCix.cxx
 ** Lexer for CIX-Scripts.
 **/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "Platform.h"

#include "PropSet.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "KeyWords.h"
#include "Scintilla.h"
#include "SciLexer.h"

/*
char HTTPChrTable[ 256 ] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,1,0,1,1,1,1,0,0,0,0,1,0,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,0,1,0,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,0,1,0,0,1,
	0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
*/
char HTTPChrTable[ 256 ] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 15
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 31
	0,1,0,1,1,1,1,0,1,1,0,1,1,1,1,1, // 46
	1,1,1,1,1,1,1,1,1,1,1,0,0,1,0,1, // 61
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 76
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1, // 91
	0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //	106
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0, // 121
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 136
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 151
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 166
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 181
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 196
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 211
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 226
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 241
};

#define	IsHTTPChr(c) 	(HTTPChrTable[c] != 0)


int gLastStyle = SCE_CIX_DEFAULT;

static inline bool IsEOL(const int ch) {
	return  (ch == '\n' || ch == '\r');
}

static inline bool IsEOW(const int ch) {
	return  ( ch == ' ' || ch == '\t' || ch == '"' || IsEOL(ch) || ch == '<' || ch == '>' || ch == ')' || ch == '(' || 
		      ch == '.' || ch == ',' || ch == '?' || ch == '[' || ch == ']');
}

static int POS(char * s, char * l)
{
	char * pDest;

	pDest = strstr(_strlwr(s), l); // FS#185
	if (pDest != NULL)
		return( pDest - s + 1 );
	else
		return 0;
}

static int IsURLPrefix(char * s)
{
	int ret = 0;

	if (s[0] != '\x0')
	{
		if ( POS(s, "http:") == 1){
			ret = 4;
		} else if ( POS(s, "https:") == 1){
			ret = 5;
		} else if ( POS(s, "feed:") == 1){
			ret = 4;
		} else if ( POS(s, "file:") == 1) {
			ret = 4;
		} else if ( POS(s, "mailto:") == 1) {
			ret = 6;
		} else if ( POS(s, "cix:") == 1) {
			ret = 3;
		} else if ( POS(s, "cixfile:") == 1) {
			ret = 7;
		} else if ( POS(s, "am2file:") == 1) {
			ret = 7;
		} else if ( POS(s, "**COPIED FROM: >>>") == 1) {
			ret = 19;
		} else if ( POS(s, "**COPIED") == 1) {
			ret = 7;
		} else if ( POS(s, "ftp:") == 1) {
			ret = 3;
		} else if ( POS(s, "news:") == 1) {
			ret = 4;
		} else if ( POS(s, "gopher:") == 1) {
			ret = 6;
		} else if ( POS(s, "callto:") == 1) {
			ret = 6;
		} else if ( POS(s, "telnet:") == 1) {
			ret = 6;
		} else if ( POS(s, "irc:") == 1) {
			ret = 3;
		} else if ( POS(s, "cixchat:") == 1) {
			ret = 7;
		} else if ( POS(s, "www.") == 1) {
			ret = 3;
		}
	}
	return ret;
}

int gPreviousState;

static int GetNewState(int pCurState, int pNewState)
{

	if (pCurState == SCE_CIX_INVISIBLE )
		pCurState = gPreviousState;

	switch (pCurState)
	{

		case SCE_CIX_DEFAULT:
			return pNewState;

		case SCE_CIX_BOLD:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_DEFAULT;

					case SCE_CIX_BOLD:
						return SCE_CIX_DEFAULT;

					case SCE_CIX_UNDERLINE:
						return SCE_CIX_BOLD_UNDERLINE;

					case SCE_CIX_ITALIC:
						return SCE_CIX_BOLD_ITALIC;

					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_ITALIC;

					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_UNDERLINE;

					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_ITALIC_UNDERLINE;
				}
				break;
			}

		case SCE_CIX_UNDERLINE:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_DEFAULT;

					case SCE_CIX_BOLD:
						return SCE_CIX_BOLD_UNDERLINE;

					case SCE_CIX_ITALIC:
						return SCE_CIX_ITALIC_UNDERLINE;

					case SCE_CIX_UNDERLINE:
						return SCE_CIX_DEFAULT;

					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_ITALIC;

					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_UNDERLINE;

					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_ITALIC_UNDERLINE;
				}
				break;
			}

		case SCE_CIX_ITALIC:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_DEFAULT;

					case SCE_CIX_BOLD:
						return SCE_CIX_BOLD_ITALIC;

					case SCE_CIX_ITALIC:
						return SCE_CIX_DEFAULT;

					case SCE_CIX_UNDERLINE:
						return SCE_CIX_ITALIC_UNDERLINE;

					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_ITALIC;

					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_UNDERLINE;

					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_ITALIC_UNDERLINE;
				}
				break;
			}

		case SCE_CIX_BOLD_UNDERLINE:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_DEFAULT;

					case SCE_CIX_BOLD:
						return SCE_CIX_UNDERLINE;

					case SCE_CIX_ITALIC:
						return SCE_CIX_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_UNDERLINE:
						return SCE_CIX_BOLD;

					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_ITALIC_UNDERLINE;

					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_DEFAULT;

					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_ITALIC;
				}
				break;
			}

		case SCE_CIX_BOLD_ITALIC:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_DEFAULT;

					case SCE_CIX_BOLD:
						return SCE_CIX_ITALIC;

					case SCE_CIX_ITALIC:
						return SCE_CIX_BOLD;

					case SCE_CIX_UNDERLINE:
						return SCE_CIX_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_DEFAULT;

					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_ITALIC_UNDERLINE;

					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_BOLD_UNDERLINE;

					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_UNDERLINE;
				}
				break;
			}

		case SCE_CIX_ITALIC_UNDERLINE:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_DEFAULT;

					case SCE_CIX_BOLD:
						return SCE_CIX_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_ITALIC:
						return SCE_CIX_UNDERLINE;

					case SCE_CIX_UNDERLINE:
						return SCE_CIX_ITALIC;

					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_BOLD_UNDERLINE;

					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_BOLD_ITALIC;

					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_DEFAULT;

					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_BOLD;
				}
				break;
			}

		case SCE_CIX_BOLD_UNDERLINE_ITALIC:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_DEFAULT;

					case SCE_CIX_BOLD:
						return SCE_CIX_ITALIC_UNDERLINE;

					case SCE_CIX_ITALIC:
						return SCE_CIX_BOLD_UNDERLINE;

					case SCE_CIX_UNDERLINE:
						return SCE_CIX_BOLD_ITALIC;

					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_UNDERLINE;

					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_ITALIC;

					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_BOLD;

					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_DEFAULT;
				}
				break;
			}

		case SCE_CIX_QUOTED_BOLD:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_BOLD:
					case SCE_CIX_BOLD:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_ITALIC:
					case SCE_CIX_ITALIC:
						return SCE_CIX_QUOTED_BOLD_ITALIC;

					case SCE_CIX_QUOTED_UNDERLINE:
					case SCE_CIX_UNDERLINE:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE:
					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE;

					case SCE_CIX_QUOTED_BOLD_ITALIC:
					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_QUOTED_ITALIC;

					case SCE_CIX_QUOTED_ITALIC_UNDERLINE:
					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC:
					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_QUOTED_ITALIC_UNDERLINE;

					case SCE_CIX_QUOTED_HOTLINK:
					case SCE_CIX_HOTLINK:
						return SCE_CIX_QUOTED_HOTLINK;
				}
			}

		case SCE_CIX_QUOTED_UNDERLINE:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_BOLD:
					case SCE_CIX_BOLD:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE;

					case SCE_CIX_QUOTED_ITALIC:
					case SCE_CIX_ITALIC:
						return SCE_CIX_QUOTED_ITALIC_UNDERLINE;

					case SCE_CIX_QUOTED_UNDERLINE:
					case SCE_CIX_UNDERLINE:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE:
					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_QUOTED_UNDERLINE;

					case SCE_CIX_QUOTED_BOLD_ITALIC:
					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_QUOTED_ITALIC_UNDERLINE:
					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_QUOTED_ITALIC;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC:
					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_QUOTED_BOLD_ITALIC;

					case SCE_CIX_QUOTED_HOTLINK:
					case SCE_CIX_HOTLINK:
						return SCE_CIX_QUOTED_HOTLINK;
				}
			}

		case SCE_CIX_QUOTED_ITALIC:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_BOLD:
					case SCE_CIX_BOLD:
						return SCE_CIX_QUOTED_BOLD_ITALIC;

					case SCE_CIX_QUOTED_ITALIC:
					case SCE_CIX_ITALIC:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_UNDERLINE:
					case SCE_CIX_UNDERLINE:
						return SCE_CIX_QUOTED_ITALIC_UNDERLINE;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE:
					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_QUOTED_BOLD_ITALIC:
					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_QUOTED_BOLD;

					case SCE_CIX_QUOTED_ITALIC_UNDERLINE:
					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_QUOTED_UNDERLINE;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC:
					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_QUOTED_BOLD_ITALIC;

					case SCE_CIX_QUOTED_HOTLINK:
					case SCE_CIX_HOTLINK:
						return SCE_CIX_QUOTED_HOTLINK;
				}
			}

		case SCE_CIX_QUOTED_BOLD_UNDERLINE:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_BOLD:
					case SCE_CIX_BOLD:
						return SCE_CIX_QUOTED_UNDERLINE;

					case SCE_CIX_QUOTED_ITALIC:
					case SCE_CIX_ITALIC:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_QUOTED_UNDERLINE:
					case SCE_CIX_UNDERLINE:
						return SCE_CIX_QUOTED_BOLD;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE:
					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_BOLD_ITALIC:
					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_QUOTED_ITALIC_UNDERLINE;

					case SCE_CIX_QUOTED_ITALIC_UNDERLINE:
					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC:
					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_QUOTED_BOLD_ITALIC;

					case SCE_CIX_QUOTED_HOTLINK:
					case SCE_CIX_HOTLINK:
						return SCE_CIX_QUOTED_HOTLINK;
				}
			}
		case SCE_CIX_QUOTED_BOLD_ITALIC:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_BOLD:
					case SCE_CIX_BOLD:
						return SCE_CIX_QUOTED_ITALIC;

					case SCE_CIX_QUOTED_ITALIC:
					case SCE_CIX_ITALIC:
						return SCE_CIX_QUOTED_BOLD;

					case SCE_CIX_QUOTED_UNDERLINE:
					case SCE_CIX_UNDERLINE:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE:
					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_QUOTED_ITALIC_UNDERLINE;

					case SCE_CIX_QUOTED_BOLD_ITALIC:
					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_ITALIC_UNDERLINE:
					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC:
					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_QUOTED_UNDERLINE;

					case SCE_CIX_QUOTED_HOTLINK:
					case SCE_CIX_HOTLINK:
						return SCE_CIX_QUOTED_HOTLINK;
				}
			}

		case SCE_CIX_QUOTED_ITALIC_UNDERLINE:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_BOLD:
					case SCE_CIX_BOLD:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_QUOTED_ITALIC:
					case SCE_CIX_ITALIC:
						return SCE_CIX_QUOTED_UNDERLINE;

					case SCE_CIX_QUOTED_UNDERLINE:
					case SCE_CIX_UNDERLINE:
						return SCE_CIX_QUOTED_ITALIC;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE:
					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_QUOTED_BOLD_ITALIC;

					case SCE_CIX_QUOTED_BOLD_ITALIC:
					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_QUOTED_UNDERLINE;

					case SCE_CIX_QUOTED_ITALIC_UNDERLINE:
					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC:
					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_QUOTED_BOLD;

					case SCE_CIX_QUOTED_HOTLINK:
					case SCE_CIX_HOTLINK:
						return SCE_CIX_QUOTED_HOTLINK;
				}
			}

		case SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_BOLD:
					case SCE_CIX_BOLD:
						return SCE_CIX_QUOTED_ITALIC_UNDERLINE;

					case SCE_CIX_QUOTED_ITALIC:
					case SCE_CIX_ITALIC:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE;

					case SCE_CIX_QUOTED_UNDERLINE:
					case SCE_CIX_UNDERLINE:
						return SCE_CIX_QUOTED_BOLD_ITALIC;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE:
					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_QUOTED_ITALIC;

					case SCE_CIX_QUOTED_BOLD_ITALIC:
					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_QUOTED_UNDERLINE;

					case SCE_CIX_QUOTED_ITALIC_UNDERLINE:
					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_QUOTED_BOLD;

					case SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC:
					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_QUOTED;

					case SCE_CIX_QUOTED_HOTLINK:
					case SCE_CIX_HOTLINK:
						return SCE_CIX_QUOTED_HOTLINK;
				}
			}

		case SCE_CIX_QUOTED_HOTLINK:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_QUOTED;

					case SCE_CIX_BOLD:
						return SCE_CIX_QUOTED_BOLD;

					case SCE_CIX_ITALIC:
						return SCE_CIX_QUOTED_ITALIC;

					case SCE_CIX_UNDERLINE:
						return SCE_CIX_QUOTED_UNDERLINE;

					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE;

					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_QUOTED_BOLD_ITALIC;

					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_QUOTED_ITALIC_UNDERLINE;

					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC;
//						return SCE_CIX_QUOTED;

					case SCE_CIX_HOTLINK:
						return SCE_CIX_QUOTED;
				}
			}

		case SCE_CIX_QUOTED:
			{
				switch(pNewState)
				{
					case SCE_CIX_DEFAULT:
						return SCE_CIX_QUOTED;

					case SCE_CIX_BOLD:
						return SCE_CIX_QUOTED_BOLD;

					case SCE_CIX_ITALIC:
						return SCE_CIX_QUOTED_ITALIC;

					case SCE_CIX_UNDERLINE:
						return SCE_CIX_QUOTED_UNDERLINE;

					case SCE_CIX_BOLD_UNDERLINE:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE;

					case SCE_CIX_BOLD_ITALIC:
						return SCE_CIX_QUOTED_BOLD_ITALIC;

					case SCE_CIX_ITALIC_UNDERLINE:
						return SCE_CIX_QUOTED_ITALIC_UNDERLINE;

					case SCE_CIX_BOLD_UNDERLINE_ITALIC:
						return SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC;

					case SCE_CIX_HOTLINK:
						return SCE_CIX_QUOTED_HOTLINK;
				}
//				return SCE_CIX_QUOTED;
			}
	}
	return pNewState;
}


static inline bool FindTerminator(unsigned int startPos, int length, unsigned int pChar, Accessor &styler) 
{
	unsigned int ch, chNext;

	for (unsigned int j = startPos; j < (startPos + length); j++) 
	{
		ch     = styler.SafeGetCharAt(j);
		chNext = styler.SafeGetCharAt(j+1);
		
		if(ch == pChar)
			if( chNext == ' ' || chNext == '_' || chNext == '*' || chNext == '/' || chNext == ch || IsEOL(chNext) || IsEOW(chNext))
				return(1);
			else
				return(0);

		if(IsEOL(ch))
			return(0);
	}
	return(0);
}

static void ColouriseCixDocSimple(unsigned int startPos, int length, int initStyle, WordList *keywordlists[], Accessor &styler) {

	char cWord[4096];
	unsigned int pos, i, lChg, ch, chNext, chNext1, chNext2, chNext3, chNext4, chNext5, chNext6, lLineStart, lastChar;
	unsigned lEndPos;
	int lHideStyleChars = 1;
	int lWrapURLs = styler.GetPropertyInt("MultiLineURLs", 0);

	// NOP to silence warning 4100 about unreferenced parameter.
	keywordlists = keywordlists;
	
	styler.StartAt(startPos);

	lEndPos = startPos + length;

	StyleContext sc(startPos, length, initStyle, styler);
 
 	i = 0;
	lastChar = 0;
	lLineStart = 1;

	unsigned int j = 0;
	for (j = startPos; j < lEndPos; j++) {
//	for (unsigned int j = startPos; sc.More(), j < (startPos + length); sc.Forward(), j++) {


		ch = styler.SafeGetCharAt(j);
		chNext  = ((j + 1) >= lEndPos) ? '\x0' : styler.SafeGetCharAt(j + 1);
		chNext1 = ((j + 2) >= lEndPos) ? '\x0' : styler.SafeGetCharAt(j + 2);
		chNext2 = ((j + 3) >= lEndPos) ? '\x0' : styler.SafeGetCharAt(j + 3);
		chNext3 = ((j + 4) >= lEndPos) ? '\x0' : styler.SafeGetCharAt(j + 4);
		chNext4 = ((j + 5) >= lEndPos) ? '\x0' : styler.SafeGetCharAt(j + 5);
		chNext5 = ((j + 6) >= lEndPos) ? '\x0' : styler.SafeGetCharAt(j + 6);
		chNext6 = ((j + 7) >= lEndPos) ? '\x0' : styler.SafeGetCharAt(j + 7);

		if( IsEOL(lastChar) || (j == startPos))
			lLineStart = 1;
		else
			lLineStart = 0;

		if (( j == startPos || lLineStart == 1 ) && 
			( ch == '*' && chNext == '*' && chNext1 == 'C' && chNext2 == 'O' && chNext3 == 'P' && chNext4 == 'I' && chNext5 == 'E' && chNext6 == 'D'))
		{
			sc.currentPos = j ;//- 8;
			sc.SetState(GetNewState(sc.state, SCE_CIX_HOTLINK));
			while(styler.SafeGetCharAt(j++) != ' ' && j < (unsigned int) lEndPos) // **COPIED
				;
			while(styler.SafeGetCharAt(j++) == ' ' && j < (unsigned int) lEndPos) // 
				;
			while(styler.SafeGetCharAt(j++) != ' ' && j < (unsigned int) lEndPos) // FROM:
				;
			while(styler.SafeGetCharAt(j++) == ' ' && j < (unsigned int) lEndPos)
				;
			ch = styler.SafeGetCharAt(j++);
			while( (ch != ' ') && !IsEOL(ch) && j < (unsigned int) lEndPos) // >>>cix://??
				ch = styler.SafeGetCharAt(j++);
			while(styler.SafeGetCharAt(j++) == ' ' && j < (unsigned int) lEndPos)
				;
			
			ch = styler.SafeGetCharAt(j++);
			while( ( ch  != ' ' ) && ( j < (unsigned int) lEndPos ) && (!IsEOL(ch)) ) //  No.??
				ch = styler.SafeGetCharAt(j++);

			sc.currentPos = j-1;
			sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
		}

		if( i < 4096)
		{
			if(IsEOW(ch) /*&& !IsURLPrefix(cWord)*/) // End of Word
			{
				cWord[i] = ch;  
				cWord[i+1] = '\x0';
				i = 0;
			}
			else
			{
				cWord[i] = ch;
				i++;
				cWord[i] = '\x0';
			}
		}
		else
			i = 0;

		lChg = 0;


		if (sc.state == SCE_CIX_BOLD || 
			sc.state == SCE_CIX_BOLD_ITALIC || sc.state == SCE_CIX_BOLD_UNDERLINE     || sc.state == SCE_CIX_BOLD_UNDERLINE_ITALIC ||
			sc.state == SCE_CIX_QUOTED_BOLD || sc.state == SCE_CIX_QUOTED_BOLD_ITALIC || sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE || sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC
			) 
		{
			if (ch == '*' || IsEOL(ch)) 
			{
				gLastStyle = sc.state;
				sc.currentPos = j;
				if (lHideStyleChars)
				{
					gPreviousState = sc.state;
					sc.SetState(SCE_CIX_INVISIBLE);
					while (ch == '*' || ch == '_' || ch == '/' ) 
					{
						ch = styler.SafeGetCharAt(++j);
					}
					sc.currentPos = j;
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
				}
				else
					sc.ForwardSetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
				lChg = 1;
			}
		} 

		if (sc.state == SCE_CIX_UNDERLINE || 
			sc.state == SCE_CIX_ITALIC_UNDERLINE || sc.state == SCE_CIX_BOLD_UNDERLINE || sc.state == SCE_CIX_BOLD_UNDERLINE_ITALIC ||
			sc.state == SCE_CIX_QUOTED_UNDERLINE || sc.state == SCE_CIX_QUOTED_ITALIC_UNDERLINE || sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE || sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC
		   ) 
		{
			if (ch == '_' || IsEOL(ch)) 
			{
				gLastStyle = sc.state;
				sc.currentPos = j;
				if (lHideStyleChars)
				{
					gPreviousState = sc.state;
					sc.SetState(SCE_CIX_INVISIBLE);
					while (ch == '*' || ch == '_' || ch == '/' ) 
					{
						ch = styler.SafeGetCharAt(++j);
					}
					sc.currentPos = j;
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
				}
				else
					sc.ForwardSetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
				lChg = 1;
			}
		} 
		
		if (sc.state == SCE_CIX_ITALIC || 
			sc.state == SCE_CIX_ITALIC_UNDERLINE || sc.state == SCE_CIX_BOLD_ITALIC || sc.state == SCE_CIX_BOLD_UNDERLINE_ITALIC ||
			sc.state == SCE_CIX_QUOTED_ITALIC    || sc.state == SCE_CIX_QUOTED_ITALIC_UNDERLINE || sc.state == SCE_CIX_QUOTED_BOLD_ITALIC || sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC
		   ) 
		{
			if (ch == '/' || IsEOL(ch)) 
			{
				gLastStyle = sc.state;
				sc.currentPos = j;
				if (lHideStyleChars)
				{
					gPreviousState = sc.state;
					sc.SetState(SCE_CIX_INVISIBLE);
					while (ch == '*' || ch == '_' || ch == '/' ) 
					{
						ch = styler.SafeGetCharAt(++j);
					}
					sc.currentPos = j;
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
				}
				else
					sc.ForwardSetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
				lChg = 1;
			}
		} 
		
		if (sc.state == SCE_CIX_QUOTED ||
		    sc.state == SCE_CIX_QUOTED ||
			sc.state == SCE_CIX_QUOTED_BOLD ||
			sc.state == SCE_CIX_QUOTED_UNDERLINE ||
			sc.state == SCE_CIX_QUOTED_ITALIC ||
			sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE ||
			sc.state == SCE_CIX_QUOTED_BOLD_ITALIC ||
			sc.state == SCE_CIX_QUOTED_ITALIC_UNDERLINE ||
			sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC ||
			sc.state == SCE_CIX_QUOTED_HOTLINK
			) 
		{
			if ( IsEOL(ch) ) 
			{
				gLastStyle = sc.state;
				sc.currentPos = j;
				sc.ForwardSetState(SCE_CIX_DEFAULT);
				lChg = 1;
			}
		} 

		if (sc.state == SCE_CIX_HOTLINK || sc.state == SCE_CIX_QUOTED_HOTLINK) 
		{
			if(ch == ' ' && (j == lEndPos - 1 ) ) //!!SM!! 2046
			{
					sc.currentPos = j;
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
					lChg = 1;
			}
			else if (chNext == ' ' && chNext2 == 0) // FS#185
			{
					sc.currentPos = j + 1;
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
					lChg = 1;
			}
			else if (j == lEndPos - 1 )
			{
					sc.currentPos = j + 1;
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
					lChg = 1;
			}
			// 20060506 2.56.2051 FS#128
			else if (ispunct(chNext) && ch != '!' && ch != '+' && ch != '_' && ch != '/' && ch != ';' && j == lEndPos - 2 )
			{
					sc.currentPos = j + 1;
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
					lChg = 1;
			}
			else if(   ( ispunct(ch) && ch != '!' && ch != '+' && ch != '_' && ch != '/' && ch != ';') && 
				       ( chNext == ' ' || chNext == '\t' || (IsEOL(chNext) && !lWrapURLs ) )
				   )
			{
					sc.currentPos = j;
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
					lChg = 1;
			}
			// 20060506 2.56.2051 FS#92
			else if((ch == '>') || ((  ch == '(' || ch == ')' ) && ( (chNext1=='.' && (IsEOL(chNext2) || chNext2==' ')) || chNext == ' ' || j+1 == lEndPos)))
			{
					sc.currentPos = j;
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
					lChg = 1;
			}
			else if (IsEOL( ch ) && IsEOL( chNext ))
			{
					// This will allow URL's to go onto other lines, but then it's a problem with when to stop them.
					if (!lWrapURLs || ((!IsHTTPChr(chNext1) || (IsEOL( chNext1 ) && IsEOL( chNext2 )))))
					{
						sc.currentPos = j;
						sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
						lChg = 1;
					}
			}
			else if (!IsHTTPChr(ch) && !IsEOL( ch ) && !IsEOL( chNext ) && ch != ';')
			{
					sc.currentPos = j;
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
					lChg = 1;
			}
			else if( ch == ' ' || ch == '\t' )  
			{
					sc.currentPos = j;
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
					lChg = 1;
			}

		} 

		if (sc.state != SCE_CIX_HOTLINK && sc.state != SCE_CIX_QUOTED_HOTLINK && (lChg == 0) && IsHTTPChr(ch)) 
		{
			pos = IsURLPrefix(cWord);
			if ( (pos > 0) || (( i >= 2 ) && IsHTTPChr(cWord[i - 2])) )
			{
				if ( pos > 0 && chNext != ' ' && !IsEOL( ch ) && !IsEOL( chNext )) 
				{	
					if (sc.currentPos < j - pos )
						sc.currentPos = j - pos ;
					sc.SetState(GetNewState(sc.state, SCE_CIX_HOTLINK));
				}
			}
		} 

		if (sc.state != SCE_CIX_HOTLINK && (lChg == 0) && (((lastChar == ' ' || lastChar == '\t') && chNext != ' ') || lLineStart || 
			sc.state == SCE_CIX_INVISIBLE ||
			sc.state == SCE_CIX_BOLD || sc.state == SCE_CIX_UNDERLINE || sc.state == SCE_CIX_ITALIC ||
            sc.state == SCE_CIX_BOLD_UNDERLINE || sc.state == SCE_CIX_BOLD_ITALIC || sc.state == SCE_CIX_ITALIC_UNDERLINE ||sc.state == SCE_CIX_BOLD_UNDERLINE_ITALIC  ||
            sc.state == SCE_CIX_QUOTED_BOLD || sc.state == SCE_CIX_QUOTED_ITALIC || sc.state == SCE_CIX_QUOTED_UNDERLINE ||
			sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE || sc.state == SCE_CIX_QUOTED_BOLD_ITALIC || sc.state == SCE_CIX_QUOTED_ITALIC_UNDERLINE || sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC)) 
		{
			if (ch == '*' && chNext != '*' ) 
			{
				if( FindTerminator(j + 1, length, '*', styler) )
				{
					sc.currentPos = j;
					if (lHideStyleChars)
					{
						gPreviousState = sc.state;
						sc.SetState(SCE_CIX_INVISIBLE);
						//sc.currentPos = j + 1;
						sc.ForwardSetState(GetNewState(sc.state, SCE_CIX_BOLD));
					}
					else
						sc.SetState(GetNewState(sc.state, SCE_CIX_BOLD));
				}
			} 
			else if (ch == '_' && chNext != '_' ) 
			{
				if( FindTerminator(j + 1, length, '_', styler) )
				{
					sc.currentPos = j;
					if (lHideStyleChars)
					{
						gPreviousState = sc.state;
						sc.SetState(SCE_CIX_INVISIBLE);
						//sc.currentPos = j + 1;
						sc.ForwardSetState(GetNewState(sc.state, SCE_CIX_UNDERLINE));
					}
					else
						sc.SetState(GetNewState(sc.state, SCE_CIX_UNDERLINE));
				}
			} 
			else if (ch == '/' && chNext != '/' ) 
			{
				if( FindTerminator(j + 1, length, '/', styler) )
				{
					sc.currentPos = j;
					if (lHideStyleChars)
					{
						gPreviousState = sc.state;
						sc.SetState(SCE_CIX_INVISIBLE);
						//sc.currentPos = j + 1;
						sc.ForwardSetState(GetNewState(sc.state, SCE_CIX_ITALIC));
					}
					else
						sc.SetState(GetNewState(sc.state, SCE_CIX_ITALIC));
				}
			} 
			else if (ch == '>' && ( lLineStart || j == startPos) ) 
			{
				sc.currentPos = j;
				sc.SetState(GetNewState(sc.state, SCE_CIX_QUOTED));
			}
		} 

		lastChar = ch;
	}

    sc.currentPos = j - 1;

	for (; sc.More(); sc.Forward())
		;

	sc.Complete();
}

LexerModule lmCix(SCLEX_CIX, ColouriseCixDocSimple, "cix");

		/*
		if (sc.state == SCE_CIX_BOLD || 
			sc.state == SCE_CIX_BOLD_ITALIC || sc.state == SCE_CIX_BOLD_UNDERLINE     || sc.state == SCE_CIX_BOLD_UNDERLINE_ITALIC ||
			sc.state == SCE_CIX_QUOTED_BOLD || sc.state == SCE_CIX_QUOTED_BOLD_ITALIC || sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE || sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC
			) 
		{
			if (ch == '*' || IsEOL(ch)) 
			{
				gLastStyle = sc.state;
				sc.currentPos = j;
				if (lHideStyleChars)
				{
					gPreviousState = sc.state;
					sc.ForwardSetState(SCE_CIX_INVISIBLE);
					sc.ForwardSetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
				}
				else
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
				lChg = 1;
			}
		} 

		else if (sc.state == SCE_CIX_UNDERLINE || 
			     sc.state == SCE_CIX_ITALIC_UNDERLINE || sc.state == SCE_CIX_BOLD_UNDERLINE || sc.state == SCE_CIX_BOLD_UNDERLINE_ITALIC ||
			     sc.state == SCE_CIX_QUOTED_UNDERLINE || sc.state == SCE_CIX_QUOTED_ITALIC_UNDERLINE || sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE || sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC
				 ) 
		{
			if (ch == '_' || IsEOL(ch)) 
			{
				gLastStyle = sc.state;
				sc.currentPos = j;
				if (lHideStyleChars)
				{
					gPreviousState = sc.state;
					sc.ForwardSetState(SCE_CIX_INVISIBLE);
					sc.ForwardSetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
				}
				else
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
				lChg = 1;
			}
		} 
		else if (sc.state == SCE_CIX_ITALIC || 
			     sc.state == SCE_CIX_ITALIC_UNDERLINE || sc.state == SCE_CIX_BOLD_ITALIC || sc.state == SCE_CIX_BOLD_UNDERLINE_ITALIC ||
			     sc.state == SCE_CIX_QUOTED_ITALIC    || sc.state == SCE_CIX_QUOTED_ITALIC_UNDERLINE || sc.state == SCE_CIX_QUOTED_BOLD_ITALIC || sc.state == SCE_CIX_QUOTED_BOLD_UNDERLINE_ITALIC
				 ) 
		{
			if (ch == '/' || IsEOL(ch)) 
			{
				gLastStyle = sc.state;
				sc.currentPos = j;
				if (lHideStyleChars)
				{
					gPreviousState = sc.state;
					sc.ForwardSetState(SCE_CIX_INVISIBLE);
					sc.ForwardSetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
				}
				else
					sc.SetState(GetNewState(sc.state, SCE_CIX_DEFAULT));
				lChg = 1;
			}
		} 
		*/

/*		
		if (sc.state != SCE_CIX_HOTLINK && (lChg == 0) && ((lastChar == ' ' && chNext != ' ') || lLineStart )) 
		{
			if (ch == '*') 
			{
				sc.currentPos = j;
				sc.SetState(SCE_CIX_BOLD);
			} 
			else if (ch == '_') 
			{
				sc.currentPos = j;
				sc.SetState(SCE_CIX_UNDERLINE);
			} 
			else if (ch == '/') 
			{
				sc.currentPos = j;
				sc.SetState(SCE_CIX_ITALIC);
			} 
			else if (ch == '>' && ( lLineStart || j == startPos) ) 
			{
				sc.currentPos = j;
				sc.SetState(SCE_CIX_QUOTED);
			}
		} 
*/

/*

			if ((j == (startPos + length) - 1 ) ||
			    ((ispunct(ch) && (chNext == ' ' || IsEOL(chNext)))) ||
				( ch == '>' || (ch == ')' && (chNext == '.' || chNext == ','))) ||
				(IsEOL( ch ) && IsEOL( chNext )) ||
				(!IsHTTPChr(ch) && !IsEOL( ch ) && !IsEOL( chNext ) ) ||
				( ch == ')' || ch == '(' || ch == ' ')
			   )
			{
					sc.currentPos = j + 1;
					sc.SetState(SCE_CIX_DEFAULT);
					lChg = 1;
			}

*/
