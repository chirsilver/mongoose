#include <stdio.h>
#include <iostream>
#include <sys/epoll.h>

#include "../common/mongoose.h"
using namespace std;

#define ADDR "127.0.0.1:8080"
#define MG_ENABLE_SSL 1
#define LOG(level, exp)                    \
  do                                       \
  {                                        \
    cout << level << ":\t" << exp << endl; \
  } while (0)

bool exit_f = false;
void sigint(int signo){
  LOG("INFO", "exit...");
  exit_f = true;
}

mg_mgr mgr;
mg_connection *nc;
mg_bind_opts bind_opt;
mg_serve_http_opts serve_opt;
void (*HandleEvent)(mg_connection *nc, int event, void *data);
void (*HandleHttpReq)(mg_connection *nc, http_message *hm);
void (*HandleWsMsg)(mg_connection *nc, int event, websocket_message *wm);




void HandleEvent_f(mg_connection *nc, int event, void *data);
void HandleHttpRep_f(mg_connection *nc, http_message *hm);
void HandleWsMsg_f(mg_connection *nc, int event, websocket_message *wm);
bool MatchUrl(http_message *hm, const char *prefix);

int main() {
  signal(SIGINT, sigint);

  HandleEvent = HandleEvent_f;
  HandleHttpReq = HandleHttpRep_f;
  HandleWsMsg = HandleWsMsg_f;

  mg_mgr_init(&mgr, NULL);
  nc = mg_bind(&mgr, ADDR, HandleEvent_f);
  if (NULL == nc) {
    mg_mgr_free(&mgr);
    return 1;
  }

  serve_opt.document_root = "./web";
  serve_opt.enable_directory_listing = "yes";

  mg_set_protocol_http_websocket(nc);
  while (!exit_f) {
    mg_mgr_poll(&mgr, 100);
  }
  mg_mgr_free(&mgr);

  return 0;
}

void HandleEvent_f(mg_connection *nc, int event, void *data) {
  switch (event) {
    case MG_EV_HTTP_REQUEST: {
      HandleHttpReq(nc, (http_message*)data);
      break;
    }
    case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
    case MG_EV_WEBSOCKET_FRAME: {
      HandleWsMsg(nc, event, (websocket_message*)data);
      break;
    }
    case MG_EV_CLOSE: {
      if(nc->flags & MG_F_IS_WEBSOCKET) {
        HandleWsMsg(nc, event, (websocket_message*)data);
      }
      break;
    }
    default:
      break;
  }
}
void HandleHttpRep_f(mg_connection *nc, http_message *hm) {
  if(MatchUrl(hm, "/api/hello")) {

  } else if(MatchUrl(hm, "/api/sum")) {

  } else {
    mg_serve_http(nc, hm, serve_opt);
  }
}
void HandleWsMsg_f(mg_connection *nc, int event, websocket_message *wm);
bool MatchUrl(http_message *hm, const char *prefix) {
  if(mg_vcmp(&hm->uri, prefix) == 0) {
    return true;
  }
  return false;
}