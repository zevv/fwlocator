#ifndef list_h
#define list_h

#define LIST_ADD_ITEM(list, item) \
	item->prev = NULL; \
	item->next = list; \
	if(item->next) item->next->prev = item; \
	list = item;

#define LIST_REMOVE_ITEM(list, item) \
	if(item == list) list=item->next; \
	if(item->prev) item->prev->next = item->next; \
	if(item->next) item->next->prev = item->prev;

#define LIST_FOREACH(list, cursor, cursor_next) \
	for(cursor=list; cursor_next = (cursor) ? cursor->next : NULL, cursor; cursor=cursor_next) 

#endif

