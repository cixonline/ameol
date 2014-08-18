#ifndef _H_NEWS

#define  LEN_SERVERNAME       64
#define  GRPLIST_VERSION         0x4893//0x4892 !!SM!!

typedef struct tagNEWSSERVERINFO {
   char szShortName[ 9 ];                    /* Short name of server */
   char szServerName[ LEN_SERVERNAME ];      /* Name of server */
   char szAuthInfoUser[ 64 ];                /* Authentication user name (blank if none) */
   char szAuthInfoPass[ 40 ];                /* Authentication user password */
   BOOL fActive;                             /* TRUE if server is active */
   DWORD dwUsenetBatchsize;                  /* Batch size (for cixnews only) */
   BOOL fGetDescriptions;                    /* TRUE if we get newsgroup descriptions */
   char szAccessTimes[ 64 ];                 /* Permitted access times */
   WORD wUpdateDate;                         /* Date when list was last updated */
   WORD wUpdateTime;                         /* Time when list was last updated */
   WORD wNewsPort;                           /* Port number to use (default 119) */
   BOOL fWeekendAccess;                      /* TRUE if we have full access at weekends */
   BOOL fRestrictedAccess;                   /* TRUE if access is restriced to set times */
} NEWSSERVERINFO;

typedef struct tagSUBSCRIBE_INFO {
   char * pszNewsgroup;                      /* Pointer to newsgroup name */
   char * pszNewsServer;                     /* Pointer to server name */
} SUBSCRIBE_INFO;

/* NEWS.C */
BOOL FASTCALL GetUsenetServerDetails( char *, NEWSSERVERINFO * );
BOOL FASTCALL SetUsenetServerDetails( NEWSSERVERINFO * );
void FASTCALL FillUsenetServersCombo( HWND, int, char * );
void FASTCALL UsenetServers_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL UsenetServers_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
BOOL FASTCALL OkayUsenetAccess( char * );
void FASTCALL LoadUsenetHeaders( void );
PTL FASTCALL PtlFromNewsgroup( char * );
BOOL FASTCALL GetNewsgroupNewsServer( char *, char *, int );
BOOL FASTCALL IsCixnewsNewsgroup( char * );
BOOL FASTCALL IsNNTPInFolderList( HPSTR );
LPARAM FASTCALL CmdAddNewsServer( HWND, NEWSSERVERINFO * );
void FASTCALL CmdCreateNewsServer( char * );

/* SUBSCRBE.C */
BOOL FASTCALL CmdSubscribe( HWND, SUBSCRIBE_INFO * );
LPSTR FASTCALL LoadFilters( NEWSSERVERINFO FAR *, WORD * );
LRESULT EXPORT CALLBACK ObProc_UsenetJoin( UINT, HNDFILE, LPVOID, LPSTR );

/* GRPFILTR.C */
BOOL FASTCALL MatchFilters( LPSTR, WORD, char * );

#define _H_NEWS
#endif
