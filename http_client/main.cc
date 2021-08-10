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
#include <string.h>

#include "../common/mongoose.h"

using namespace std;
typedef void (*RecvCallBack)(string data);

void SigInt(int signo);
void SendHttpReq(string url, RecvCallBack recvback, string post_data, bool use_tls);
void HandleHttpEvent(mg_connection *nc, int event, void *event_data, void *fn_data);
void CallBack(string data);
void HandleStdin();
vector<string> CutStr(string str);

bool exit_f = false;
bool exit_f = false;
bool is_ws_start = false;
RecvCallBack g_call_back;
struct Request {
    string *url;
    string *body;
    string *extra_headers;
    Request(){url=nullptr; body=nullptr; extra_headers=nullptr;}
};

void addHeader(Request *req, string name, string value) {
    if(!req->extra_headers) req->extra_headers = new string;
    req->extra_headers->append(name + ": " + value + "\r\n");
}

int main() {
    signal(SIGINT, SigInt);
    printf("\033[0;32mUsage\033[0m: protocol [url] [post_data(http) | msg(other)].\n");
    int epfd = epoll_create(5);
    epoll_event ev, evs[5];
    ev.data.fd = 0;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &ev);
    
    while(!exit_f) {
        int ret = epoll_wait(epfd, evs, 5, -1);
        if(ret > 0) {
            epoll_event iev;
            for(int i = 0;  i < ret; i++) iev = evs[i];
            if(iev.data.fd == 0) HandleStdin();
        }
    }
    return 0;
}

void HandleStdin() {
    string protocol, url, data;
    cin >> protocol;
    if(protocol == "http" || protocol == "https") {
        cin >> url;
        getline(cin, data);
        data = data.substr(1, data.size() - 1);
        SendHttpReq(url, CallBack, data, (protocol=="http")?false:true);
    } else if(protocol == "ws") {
        printf("SendWebsocketMsg: %s.\n", data.c_str());
    }
}

vector<string> CutStr(string str) {
    vector<string> res;
    int sz = str.size() - 1, st = 0, ed = 0;
    while(ed != sz) {
        ed = str.find(' ', st);
        res.push_back(str.substr(st, ed - st));
        st = ed + 1;
    }
    return res;
}

void HandleHttpEvent(mg_connection *nc, int event, void *event_data, void *fn_data) {
    Request *req = (Request*)fn_data;
    if(event == MG_EV_CONNECT) {
        mg_printf(nc, "%s %s HTTP/1.1\r\n"
                    "%s"
                    "\r\n", (req->body)?"POST":"GET", req->url->c_str(), req->extra_headers->c_str());
        if(req->body) mg_printf(nc, "%s", req->body->c_str());
    } else if(event == MG_EV_HTTP_MSG) {
        mg_http_message *hm = (mg_http_message*)event_data;
        printf("[Client] rsp: %*.s.\n", hm->body.len, hm->body.ptr);
        nc->is_closing = 1;
        exit_f = true;
    } else if(event == MG_EV_ERROR) {
        puts("[Client] some error.");
        exit_f = true;
    }
}

void SendHttpReq(string url, RecvCallBack recvback, string post_data, bool use_tls) {
    mg_mgr mgr;
    mg_mgr_init(&mgr);

    mg_connection *nc = mg_http_connect(&mgr, url.c_str(), HandleHttpEvent, NULL);
    if(NULL == nc) {
        mg_mgr_free(&mgr);
        perror("cannot connect to host.");
        return;
    }
    if(use_tls) {
        mg_tls_opts opt = {.ca = "./.ssl/ca.crt"};
        mg_tls_init(nc, &opt);
    }
    Request *req = new Request;

    req->url = &url;
    addHeader(req, "Connect", "close");
    if(post_data != "NULL") {
        addHeader(req, "Content-Length", to_string((int)post_data.size()).c_str());
        req->body = new string; 
        req->body = &post_data;
    }

    memcmp(nc->fn_data, req, sizeof(req));
    exit_f = false;
    while(!exit_f && !exit_f) mg_mgr_poll(&mgr, 1000);

    delete req;
    mg_mgr_free(&mgr);
}

void SigInt(int signo) {
  printf("-------------------------exit-------------------------\n");
  exit_f = true;
}

void CallBack(string data) {
    printf("[Client] recv callback: %s.\n", data.c_str());
}