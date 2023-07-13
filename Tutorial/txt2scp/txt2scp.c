/* TXT2SCP.C - Compiles a text file into a scratchpad
 *
 * Copyright 1996 CIX, All Rights Reserved
 *
 * Use, duplication, and disclosure of this file is governed
 * by a license agreement between CIX. and the licensee.
 *
 * Written by Steve Palmer, Manager,
 * CIX Ameol Development Group, Surbiton
 *
 * Description:
 *
 * This simple program takes a TXT file as input and creates a file
 * named TUTORIAL.SCR as output. The input file consists of a number
 * of sections delimited by %START and %END which comprise individual
 * messages in the scratchpad.
 *
 * Usage:  TXT2SCP <filename>
 *
 * Each message begins with a %START marker. The %START marker can be
 * followed by one or more of these parameters:
 *
 *  Folder=<foldername>    Specifies the conference to which subsequent
 *									messages are filed.
 *  Topic=<topicname>      Specifies the topic to which subsequent messages
 *								   are filed. If this is specified without Folder=
 *									in the same %START statement then the topic is
 *									assumed to be within the most recent conference.
 *  Comment						Specifies that this message is a comment to the
 *									previous message
 *
 * For example:
 *
 *    %START Folder=Tutorial Topic=Welcome
 *    Welcome to Ameol2
 *	   ...
 *    %END
 */

#include <stdio.h>
#include <process.h>
#include <ctype.h>
#include <string.h>

#define	MSG_ERROR		1
#define	MSG_HDR			2
#define	MSG_BODY			3
#define	MSG_START		4

char szCompactHdrTmpl[] = ">>>%.14s/%.14s %lu %.14s(%lu)%d%s%2.2d %2.2d:%2.2d";
char szOutputFile[] = "tutorial.scr";

void main( int argc, char * argv[] )
{
	char szConfName[ 80 ];
	char szTopicName[ 80 ];
	char szAuthor[ 80 ];
	FILE * fhSrc;
	FILE * fhOut;
	unsigned long dwMsgStart;
	unsigned long dwMsg;
	unsigned long dwComment;
	unsigned long cbMsg;
	int cMsgLines;
	int state;

	/* Make sure we have just one argument on
	 * the command line.
	 */
	if( argc != 2 )
		{
		fprintf( stderr, "Usage: TXT2SCP filename\n" );
		exit( 1 );
		}

	/* Open the input file
	 */
	fhSrc = fopen( argv[ 1 ], "r" );
	if( NULL == fhSrc )
		{
		fprintf( stderr, "Error: Cannot open %s for reading\n", argv[ 1 ] );
		exit( 2 );
		}

	/* Open the output file.
	 */
	fhOut = fopen( szOutputFile, "wb" );
	if( NULL == fhOut )
		{
		fprintf( stderr, "Error: Cannot open %s for writing\n", szOutputFile );
		exit( 3 );
		}

	/* Show caption.
	 */
	fprintf( stderr, "TXT2SCP v1.01 (c) 1996 CompuLink Information Exchange Ltd\n" );
	fprintf( stderr, "All Rights Reserved\n\n" );

	/* Initialise variables.
	 */
	state = MSG_HDR;
	dwMsg = 1;
	dwComment = 0;
	szConfName[ 0 ] = '\0';
	szTopicName[ 0 ] = '\0';
	strcpy( szAuthor, "Ameol2" );

	/* Loop on the input file and parse the input
	 */
	while( !feof( fhSrc ) && state != MSG_ERROR )
		{
		char sz[ 120 ];

		/* Handle any I/O errors first.
		 */
		if( ferror( fhSrc ) )
			{
			fprintf( stderr, "Error: Cannot read from input file %s\n", argv[ 1 ] );
			break;
			}
		if( ferror( fhOut ) )
			{
			fprintf( stderr, "Error: Cannot write to output file %s\n", szOutputFile );
			break;
			}

		/* Read one line and break if we've got to the end of the
		 * file.
		 */
		if( NULL == fgets( sz, sizeof(sz), fhSrc ) )
			break;
		sz[ strlen( sz ) - 1 ] = '\0';

		/* Now switch depending on the state we're in.
		 */
		switch( state )
			{
			case MSG_HDR:
				if( strncmp( sz, "%START", 6 ) == 0 )
					{
					char * psz;
					
					psz = sz + 7;
					while( *psz )
						{
						while( *psz && isspace( *psz ) )
							++psz;
						if( strncmp( psz, "Comment", 7 ) == 0 )
							{
							/* This message is a comment to the
							 * previous message.
							 */
							dwComment = dwMsg - 1;
							psz += 7;
							}
						else if( strncmp( psz, "Folder", 6 ) == 0 )
							{
							register int c;
							
							/* This is a folder name.
							 */
							psz += 6;
							if( *psz == '=' )
								++psz;
							for( c = 0; *psz && *psz != ' '; ++c )
								szConfName[ c ] = *psz++;
							szConfName[ c ] = '\0';
							szTopicName[ 0 ] = '\0';
							}
						else if( strncmp( psz, "Topic", 5 ) == 0 )
							{
							register int c;
							
							/* This is a topic name.
							 */
							psz += 5;
							if( *psz == '=' )
								++psz;
							for( c = 0; *psz && *psz != ' '; ++c )
								szTopicName[ c ] = *psz++;
							szTopicName[ c ] = '\0';
							dwMsg = 1;
							}
						}
					dwMsgStart = ftell( fhSrc );
					state = MSG_START;
					}
				break;

			case MSG_START:
				/* Remember where this message starts.
				 */
				cbMsg = 0;
				state = MSG_BODY;
				cMsgLines = 0;

			case MSG_BODY:
				if( strcmp( sz, "%END" ) == 0 )
					{
					register int c;

					/* Make sure we have a folder and topic
					 * name!.
					 */
					if( !*szConfName || !*szTopicName )
						{
						fprintf( stderr, "Error: No folder or topic name defined for this message\n" );
						state = MSG_ERROR;
						break;
						}

					/* Create the compact header.
					 */		
					sprintf( sz, szCompactHdrTmpl, szConfName, szTopicName, dwMsg, szAuthor, cbMsg, 20, "Oct", 96, 12, 0 );
					if( dwComment )
						sprintf( sz + strlen( sz ), " c%lu*", dwComment );
					strcat( sz, "\n" );
					fprintf( fhOut, sz );

					/* Now seek back to the start of the message and
					 * copy everything to the output file.
					 */
					fseek( fhSrc, dwMsgStart, SEEK_SET );
					for( c = 0; c < cMsgLines; ++c )
						{
						fgets( sz, sizeof(sz), fhSrc );
						fprintf( fhOut, sz );
						}
					fprintf( fhOut, "\n" );

					/* Done! Set state for next message.
					 */
					++dwMsg;
					dwComment = 0;
					state = MSG_HDR;
					break;
					}
				cbMsg += (unsigned long)strlen( sz ) + 1;
				++cMsgLines;
				break;
			}
		}

	/* We must be in MSG_HDR state
	 */
	if( state != MSG_HDR )
		fprintf( stderr, "Error: End of file found (missing %%END%%)\n" );

	/* Close and exit.
	 */
	fclose( fhSrc );
	fclose( fhOut );
}
