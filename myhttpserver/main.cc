#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <bits/stdc++.h>
#include "../common/mongoose.h"
using namespace std;

void EventHandler(mg_connection *nc, int event, void *event_data);
void Sigint(int signo);
bool CheckPrefix(http_message *hm, const char *prefix);
void HandleHttpMessage(mg_connection *nc, http_message *hm);
void HandleWebsocketMessage(mg_connection *nc, int event, websocket_message *wm);
void SendHttpRsp(mg_connection *nc, const string rsp);
void SendWebsocketMsg(mg_connection *nc, const string msg);

#define PORT "8080"
bool g_quit = false;

mg_serve_http_opts opt;
unordered_set<mg_connection*> session;

int main() {
  signal(SIGINT, Sigint);

  opt.document_root = "./web";
  opt.enable_directory_listing = "yes";

  mg_mgr mgr;
  mg_mgr_init(&mgr, NULL);
  mg_connection *nc = mg_bind(&mgr, PORT, EventHandler);
  if(NULL == nc) {
    mg_mgr_free(&mgr);
    perror("error: bind.");
  }
  printf("bind on 8080 OK!\n");

  mg_set_protocol_http_websocket(nc);
  while(!g_quit) {
    mg_mgr_poll(&mgr, 500);
  }

  mg_mgr_free(&mgr);
  return 0;
}

extern "C" {
  #include "../common/mongoose.c"
}

void SendHttpRsp(mg_connection *nc, const string rsp) {
  mg_send_head(nc, 200, -1, NULL);
  mg_printf_http_chunk(nc, "{ \"result\": \"%s\" }", rsp.c_str());
  mg_send_http_chunk(nc, "", 0);
}

void HandleHttpMessage(mg_connection *nc, http_message *hm) {
  string method = string(hm->method.p, hm->method.len);
  string url = string(hm->uri.p, hm->uri.len);
  string parm = string(hm->query_string.p, hm->query_string.len);
  string body = string(hm->body.p, hm->body.len);
  string message = string(hm->message.p, hm->message.len);
  printf("a http request with: \n"
         "\033[0;32mmethod\033[0m: %s\n"
         "\033[0;32murl\033[0m: %s;\n"
         "\033[0;32mparm:\033[0m %s;\n"
         "\033[0;32mbody\033[0m: %s;\n",
         "\033[0;32mmessage\033[0m: %s;\n\n",
         method.c_str(), url.c_str(), parm.c_str(), body.c_str(), message.c_str());
  if(CheckPrefix(hm, "/")) {
    mg_serve_http(nc, hm, opt);
  } else if(CheckPrefix(hm, "/api/get")) {
    char html_data[1024];

    mg_get_http_var(&hm->query_string, "data", html_data, sizeof(html_data));

    SendHttpRsp(nc, "callback your msg: " + string(html_data));
  } else if(CheckPrefix(hm, "/api/post")) {

		char n1[100], n2[100];
		double result;

		mg_get_http_var(&hm->body, "a", n1, sizeof(n1));
		mg_get_http_var(&hm->body, "b", n2, sizeof(n2));

		result = strtod(n1, NULL) + strtod(n2, NULL);
		SendHttpRsp(nc, to_string(result));
  }
}

void Sigint(int signo) {
  printf("-------------------------exit-------------------------\n");
  g_quit = true;
}

void EventHandler(mg_connection *nc, int event, void *event_data) {
  switch(event) {
    case MG_EV_HTTP_REQUEST: {
      http_message *hm = (http_message *)event_data;
      HandleHttpMessage(nc, hm);
      break;
    }
    default: {
      break;
    }
  }
}

bool CheckPrefix(http_message *hm, const char *prefix) {
  if(mg_vcmp(&hm->uri, prefix) == 0) {
    return true;
  } else {
    return false;
  }
}