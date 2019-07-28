/*
Copyright (C) 2005  Georgia Public Library Service 
Bill Erickson <highfalutin@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <opensrf/osrf_cache.h>
#include <ctype.h>

#define MAX_KEY_LEN 250

static struct memcached_st* _osrfCache = NULL;
static time_t _osrfCacheMaxSeconds = -1;
static char* _clean_key( const char* );

int osrfCacheInit( const char* serverStrings[], int size, time_t maxCacheSeconds ) {
	memcached_server_st *server_pool;
	memcached_return rc;

	if( !(serverStrings && size > 0) ) return -1;
	osrfCacheCleanup(); /* in case we've already been init-ed */

	int i;
	_osrfCache = memcached_create(NULL);
	_osrfCacheMaxSeconds = maxCacheSeconds;

	for( i = 0; i < size && serverStrings[i]; i++ ) {
		/* TODO: modify caller to pass a list of servers all at once */
		server_pool = memcached_servers_parse(serverStrings[i]);
		rc = memcached_server_push(_osrfCache, server_pool);
		if (rc != MEMCACHED_SUCCESS) {
			osrfLogError(OSRF_LOG_MARK,
				"Failed to add memcached server: %s - %s",
				serverStrings[i], memcached_strerror(_osrfCache, rc));
		}
	}

	return 0;
}

int osrfCachePutObject( const char* key, const jsonObject* obj, time_t seconds ) {
	if( !(key && obj) ) return -1;
	char* s = jsonObjectToJSON( obj );
	osrfLogInternal( OSRF_LOG_MARK, "osrfCachePut(): Putting object (key=%s): %s", key, s);
	osrfCachePutString(key, s, seconds);
	free(s);
	return 0;
}

char* _clean_key( const char* key ) {
    char* clean_key = (char*)strdup(key);
    char* d = clean_key;
    char* s = clean_key;
    do {
        while(isspace(*s) || ((*s != '\0') && iscntrl(*s))) s++;
    } while((*d++ = *s++));
    if (strlen(clean_key) > MAX_KEY_LEN) {
        char *hashed = md5sum(clean_key);
        clean_key[0] = '\0';
        strncat(clean_key, "shortened_", 11);
        strncat(clean_key, hashed, MAX_KEY_LEN);
        free(hashed);
    }
    return clean_key;
}

int osrfCachePutString( const char* key, const char* value, time_t seconds ) {
	memcached_return rc;
	if( !(key && value) ) return -1;
	seconds = (seconds <= 0 || seconds > _osrfCacheMaxSeconds) ? _osrfCacheMaxSeconds : seconds;
	osrfLogInternal( OSRF_LOG_MARK, "osrfCachePutString(): Putting string (key=%s): %s", key, value);

	char* clean_key = _clean_key( key );

	/* add or overwrite existing key:value pair */
	rc = memcached_set(_osrfCache, clean_key, strlen(clean_key), value, strlen(value), seconds, 0);
	if (rc != MEMCACHED_SUCCESS) {
		osrfLogError(OSRF_LOG_MARK, "Failed to cache key:value [%s]:[%s] - %s",
			key, value, memcached_strerror(_osrfCache, rc));
	}

	free(clean_key);
	return 0;
}

jsonObject* osrfCacheGetObject( const char* key, ... ) {
	size_t val_len;
	uint32_t flags;
	memcached_return rc;
	jsonObject* obj = NULL;
	if( key ) {
		char* clean_key = _clean_key( key );
		const char* data = (const char*) memcached_get(_osrfCache, clean_key, strlen(clean_key), &val_len, &flags, &rc);
		free(clean_key);
		if (rc != MEMCACHED_SUCCESS) {
			osrfLogDebug(OSRF_LOG_MARK, "Failed to get key [%s] - %s",
				key, memcached_strerror(_osrfCache, rc));
		}
		if( data ) {
			osrfLogInternal( OSRF_LOG_MARK, "osrfCacheGetObject(): Returning object (key=%s): %s", key, data);
			obj = jsonParse( data );
			return obj;
		}
		osrfLogDebug(OSRF_LOG_MARK, "No cache data exists with key %s", key);
	}
	return NULL;
}

char* osrfCacheGetString( const char* key, ... ) {
	size_t val_len;
	uint32_t flags;
	memcached_return rc;
	if( key ) {
		char* clean_key = _clean_key( key );
		char* data = (char*) memcached_get(_osrfCache, clean_key, strlen(clean_key), &val_len, &flags, &rc);
		free(clean_key);
		if (rc != MEMCACHED_SUCCESS) {
			osrfLogDebug(OSRF_LOG_MARK, "Failed to get key [%s] - %s",
				key, memcached_strerror(_osrfCache, rc));
		}
		osrfLogInternal( OSRF_LOG_MARK, "osrfCacheGetString(): Returning object (key=%s): %s", key, data);
		if(!data) osrfLogDebug(OSRF_LOG_MARK, "No cache data exists with key %s", key);
		return data;
	}
	return NULL;
}


int osrfCacheRemove( const char* key, ... ) {
	memcached_return rc;
	if( key ) {
		char* clean_key = _clean_key( key );
		rc = memcached_delete(_osrfCache, clean_key, strlen(clean_key), 0 );
		free(clean_key);
		if (rc != MEMCACHED_SUCCESS && rc != MEMCACHED_BUFFERED) {
			osrfLogDebug(OSRF_LOG_MARK, "Failed to delete key [%s] - %s",
				key, memcached_strerror(_osrfCache, rc));
		}
		return 0;
	}
	return -1;
}


int osrfCacheSetExpire( time_t seconds, const char* key, ... ) {
	if( key ) {
		char* clean_key = _clean_key( key );
		jsonObject* o = osrfCacheGetObject( clean_key );
		int rc = osrfCachePutObject( clean_key, o, seconds );
		jsonObjectFree(o);
		return rc;
	}
	return -1;
}

void osrfCacheCleanup() {
	if(_osrfCache) {
		memcached_free(_osrfCache);
	}
}


