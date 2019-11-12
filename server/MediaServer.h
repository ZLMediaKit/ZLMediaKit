#ifndef Server_MediaServer_H
#define Server_MediaServer_H


//## package Server

enum
{

	MediaServerStartingUpState = 0,
	MediaServerRunningState = 1,
	MediaServerRefusingConnectionsState = 2,
	MediaServerFatalErrorState = 3,// a fatal error has occurred, not shutting down yet
	MediaServerShuttingDownState = 4,
	MediaServerIdleState = 5 // Like refusing connections state, but will also kill any currently connected clients
};
typedef int MediaServer_ServerState;

#endif

