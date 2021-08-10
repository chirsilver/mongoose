#include <bits/stdc++.h>
#include "../common/mongoose.h"
#include <sys/signal.h>
#include <sys/epoll.h>

using namespace std;

void MqttEventHandle(mg_connection *nc, int event, void *event_data, void *fn_data);
void MqttDataCallBack(mg_mqtt_message *mm, void *fn_data);
void StdinHandle(mg_connection *nc);
string dePrase(int cmd);
void DEBUG(mg_mqtt_message *mm) {
    int len = (int)mm->dgram.len;
    printf("\033[0;32mmqtt_cmd\033[0m: %s\n", dePrase(mm->cmd).data());
    puts("\033[0;32mdgram\033[0m: ");
    for (int i = 0; i < len; i++) {
        printf("%02x", (uint8_t)mm->dgram.ptr[i]);
        if ((i + 1) % 8 == 0) printf("\n");
        else if ((i + 1) % 4 == 0) printf(" ");
    }
    puts("\n");
}

string url = "mqtts://127.0.0.1:9000";
string s_topic = "mg/bye";
bool exit_f = false;
void (*mqtt_data_cb)(mg_mqtt_message*, void*);
double last_recv = mg_time();
int keepalive = 0;

void SigInt(int signo) {printf("-----------------------exit-----------------------\n");exit_f=true;}

int main() {
    printf("\033[0;31mUsage\033[0m: %s.\n\n", "[pub | sub] topic [data]");

    signal(SIGINT, SigInt);
    mg_mgr mgr;
    mg_mqtt_opts opt;
    mg_mgr_init(&mgr);
    bzero(&opt, sizeof(opt));

    opt.keepalive = 10;                        // Set Keep alive to 10s
    keepalive = opt.keepalive;

    mg_connection *nc = mg_mqtt_connect(&mgr, url.c_str(), &opt, MqttEventHandle, NULL);
    if(!nc) {
        puts("connect faild.\n");
        mg_mgr_free(&mgr);
        return 1;
    }

    nc->is_hexdumping = 1;

    mqtt_data_cb = MqttDataCallBack;

    int epfd = epoll_create(5);
    epoll_event ev;
    ev.data.fd = 0;
    ev.events = EPOLLIN;
    epoll_event evs[5];
    epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &ev);

    while(!exit_f) {
        mg_mgr_poll(&mgr, 100);
        int ret = epoll_wait(epfd, evs, 5, 100);
        if(ret > 0) {
            for(int i = 0; i < ret; i++) {
                if(evs[i].data.fd == 0) {
                    StdinHandle(nc);
                }
            }
        }
    }

    close(epfd);
    mg_mgr_free(&mgr);
    return 0;
}

void StdinHandle(mg_connection *nc) {
    string opt; cin >> opt;
    if(opt == "pub") {
        string topic, data; cin >> topic;
        getline(cin, data);
        data = data.substr(1, data.size() - 1);
        mg_str topic_ = mg_str(topic.data());
        mg_str data_ = mg_str(data.data());
        mg_mqtt_pub(nc, &topic_, &data_, 1, false);
        // LOG(LL_INFO, ("PUBSLISHED %.*s -> [%.*s]", (int)data_.len, data_.ptr, (int)topic_.len, topic_.ptr));
    } else if(opt == "sub") {
        string topic; cin >> topic;
        mg_str topic_ = mg_str(topic.data());
        mg_mqtt_sub(nc, &topic_, 1);
        // LOG(LL_INFO, ("SUBSLISHED topic [%.*s]", (int)topic_.len, topic_.ptr));
    } else if(opt == "unsub") {
        int num; cin >> num;
        char buf[128]; int now = 4;
        static uint16_t id = 0;
        if(++id == 0) ++id;
        for(int i = 0; i < num; i++) {
            string topic; cin >> topic;
            uint16_t len = topic.size();
            *(uint16_t*)(buf + now) = mg_htons(len); now += 2;
            sprintf(buf + now, "%s", topic.data()); now += topic.size();
            // LOG(LL_INFO, ("UNSUBSLISHED topic [%s]", topic.data()));
        }
        buf[0] = (uint8_t)(MQTT_CMD_UNSUBSCRIBE << 4 | 2);
        buf[1] = (uint8_t)(now - 2);
        *(uint16_t*)(buf+2) = mg_htons(id);

        mg_send(nc, buf, now);
    } else {
        printf("\033[0;31mUsage\033[0m: %s.", "[pub | sub] topic [data]");
    }
}

void MqttDataCallBack(mg_mqtt_message *mm, void *fn_data) {
    printf("\033[0;32mtopic\033[0m: %.*s\t\033[0;32mdata\033[0m: %.*s\n"
        , (int)mm->topic.len, mm->topic.ptr
        , (int)mm->data.len, mm->data.ptr);
}

void MqttEventHandle(mg_connection *nc, int event, void *event_data, void *fn_data) {
    switch(event) {
        case MG_EV_ERROR: {
            puts("some error happend.");
            exit_f = true;
            break;
        }
        case MG_EV_CLOSE: {
            puts("will close mg.");
            exit_f = true;
            break;
        }
        case MG_EV_CONNECT: {
            if(mg_url_is_ssl(url.c_str())) {
                mg_tls_opts opt = {.ca = ".ssl/ca.crt"/*, .cert = ".ssl/me.crt", .certkey = ".ssl/me.key"*/};
                mg_tls_init(nc, &opt);
            }
            last_recv = mg_time();
            break;
        }
        case MG_EV_MQTT_CMD: {
            last_recv = mg_time();
            break;
        }
        case MG_EV_MQTT_MSG: {
            mg_mqtt_message *mm = (mg_mqtt_message*)event_data;
            mqtt_data_cb(mm, fn_data);
            last_recv = mg_time();
            break;
        }
        case MG_EV_POLL: {
            double now = mg_time();
            if(now - last_recv > keepalive) {
                mg_mqtt_ping(nc);
                last_recv = now;
            }
            break;
        }
    }
}

string dePrase(int cmd) {
    switch(cmd) {
        case 1: return "MQTT_CMD_CONNECT";
        case 2: return "MQTT_CMD_CONNACK";
        case 3: return "MQTT_CMD_PUBLISH";
        case 4: return "MQTT_CMD_PUBACK";
        case 5: return "MQTT_CMD_PUBREC";
        case 6: return "MQTT_CMD_PUBREL";
        case 7: return "MQTT_CMD_PUBCOMP";
        case 8: return "MQTT_CMD_SUBSCRIBE";
        case 9: return "MQTT_CMD_SUBACK";
        case 10: return "MQTT_CMD_UNSUBSCRIBE";
        case 11: return "MQTT_CMD_UNSUBACK";
        case 12: return "MQTT_CMD_PINGREQ";
        case 13: return "MQTT_CMD_PINGRESP";
        case 14: return "MQTT_CMD_DISCONNECT";
        default: return "MQTT_CMD_Reserved";
    }
}