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

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/comp.h>

extern "C" {
    #include "../common/mongoose.h"
}

using namespace std;
typedef void (*RecvCallBack)(string data);

void SigInt(int signo);
void SendHttpReq(string url, string post_data, RecvCallBack recvback);
void HandleHttpEvent(mg_connection *nc, int event, void *event_data);
void CallBack(string data);
void HandleStdin();
vector<string> CutStr(string str);

bool g_quit = false;
bool in_poll = false;
bool is_ws_start = false;
RecvCallBack g_call_back;

int main() {
    signal(SIGINT, SigInt);
    printf("\033[0;32mUsage\033[0m: protocol [url] [post_data(http) | msg(other)], \033[0;31monly one space!\033[0m\n");
    int epfd = epoll_create(5);
    epoll_event ev, evs[5];
    ev.data.fd = 0;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &ev);
    
    while(!g_quit) {
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
    string str;
    getline(cin, str);
    str += ' ';
    if (str.find(' ') == str.size() - 1) {
        printf("\033[01;31merror\033[0m: Usage: protocol [url] [post_data(http) | msg(other)], \033[01;31monly one space!\033[0m\n");
        return ;
    }
    vector<string> vstr = CutStr(str);
    if(vstr[0] == "http") {
        string url = vstr[1];
        string post_data = str.substr(6 + url.size(), str.size() - 7 - url.size());
        printf("Http Request: url: %s post: %s.\n", url.c_str(), post_data.c_str());
        SendHttpReq("http://localhost:8080" + url, post_data, CallBack);
    } else if(vstr[0] == "ws") {
        string msg = str.substr(3, str.size() - 4);
        printf("SendWebsocketMsg: %s.\n", msg.c_str());
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

void HandleHttpEvent(mg_connection *nc, int event, void *event_data) {
    if(event == MG_EV_CONNECT) {
        int status = *(int *)event_data;
        if(status != 0) {
            printf("Error connecting to server, error code: %d\n", status);
        }
        printf("[Client] connected.\n");
    } else if(event == MG_EV_HTTP_REPLY) {
        http_message *hm = (http_message *)event_data;
        if(hm->resp_code != 200) {
            g_call_back(string(hm->body.p, hm->body.len));
            printf("Response status-code: %d\n", hm->resp_code);
        } else {
            string body = string(hm->body.p, hm->body.len);
            g_call_back(body);
        }
        nc->flags |= MG_F_SEND_AND_CLOSE;
        in_poll = true;
    } else if(event == MG_EV_CLOSE && nc->flags) {
        printf("[Client] disconnected.\n\n");
    }
}

void SendHttpReq(string url, string post_data, RecvCallBack recvback) {
    mg_mgr mgr;
    mg_connect_opts opts;
    mg_connection *nc;
    mg_mgr_init(&mgr, NULL);

    memset(&opts, 0, sizeof(opts));
    opts.ssl_key = "./.ssl/me.key";
    opts.ssl_cert = "./.ssl/me.crt";
    opts.ssl_ca_cert = "./.ssl/ca.crt";
    
    if(post_data == "NULL") {
        nc = mg_connect_http_opt(&mgr, HandleHttpEvent, opts, url.c_str(), NULL, NULL);
    } else {
        nc = mg_connect_http_opt(&mgr, HandleHttpEvent, opts, url.c_str(), NULL, post_data.c_str());
    }
    if(NULL == nc) {
        mg_mgr_free(&mgr);
        perror("error: connect to host.");
        return;
    }
    mg_set_protocol_http_websocket(nc);
    g_call_back = recvback;
    in_poll = false;
    while(!g_quit && !in_poll) {
        mg_mgr_poll(&mgr, 1000);
    }

    mg_mgr_free(&mgr);
}

void SigInt(int signo) {
  printf("-------------------------exit-------------------------\n");
  g_quit = true;
}

void CallBack(string data) {
    printf("[Client] receive callback: %s.\n", data.c_str());
}

extern "C" {
    #include "../common/mongoose.c"
}