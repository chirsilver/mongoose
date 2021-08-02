// C Headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <bits/stdc++.h>
#include "../common/mongoose.h"
using namespace std;
typedef void (*RecvCallBack)(string data);

void SigInt(int signo);
void SendHttpReq(string url, string post_data, RecvCallBack recvback);
void HandleHttpEvent(mg_connection *nc, int event, void *event_data);
void CallBack(string data);

bool g_quit = false;
bool in_poll = false;
RecvCallBack g_call_back;

int main() {
    signal(SIGINT, SigInt);
    SendHttpReq("http://localhost:8080/api/post", "a=13&b=14", CallBack);
    
    return 0;
}

extern "C" {
    #include "../common/mongoose.c"
}

void HandleHttpEvent(mg_connection *nc, int event, void *event_data) {
    if(event == MG_EV_CONNECT) {
        int status = *(int *)event_data;
        if(status != 0) {
            printf("Error connecting to server, error code: %d\n", status);
        }
        printf("connected success.\n");
    } else if(event == MG_EV_HTTP_REPLY) {
        http_message *hm = (http_message *)event_data;
        if(hm->resp_code != 200) {
            g_call_back(string(hm->body.p, hm->body.len));
            printf("Response status-code: %d\n", hm->resp_code);
        } else {
            string body = string(hm->body.p, hm->body.len);
            g_call_back(body);
        
            nc->flags |= MG_F_SEND_AND_CLOSE;
            in_poll = false;
        }
    } else if(event == MG_EV_CLOSE) {
        printf("disconnected success.\n");
        in_poll = false;
    }
    in_poll = false;
}

void SendHttpReq(string url, string post_data, RecvCallBack recvback) {
    mg_mgr mgr;
    mg_mgr_init(&mgr, NULL);
    mg_connection *nc;
    if(post_data == "NULL") {
        nc = mg_connect_http(&mgr, HandleHttpEvent, url.c_str(), NULL, NULL);
    } else {
        nc = mg_connect_http(&mgr, HandleHttpEvent, url.c_str(), NULL, post_data.c_str());
    }
    if(NULL == nc) {
        mg_mgr_free(&mgr);
        perror("error: connect to host.");
        return;
    }
    mg_set_protocol_http_websocket(nc);
    g_call_back = recvback;
    while(!g_quit && !in_poll) {
        in_poll = true;
        mg_mgr_poll(&mgr, 1000);
    }

    mg_mgr_free(&mgr);
}

void SigInt(int signo) {
  printf("-------------------------exit-------------------------\n");
  g_quit = true;
}

void CallBack(string data) {
    printf("receive callback: %s.\n", data.c_str());
}