#include <bits/stdc++.h>
#include "../common/mongoose.h"
#include <sys/signal.h>
using namespace std;


string url = "mqtts://127.0.0.1:9000";

struct sub {
    sub *next;
    mg_connection *c;
    mg_str topic;
    uint8_t qos;
};

void MqttEventHandle(mg_connection *nc, int event, void *event_data, void *fn_data);

static sub *s_subs = NULL;
bool exit_f = false;

void SigInt(int signo) {printf("-----------------------exit-----------------------\n");exit_f=true;}

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

int main() {
    signal(SIGINT, SigInt);
    mg_mgr mgr;
    mg_mgr_init(&mgr);

    mg_connection *nc = mg_mqtt_listen(&mgr, url.c_str(), MqttEventHandle, NULL);
    if(!nc) {
        puts("some error happend.");
        goto fail_init;
    }
    while(!exit_f) mg_mgr_poll(&mgr, 500);
fail_init:
    mg_mgr_free(&mgr);
    return 0;
}

void MqttEventHandle(mg_connection *nc, int event, void *event_data, void *fn_data) {
    switch(event) {
        case MG_EV_ACCEPT: {
            mg_tls_opts tls_opt = {/*.ca = ".ssl/ca.crt", */.cert = ".ssl/me.crt", .certkey = ".ssl/me.key"};
            mg_tls_init(nc, &tls_opt);
            break;
        }
        case MG_EV_MQTT_CMD: {
            mg_mqtt_message *mm = (mg_mqtt_message*)event_data;

            #ifdef DEBUG_
            DEBUG(mm);
            #endif //DEBUG_

            switch(mm->cmd) {
                case MQTT_CMD_CONNECT: {
                    uint8_t rsp[] = {0, 0};
                    mg_mqtt_send_header(nc, MQTT_CMD_CONNACK, 0, sizeof(rsp));
                    mg_send(nc, rsp, sizeof(rsp));
                    break;
                }
                case MQTT_CMD_SUBSCRIBE: {
                    //pos为4 固定报头1字节，剩余长度1字节，可变报头2字节（表示报文标识符，用于SUBACK）
                    int pos = 4;
                    uint8_t qos;
                    mg_str topic;
                    while((pos = mg_mqtt_next_sub(mm, &topic, &qos, pos)) > 0) {
                        sub *sub_ = new sub;
                        sub_->c = nc;
                        sub_->topic = mg_strdup(topic);
                        sub_->qos = qos;
                        sub_->next = s_subs;
                        s_subs = sub_;
                        LOG(LL_INFO, ("SUB %p [%.*s]", nc->fd, (int)sub_->topic.len, sub_->topic.ptr));
                    }
                    break;
                }
                case MQTT_CMD_PUBLISH: {
                    //客户端发布消息，添加到所有的订阅频道
                    LOG(LL_INFO, ("PUB %p [%.*s] -> [%.*s]", nc->fd, (int)mm->data.len, mm->data.ptr, (int)mm->topic.len, mm->topic.ptr));
                    for(sub *sub_ = s_subs; sub_ != NULL; sub_ = sub_->next) {
                        if(mg_strcmp(sub_->topic, mm->topic) != 0) continue;
                        mg_mqtt_pub(sub_->c, &mm->topic, &mm->data, 1, false);
                    }
                    break;
                }
                case MQTT_CMD_UNSUBSCRIBE: {
                    int pos = 4;
                    mg_str topic;
                    while((pos = mg_mqtt_next_unsub(mm, &topic, pos)) > 0) {
                        printf("you want unsub %.*s\n", (int)topic.len, topic.ptr);
                        for (sub *sub_ = s_subs, *pre = NULL; sub_ != NULL; sub_ = sub_->next) {
                            if(sub_->c->fd == nc->fd && mg_strcmp(sub_->topic, topic) == 0) {
                                if(pre) pre->next = sub_->next;
                                else s_subs = sub_->next;
                                delete sub_;
                                LOG(LL_INFO, ("UNSUB %p [%.*s]", nc->fd, (int)sub_->topic.len, sub_->topic.ptr));
                                break;
                            }
                            pre = sub_;
                        }    
                    }
                    
                    //UNSUBACK
                    char ack[128];
                    ack[0] = (uint8_t)(MQTT_CMD_UNSUBACK << 4 | 0);
                    ack[1] = (uint8_t)(2);
                    *(uint16_t*)(ack+2) = *(uint16_t*)(mm->dgram.ptr + 2);
                    mg_send(nc, ack, 4);
                }
                default: {
                    #ifdef DEBUG_
                    DEBUG(mm);
                    #endif //DEBUG_
                }
            }
            break;
        }
        case MG_EV_CLOSE: {
            //将这条连接上的订阅全部取消
            for(sub *nxt, *sub_ = s_subs; sub_ != NULL; sub_ = nxt) {
                nxt = sub_->next;
                if(nc != sub_->c) continue;
                LOG(LL_INFO,
                    ("UNSUB %p [%.*s]", nc->fd, (int)sub_->topic.len, sub_->topic.ptr));
                delete sub_;
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