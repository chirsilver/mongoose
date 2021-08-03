#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <bits/stdc++.h>

// Socket Headers
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "../common/mongoose.h"
using namespace std;

void EventHandler(mg_connection *nc, int event, void *event_data);
void Sigint(int signo);
bool CheckPrefix(http_message *hm, const char *prefix);
void HandleHttpMessage(mg_connection *nc, http_message *hm);
void HandleWebsocketMessage(mg_connection *nc, int event, websocket_message *wm);
void SendHttpRsp(mg_connection *nc, const string rsp);
void SendWebsocketMsg(mg_connection *nc, const string msg);
string ArrToJson(int arr[], int size);
void BroadcastWebsocketMsg(const string msg);

unordered_set<mg_connection*> clients;

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

  int epfd = epoll_create(5);
  epoll_event ev, evs[5];
  ev.data.fd = 0;
  ev.events = EPOLLIN;
  epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &ev);

  mg_set_protocol_http_websocket(nc);
  while(!g_quit) {
    mg_mgr_poll(&mgr, 500);
    int ret = epoll_wait(epfd, evs, 5, 100);
    if(ret > 0) {
      string msg;
      getline(cin, msg);
      BroadcastWebsocketMsg(msg);
    }
  }

  mg_mgr_free(&mgr);
  return 0;
}

extern "C" {
  #include "../common/mongoose.c"
}

void BroadcastWebsocketMsg(const string msg) {
  for(auto client: clients) {
    SendWebsocketMsg(client, "[Server Broadcast] " + msg);
  }
}

void SendHttpRsp(mg_connection *nc, const string rsp) {
  mg_send_head(nc, 200, -1, NULL);
  mg_printf_http_chunk(nc, "{ \"result\": %s }", rsp.c_str());
  mg_send_http_chunk(nc, "", 0);
}

void SendWebsocketMsg(mg_connection *nc, const string msg) {
  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, msg.c_str(), strlen(msg.c_str()));
}

void HandleWebsocketMessage(mg_connection *nc, int event, websocket_message *wm) {
  if(event == MG_EV_WEBSOCKET_HANDSHAKE_DONE) {
    clients.insert(nc);
    printf("[Server] client[%s:%d] connected.\n", inet_ntoa(nc->sa.sin.sin_addr), ntohs(nc->sa.sin.sin_port));
    SendWebsocketMsg(nc, "[Server] client connected.");
  } else if(event == MG_EV_WEBSOCKET_FRAME) {
    mg_str received_msg = { (char *)wm->data, wm->size };

		char buff[1024] = {0};
		strncpy(buff, received_msg.p, received_msg.len);
		printf("[Server] received msg from [%s:%d]: %s\n", inet_ntoa(nc->sa.sin.sin_addr), ntohs(nc->sa.sin.sin_port), buff);
		SendWebsocketMsg(nc, "[Server] msg back: " + std::string(buff));
  } else if(event == MG_EV_CLOSE) {
    if(nc->flags & MG_F_IS_WEBSOCKET) {
      if(clients.find(nc) != clients.end()) {
        printf("[Server] client[%s:%d] disconnected.\n", inet_ntoa(nc->sa.sin.sin_addr), ntohs(nc->sa.sin.sin_port));
        clients.erase(nc);
      }
    }
  }
}

void HandleHttpMessage(mg_connection *nc, http_message *hm) {
  string message = string(hm->message.p, hm->message.len);
  printf("a http request with: \n"
         "\033[0;32mmessage\033[0m: %s.\n",
         message.c_str());
  if(CheckPrefix(hm, "/")) {
    mg_serve_http(nc, hm, opt);
  } else if(CheckPrefix(hm, "/api/get")) {
    char html_data[1024];

    mg_get_http_var(&hm->query_string, "msg", html_data, sizeof(html_data));

    SendHttpRsp(nc, "\"[ServerCallBack] " + string(html_data) + "\"");
  } else if(CheckPrefix(hm, "/api/post")) {

		char n1[100], n2[100];
		double result;

		mg_get_http_var(&hm->body, "a", n1, sizeof(n1));
		mg_get_http_var(&hm->body, "b", n2, sizeof(n2));

		result = strtod(n1, NULL) + strtod(n2, NULL);
		SendHttpRsp(nc, "\"" + to_string(result) + "\"");
  } else if(CheckPrefix(hm, "/api/postarr")) {

		char a[100];

    mg_get_http_var(&hm->body, "num", a, sizeof(a));

		int num = strtol(a, NULL, 10);
    int arr[5];
    for(int i = 0; i < 5; i++) {
      arr[i] = num * (i + 1);
    }
		SendHttpRsp(nc, ArrToJson(arr, 5));
  }
}

string ArrToJson(int arr[], int size) {
  string res = "[";
  for(int i = 0; i < size; i++) {
    if(i == size - 1) res += to_string(arr[i]) + ']';
    else res += to_string(arr[i]) + ',';
  }
  return res;
}

void Sigint(int signo) {
  printf("-------------------------exit-------------------------\n");
  g_quit = true;
}

void EventHandler(mg_connection *nc, int event, void *event_data) {
  if(event == MG_EV_HTTP_REQUEST) {
    http_message *hm = (http_message *)event_data;
    HandleHttpMessage(nc, hm);
  } else if(event == MG_EV_WEBSOCKET_HANDSHAKE_REQUEST) {
    http_message *hm = (http_message *)event_data;
    HandleHttpMessage(nc, hm);
  } else if(event == MG_EV_WEBSOCKET_HANDSHAKE_DONE) {
    HandleWebsocketMessage(nc, event, NULL);
  } else if(event == MG_EV_WEBSOCKET_FRAME) {
    websocket_message *wm = (websocket_message *)event_data;
    HandleWebsocketMessage(nc, event, wm);
  } else if(event == MG_EV_CLOSE) {
    HandleWebsocketMessage(nc, event, NULL);
  }
}

bool CheckPrefix(http_message *hm, const char *prefix) {
  if(mg_vcmp(&hm->uri, prefix) == 0) {
    return true;
  } else {
    return false;
  }
}