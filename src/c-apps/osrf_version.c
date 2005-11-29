#include "opensrf/osrf_app_session.h"
#include "opensrf/osrf_application.h"
#include "objson/object.h"
#include "opensrf/utils.h"
#include "opensrf/log.h"

#define OSRF_VERSION_CACHE_TIME 300

int osrfAppInitialize();
int osrfAppChildInit();
int osrfVersion( osrfMethodContext* );


int osrfAppInitialize() {

	osrfAppRegisterMethod( 
			"opensrf.version", 
			"opensrf.version.verify", 
			"osrfVersion", 
			"The data for a service/method/params combination will be retrieved "
			"from the necessary server and the MD5 sum of the total values received "
			"will be returned. PARAMS( serviceName, methodName, [param1, ...] )", 
			2, 0 );
	
	return 0;
}

int osrfAppChildInit() {
	return 0;
}

int osrfVersion( osrfMethodContext* ctx ) {

	OSRF_METHOD_VERIFY_CONTEXT(ctx); 

	/* First, see if the data is in the cache */
	char* json = jsonObjectToJSON(ctx->params);
	char* paramsmd5 = md5sum(json);
	char* cachedmd5 = osrfCacheGetString(paramsmd5);
	free(json); 

	if( cachedmd5 ) {
		osrfLogDebug( "Found %s object in cache, returning....", cachedmd5 );
		jsonObject* resp = jsonNewObject(cachedmd5);
		osrfAppRespondComplete( ctx, resp  );
		jsonObjectFree(resp);
		free(paramsmd5);
		free(cachedmd5);
		return 0;
	}

	jsonObject* serv = jsonObjectGetIndex(ctx->params, 0);
	jsonObject* meth = jsonObjectGetIndex(ctx->params, 1);
	char* service = jsonObjectGetString(serv);
	char* methd = jsonObjectGetString(meth);

	if( service && methd ) {
		/* shove the additional params into an array */
		jsonObject* tmpArray = jsonNewObject(NULL);
		int i;
		for( i = 2; i != ctx->params->size; i++ ) 
			jsonObjectPush( tmpArray, jsonObjectClone(jsonObjectGetIndex(ctx->params, i)));

		osrfAppSession* ses = osrfAppSessionClientInit(service);
		int reqid = osrfAppSessionMakeRequest( ses, tmpArray, methd, 1, NULL );
		osrfMessage* omsg = osrfAppSessionRequestRecv( ses, reqid, 60 );
		jsonObjectFree(tmpArray);

		if( omsg ) {

			jsonObject* result = osrfMessageGetResult( omsg );
			char* resultjson = jsonObjectToJSON(result);
			char* resultmd5 = md5sum(resultjson);
			free(resultjson);
			osrfMessageFree(omsg);

			if( resultmd5 ) {
				jsonObject* resp = jsonNewObject(resultmd5);
				osrfAppRespondComplete( ctx, resp );
				jsonObjectFree(resp);
				osrfAppSessionFree(ses);
				osrfLogDebug("Found version string %s, caching and returning...", resultmd5 );
				osrfCachePutString( paramsmd5, resultmd5, OSRF_VERSION_CACHE_TIME );
				free(resultmd5);
				free(paramsmd5);
				return 0;
			} 
		}
		osrfAppSessionFree(ses);
	}

	free(paramsmd5);

	return -1;
}



