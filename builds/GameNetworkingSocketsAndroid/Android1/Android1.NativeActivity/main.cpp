/*
* Copyright (C) 2010 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/

#include <malloc.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "LOG_TESTAPP", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "LOG_TESTAPP", __VA_ARGS__))

#include "test_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <string>
#include <random>
#include <chrono>
#include <thread>


#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include "../examples/trivial_signaling_client.h"

HSteamListenSocket g_hListenSock;
HSteamNetConnection g_hConnection;
enum ETestRole
{
	k_ETestRole_Undefined,
	k_ETestRole_Server,
	k_ETestRole_Client,
	k_ETestRole_Symmetric,
};
ETestRole g_eTestRole = k_ETestRole_Undefined;

int g_nVirtualPortLocal = 0; // Used when listening, and when connecting
int g_nVirtualPortRemote = 0; // Only used when connecting

void Quit(int rc)
{
	if (rc == 0)
	{
		// OK, we cannot just exit the process, because we need to give
		// the connection time to actually send the last message and clean up.
		// If this were a TCP connection, we could just bail, because the OS
		// would handle it.  But this is an application protocol over UDP.
		// So give a little bit of time for good cleanup.  (Also note that
		// we really ought to continue pumping the signaling service, but
		// in this exampple we'll assume that no more signals need to be
		// exchanged, since we've gotten this far.)  If we just terminated
		// the program here, our peer could very likely timeout.  (Although
		// it's possible that the cleanup packets have already been placed
		// on the wire, and if they don't drop, things will get cleaned up
		// properly.)
		LOGI("Waiting for any last cleanup packets.\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
	GameNetworkingSockets_Kill();
#else
	SteamDatagramClient_Kill();
#endif
	exit(rc);
}

// Send a simple string message to out peer, using reliable transport.
void SendMessageToPeer(const char* pszMsg)
{
	LOGI("Sending msg '%s'\n", pszMsg);
	EResult r = SteamNetworkingSockets()->SendMessageToConnection(
		g_hConnection, pszMsg, (int)strlen(pszMsg) + 1, k_nSteamNetworkingSend_Reliable, nullptr);
	assert(r == k_EResultOK);
}

// Called when a connection undergoes a state transition.
void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo)
{
	// What's the state of the connection?
	switch (pInfo->m_info.m_eState)
	{
	case k_ESteamNetworkingConnectionState_ClosedByPeer:
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:

		LOGI("[%s] %s, reason %d: %s\n",
			pInfo->m_info.m_szConnectionDescription,
			(pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer ? "closed by peer" : "problem detected locally"),
			pInfo->m_info.m_eEndReason,
			pInfo->m_info.m_szEndDebug
		);

		// Close our end
		SteamNetworkingSockets()->CloseConnection(pInfo->m_hConn, 0, nullptr, false);

		if (g_hConnection == pInfo->m_hConn)
		{
			g_hConnection = k_HSteamNetConnection_Invalid;

			// In this example, we will bail the test whenever this happens.
			// Was this a normal termination?
			int rc = 0;
			if (rc == k_ESteamNetworkingConnectionState_ProblemDetectedLocally || pInfo->m_info.m_eEndReason != k_ESteamNetConnectionEnd_App_Generic)
				rc = 1; // failure
			Quit(rc);
		}
		else
		{
			// Why are we hearing about any another connection?
			assert(false);
		}

		break;

	case k_ESteamNetworkingConnectionState_None:
		// Notification that a connection was destroyed.  (By us, presumably.)
		// We don't need this, so ignore it.
		break;

	case k_ESteamNetworkingConnectionState_Connecting:

		// Is this a connection we initiated, or one that we are receiving?
		if (g_hListenSock != k_HSteamListenSocket_Invalid && pInfo->m_info.m_hListenSocket == g_hListenSock)
		{
			// Somebody's knocking
			// Note that we assume we will only ever receive a single connection
			assert(g_hConnection == k_HSteamNetConnection_Invalid); // not really a bug in this code, but a bug in the test

			LOGI("[%s] Accepting\n", pInfo->m_info.m_szConnectionDescription);
			g_hConnection = pInfo->m_hConn;
			SteamNetworkingSockets()->AcceptConnection(pInfo->m_hConn);
		}
		else
		{
			// Note that we will get notification when our own connection that
			// we initiate enters this state.
			assert(g_hConnection == pInfo->m_hConn);
			LOGI("[%s] Entered connecting state\n", pInfo->m_info.m_szConnectionDescription);
		}
		break;

	case k_ESteamNetworkingConnectionState_FindingRoute:
		// P2P connections will spend a bried time here where they swap addresses
		// and try to find a route.
		LOGI("[%s] finding route\n", pInfo->m_info.m_szConnectionDescription);
		break;

	case k_ESteamNetworkingConnectionState_Connected:
		// We got fully connected
		assert(pInfo->m_hConn == g_hConnection); // We don't initiate or accept any other connections, so this should be out own connection
		LOGI("[%s] connected\n", pInfo->m_info.m_szConnectionDescription);
		break;

	default:
		assert(false);
		break;
	}
}


ITrivialSignalingClient* pSignaling;

void ConnectP2PClient()
{
	SteamNetworkingIdentity identityLocal; identityLocal.Clear();
	SteamNetworkingIdentity identityRemote; identityRemote.Clear();
	const char* pszTrivialSignalingService = "167.99.33.20:10000";

	
	identityRemote.ParseString("str:unalserver");
	identityLocal.ParseString("str:levent");
	g_eTestRole = k_ETestRole_Client;


	// Initialize library, with the desired local identity
	TEST_Init(&identityLocal);

	// Hardcode STUN servers
	SteamNetworkingUtils()->SetGlobalConfigValueString(k_ESteamNetworkingConfig_P2P_STUN_ServerList, "stun.l.google.com:19302");

	// Allow sharing of any kind of ICE address.
	// We don't have any method of relaying (TURN) in this example, so we are essentially
	// forced to disclose our public address if we want to pierce NAT.  But if we
	// had relay fallback, or if we only wanted to connect on the LAN, we could restrict
	// to only sharing private addresses.
	SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_P2P_Transport_ICE_Enable, k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_All);

	// Create the signaling service
	SteamNetworkingErrMsg errMsg;
	pSignaling = CreateTrivialSignalingClient(pszTrivialSignalingService, SteamNetworkingSockets(), errMsg);
	if (pSignaling == nullptr)
		TEST_Fatal("Failed to initializing signaling client.  %s", errMsg);

	SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(OnSteamNetConnectionStatusChanged);

	// Comment this line in for more detailed spew about signals, route finding, ICE, etc
	//SteamNetworkingUtils()->SetGlobalConfigValueInt32( k_ESteamNetworkingConfig_LogLevel_P2PRendezvous, k_ESteamNetworkingSocketsDebugOutputType_Verbose );

	// Create listen socket to receive connections on, unless we are the client
	if (g_eTestRole == k_ETestRole_Server)
	{
		TEST_Printf("Creating listen socket, local virtual port %d\n", g_nVirtualPortLocal);
		g_hListenSock = SteamNetworkingSockets()->CreateListenSocketP2P(g_nVirtualPortLocal, 0, nullptr);
		assert(g_hListenSock != k_HSteamListenSocket_Invalid);
	}
	else if (g_eTestRole == k_ETestRole_Symmetric)
	{

		// Currently you must create a listen socket to use symmetric mode,
		// even if you know that you will always create connections "both ways".
		// In the future we might try to remove this requirement.  It is a bit
		// less efficient, since it always triggered the race condition case
		// where both sides create their own connections, and then one side
		// decides to their theirs away.  If we have a listen socket, then
		// it can be the case that one peer will receive the incoming connection
		// from the other peer, and since he has a listen socket, can save
		// the connection, and then implicitly accept it when he initiates his
		// own connection.  Without the listen socket, if an incoming connection
		// request arrives before we have started connecting out, then we are forced
		// to ignore it, as the app has given no indication that it desires to
		// receive inbound connections at all.
		TEST_Printf("Creating listen socket in symmetric mode, local virtual port %d\n", g_nVirtualPortLocal);
		SteamNetworkingConfigValue_t opt;
		opt.SetInt32(k_ESteamNetworkingConfig_SymmetricConnect, 1); // << Note we set symmetric mode on the listen socket
		g_hListenSock = SteamNetworkingSockets()->CreateListenSocketP2P(g_nVirtualPortLocal, 1, &opt);
		assert(g_hListenSock != k_HSteamListenSocket_Invalid);
	}

	// Begin connecting to peer, unless we are the server
	if (g_eTestRole != k_ETestRole_Server)
	{
		std::vector< SteamNetworkingConfigValue_t > vecOpts;

		// If we want the local and virtual port to differ, we must set
		// an option.  This is a pretty rare use case, and usually not needed.
		// The local virtual port is only usually relevant for symmetric
		// connections, and then, it almost always matches.  Here we are
		// just showing in this example code how you could handle this if you
		// needed them to differ.
		if (g_nVirtualPortRemote != g_nVirtualPortLocal)
		{
			SteamNetworkingConfigValue_t opt;
			opt.SetInt32(k_ESteamNetworkingConfig_LocalVirtualPort, g_nVirtualPortLocal);
			vecOpts.push_back(opt);
		}

		// Symmetric mode?  Noce that since we created a listen socket on this local
		// virtual port and tagged it for symmetric connect mode, any connections
		// we create that use the same local virtual port will automatically inherit
		// this setting.  However, this is really not recommended.  It is best to be
		// explicit.
		if (g_eTestRole == k_ETestRole_Symmetric)
		{
			SteamNetworkingConfigValue_t opt;
			opt.SetInt32(k_ESteamNetworkingConfig_SymmetricConnect, 1);
			vecOpts.push_back(opt);
			TEST_Printf("Connecting to '%s' in symmetric mode, virtual port %d, from local virtual port %d.\n",
				SteamNetworkingIdentityRender(identityRemote).c_str(), g_nVirtualPortRemote,
				g_nVirtualPortLocal);
		}
		else
		{
			TEST_Printf("Connecting to '%s', virtual port %d, from local virtual port %d.\n",
				SteamNetworkingIdentityRender(identityRemote).c_str(), g_nVirtualPortRemote,
				g_nVirtualPortLocal);
		}

		// Connect using the "custom signaling" path.  Note that when
		// you are using this path, the identity is actually optional,
		// since we don't need it.  (Your signaling object already
		// knows how to talk to the peer) and then the peer identity
		// will be confirmed via rendezvous.
		SteamNetworkingErrMsg errMsg;
		ISteamNetworkingConnectionSignaling* pConnSignaling = pSignaling->CreateSignalingForConnection(
			identityRemote,
			errMsg
		);
		assert(pConnSignaling);
		g_hConnection = SteamNetworkingSockets()->ConnectP2PCustomSignaling(pConnSignaling, &identityRemote, g_nVirtualPortRemote, (int)vecOpts.size(), vecOpts.data());
		assert(g_hConnection != k_HSteamNetConnection_Invalid);

		// Go ahead and send a message now.  The message will be queued until route finding
		// completes.
		SendMessageToPeer("Greetings!");
	}
}


void ConnectP2Server()
{
	SteamNetworkingIdentity identityLocal; identityLocal.Clear();
	SteamNetworkingIdentity identityRemote; identityRemote.Clear();
	const char* pszTrivialSignalingService = "167.99.33.20:10000";

	identityLocal.ParseString("str:unalserver");
	g_eTestRole = k_ETestRole_Server;


	// Initialize library, with the desired local identity
	TEST_Init(&identityLocal);

	// Hardcode STUN servers
	SteamNetworkingUtils()->SetGlobalConfigValueString(k_ESteamNetworkingConfig_P2P_STUN_ServerList, "stun.l.google.com:19302");

	// Allow sharing of any kind of ICE address.
	// We don't have any method of relaying (TURN) in this example, so we are essentially
	// forced to disclose our public address if we want to pierce NAT.  But if we
	// had relay fallback, or if we only wanted to connect on the LAN, we could restrict
	// to only sharing private addresses.
	SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_P2P_Transport_ICE_Enable, k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_All);

	// Create the signaling service
	SteamNetworkingErrMsg errMsg;
	pSignaling = CreateTrivialSignalingClient(pszTrivialSignalingService, SteamNetworkingSockets(), errMsg);
	if (pSignaling == nullptr)
		TEST_Fatal("Failed to initializing signaling client.  %s", errMsg);

	SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(OnSteamNetConnectionStatusChanged);

	// Comment this line in for more detailed spew about signals, route finding, ICE, etc
	//SteamNetworkingUtils()->SetGlobalConfigValueInt32( k_ESteamNetworkingConfig_LogLevel_P2PRendezvous, k_ESteamNetworkingSocketsDebugOutputType_Verbose );

	// Create listen socket to receive connections on, unless we are the client
	if (g_eTestRole == k_ETestRole_Server)
	{
		TEST_Printf("Creating listen socket, local virtual port %d\n", g_nVirtualPortLocal);
		g_hListenSock = SteamNetworkingSockets()->CreateListenSocketP2P(g_nVirtualPortLocal, 0, nullptr);
		assert(g_hListenSock != k_HSteamListenSocket_Invalid);
	}
	else if (g_eTestRole == k_ETestRole_Symmetric)
	{

		// Currently you must create a listen socket to use symmetric mode,
		// even if you know that you will always create connections "both ways".
		// In the future we might try to remove this requirement.  It is a bit
		// less efficient, since it always triggered the race condition case
		// where both sides create their own connections, and then one side
		// decides to their theirs away.  If we have a listen socket, then
		// it can be the case that one peer will receive the incoming connection
		// from the other peer, and since he has a listen socket, can save
		// the connection, and then implicitly accept it when he initiates his
		// own connection.  Without the listen socket, if an incoming connection
		// request arrives before we have started connecting out, then we are forced
		// to ignore it, as the app has given no indication that it desires to
		// receive inbound connections at all.
		TEST_Printf("Creating listen socket in symmetric mode, local virtual port %d\n", g_nVirtualPortLocal);
		SteamNetworkingConfigValue_t opt;
		opt.SetInt32(k_ESteamNetworkingConfig_SymmetricConnect, 1); // << Note we set symmetric mode on the listen socket
		g_hListenSock = SteamNetworkingSockets()->CreateListenSocketP2P(g_nVirtualPortLocal, 1, &opt);
		assert(g_hListenSock != k_HSteamListenSocket_Invalid);
	}

	// Begin connecting to peer, unless we are the server
	if (g_eTestRole != k_ETestRole_Server)
	{
		std::vector< SteamNetworkingConfigValue_t > vecOpts;

		// If we want the local and virtual port to differ, we must set
		// an option.  This is a pretty rare use case, and usually not needed.
		// The local virtual port is only usually relevant for symmetric
		// connections, and then, it almost always matches.  Here we are
		// just showing in this example code how you could handle this if you
		// needed them to differ.
		if (g_nVirtualPortRemote != g_nVirtualPortLocal)
		{
			SteamNetworkingConfigValue_t opt;
			opt.SetInt32(k_ESteamNetworkingConfig_LocalVirtualPort, g_nVirtualPortLocal);
			vecOpts.push_back(opt);
		}

		// Symmetric mode?  Noce that since we created a listen socket on this local
		// virtual port and tagged it for symmetric connect mode, any connections
		// we create that use the same local virtual port will automatically inherit
		// this setting.  However, this is really not recommended.  It is best to be
		// explicit.
		if (g_eTestRole == k_ETestRole_Symmetric)
		{
			SteamNetworkingConfigValue_t opt;
			opt.SetInt32(k_ESteamNetworkingConfig_SymmetricConnect, 1);
			vecOpts.push_back(opt);
			TEST_Printf("Connecting to '%s' in symmetric mode, virtual port %d, from local virtual port %d.\n",
				SteamNetworkingIdentityRender(identityRemote).c_str(), g_nVirtualPortRemote,
				g_nVirtualPortLocal);
		}
		else
		{
			TEST_Printf("Connecting to '%s', virtual port %d, from local virtual port %d.\n",
				SteamNetworkingIdentityRender(identityRemote).c_str(), g_nVirtualPortRemote,
				g_nVirtualPortLocal);
		}

		// Connect using the "custom signaling" path.  Note that when
		// you are using this path, the identity is actually optional,
		// since we don't need it.  (Your signaling object already
		// knows how to talk to the peer) and then the peer identity
		// will be confirmed via rendezvous.
		SteamNetworkingErrMsg errMsg;
		ISteamNetworkingConnectionSignaling* pConnSignaling = pSignaling->CreateSignalingForConnection(
			identityRemote,
			errMsg
		);
		assert(pConnSignaling);
		g_hConnection = SteamNetworkingSockets()->ConnectP2PCustomSignaling(pConnSignaling, &identityRemote, g_nVirtualPortRemote, (int)vecOpts.size(), vecOpts.data());
		assert(g_hConnection != k_HSteamNetConnection_Invalid);

		// Go ahead and send a message now.  The message will be queued until route finding
		// completes.
		SendMessageToPeer("Greetings!");
	}
}

void P2PLoop()
{
	// Check for incoming signals, and dispatch them
	pSignaling->Poll();

	// Check callbacks
	TEST_PumpCallbacks();

	// If we have a connection, then poll it for messages
	if (g_hConnection != k_HSteamNetConnection_Invalid)
	{
		SteamNetworkingMessage_t* pMessage;
		int r = SteamNetworkingSockets()->ReceiveMessagesOnConnection(g_hConnection, &pMessage, 1);
		assert(r == 0 || r == 1); // <0 indicates an error
		if (r == 1)
		{
			// In this example code we will assume all messages are '\0'-terminated strings.
			// Obviously, this is not secure.
			TEST_Printf("Received message '%s'\n", pMessage->GetData());

			// Free message struct and buffer.
			pMessage->Release();

			// If we're the client, go ahead and shut down.  In this example we just
			// wanted to establish a connection and exchange a message, and we've done that.
			// Note that we use "linger" functionality.  This flushes out any remaining
			// messages that we have queued.  Essentially to us, the connection is closed,
			// but on thew wire, we will not actually close it until all reliable messages
			// have been confirmed as received by the client.  (Or the connection is closed
			// by the peer or drops.)  If we are the "client" role, then we know that no such
			// messages are in the pipeline in this test.  But in symmetric mode, it is
			// possible that we need to flush out our message that we sent.
			if (g_eTestRole != k_ETestRole_Server)
			{
				TEST_Printf("Closing connection and shutting down.\n");
				SteamNetworkingSockets()->CloseConnection(g_hConnection, 0, "Test completed OK", true);
				Quit(0);
			}

			// We're the server.  Send a reply.
			SendMessageToPeer("I got your message");
		}
	}
}

/**
* Our saved state data.
*/
struct saved_state {
	float angle;
	int32_t x;
	int32_t y;
};

/**
* Shared state for our app.
*/
struct engine {
	struct android_app* app;

	ASensorManager* sensorManager;
	const ASensor* accelerometerSensor;
	ASensorEventQueue* sensorEventQueue;

	int animating;
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	int32_t width;
	int32_t height;
	struct saved_state state;
};

/**
* Initialize an EGL context for the current display.
*/
static int engine_init_display(struct engine* engine) {
	// initialize OpenGL ES and EGL

	/*
	* Here specify the attributes of the desired configuration.
	* Below, we select an EGLConfig with at least 8 bits per color
	* component compatible with on-screen windows
	*/
	const EGLint attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_NONE
	};
	EGLint w, h, format;
	EGLint numConfigs;
	EGLConfig config;
	EGLSurface surface;
	EGLContext context;

	EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	eglInitialize(display, 0, 0);

	/* Here, the application chooses the configuration it desires. In this
	* sample, we have a very simplified selection process, where we pick
	* the first EGLConfig that matches our criteria */
	eglChooseConfig(display, attribs, &config, 1, &numConfigs);

	/* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
	* guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
	* As soon as we picked a EGLConfig, we can safely reconfigure the
	* ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
	eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

	ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);

	surface = eglCreateWindowSurface(display, config, engine->app->window, NULL);
	context = eglCreateContext(display, config, NULL, NULL);

	if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
		LOGW("Unable to eglMakeCurrent");
		return -1;
	}

	eglQuerySurface(display, surface, EGL_WIDTH, &w);
	eglQuerySurface(display, surface, EGL_HEIGHT, &h);

	engine->display = display;
	engine->context = context;
	engine->surface = surface;
	engine->width = w;
	engine->height = h;
	engine->state.angle = 0;

	// Initialize GL state.
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	glEnable(GL_CULL_FACE);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_DEPTH_TEST);

	return 0;
}

/**
* Just the current frame in the display.
*/
static void engine_draw_frame(struct engine* engine) {
	if (engine->display == NULL) {
		// No display.
		return;
	}

	// Just fill the screen with a color.
	glClearColor(((float)engine->state.x) / engine->width, engine->state.angle,
		((float)engine->state.y) / engine->height, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	eglSwapBuffers(engine->display, engine->surface);
}

/**
* Tear down the EGL context currently associated with the display.
*/
static void engine_term_display(struct engine* engine) {
	if (engine->display != EGL_NO_DISPLAY) {
		eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (engine->context != EGL_NO_CONTEXT) {
			eglDestroyContext(engine->display, engine->context);
		}
		if (engine->surface != EGL_NO_SURFACE) {
			eglDestroySurface(engine->display, engine->surface);
		}
		eglTerminate(engine->display);
	}
	engine->animating = 0;
	engine->display = EGL_NO_DISPLAY;
	engine->context = EGL_NO_CONTEXT;
	engine->surface = EGL_NO_SURFACE;
}

/**
* Process the next input event.
*/
static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
	struct engine* engine = (struct engine*)app->userData;
	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
		engine->state.x = AMotionEvent_getX(event, 0);
		engine->state.y = AMotionEvent_getY(event, 0);
		return 1;
	}
	return 0;
}

/**
* Process the next main command.
*/
static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
	struct engine* engine = (struct engine*)app->userData;
	switch (cmd) {
	case APP_CMD_SAVE_STATE:
		// The system has asked us to save our current state.  Do so.
		engine->app->savedState = malloc(sizeof(struct saved_state));
		*((struct saved_state*)engine->app->savedState) = engine->state;
		engine->app->savedStateSize = sizeof(struct saved_state);
		break;
	case APP_CMD_INIT_WINDOW:
		// The window is being shown, get it ready.
		if (engine->app->window != NULL) {
			engine_init_display(engine);
			engine_draw_frame(engine);
		}
		break;
	case APP_CMD_TERM_WINDOW:
		// The window is being hidden or closed, clean it up.
		engine_term_display(engine);
		break;
	case APP_CMD_GAINED_FOCUS:
		// When our app gains focus, we start monitoring the accelerometer.
		if (engine->accelerometerSensor != NULL) {
			ASensorEventQueue_enableSensor(engine->sensorEventQueue,
				engine->accelerometerSensor);
			// We'd like to get 60 events per second (in microseconds).
			ASensorEventQueue_setEventRate(engine->sensorEventQueue,
				engine->accelerometerSensor, (1000L / 60) * 1000);
		}
		break;
	case APP_CMD_LOST_FOCUS:
		// When our app loses focus, we stop monitoring the accelerometer.
		// This is to avoid consuming battery while not being used.
		if (engine->accelerometerSensor != NULL) {
			ASensorEventQueue_disableSensor(engine->sensorEventQueue,
				engine->accelerometerSensor);
		}
		// Also stop animating.
		engine->animating = 0;
		engine_draw_frame(engine);
		break;
	}
}

/**
* This is the main entry point of a native application that is using
* android_native_app_glue.  It runs in its own thread, with its own
* event loop for receiving input events and doing other things.
*/
void android_main(struct android_app* state) {
	struct engine engine;

	memset(&engine, 0, sizeof(engine));
	state->userData = &engine;
	state->onAppCmd = engine_handle_cmd;
	state->onInputEvent = engine_handle_input;
	engine.app = state;

	// Prepare to monitor accelerometer
	engine.sensorManager = ASensorManager_getInstance();
	engine.accelerometerSensor = ASensorManager_getDefaultSensor(engine.sensorManager,
		ASENSOR_TYPE_ACCELEROMETER);
	engine.sensorEventQueue = ASensorManager_createEventQueue(engine.sensorManager,
		state->looper, LOOPER_ID_USER, NULL, NULL);

	if (state->savedState != NULL) {
		// We are starting with a previous saved state; restore from it.
		engine.state = *(struct saved_state*)state->savedState;
	}

	engine.animating = 1;

	// loop waiting for stuff to do.

	ConnectP2Server();


	while (1) {
		// Read all pending events.
		int ident;
		int events;
		struct android_poll_source* source;

		// If not animating, we will block forever waiting for events.
		// If animating, we loop until all events are read, then continue
		// to draw the next frame of animation.
		while ((ident = ALooper_pollAll(engine.animating ? 0 : -1, NULL, &events,
			(void**)&source)) >= 0) {

			// Process this event.
			if (source != NULL) {
				source->process(state, source);
			}

			// If a sensor has data, process it now.
			if (ident == LOOPER_ID_USER) {
				if (engine.accelerometerSensor != NULL) {
					ASensorEvent event;
					while (ASensorEventQueue_getEvents(engine.sensorEventQueue,
						&event, 1) > 0) {
						 
					}
				}
			}

			// Check if we are exiting.
			if (state->destroyRequested != 0) {
				engine_term_display(&engine);
				return;
			}
		}

		if (engine.animating) {
			// Done with events; draw next animation frame.
			engine.state.angle += .01f;
			if (engine.state.angle > 1) {
				engine.state.angle = 0;
			}

			// Drawing is throttled to the screen update rate, so there
			// is no need to do timing here.
			engine_draw_frame(&engine);
		}

		P2PLoop();
	}
}
