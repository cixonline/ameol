#ifndef _BUILDFOLDERLIST_

typedef struct tagNEWSFOLDERLIST {
   PTL FAR * ptlTopicList;          /* Pointer to array of news folder handles */
   int cTopics;                     /* Number of handles */
} NEWSFOLDERLIST, FAR * LPNEWSFOLDERLIST;

BOOL FASTCALL BuildFolderList( LPSTR, LPNEWSFOLDERLIST );

#define _BUILDFOLDERLIST_
#endif
