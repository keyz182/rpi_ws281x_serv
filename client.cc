#include <chrono>
#include <thread>
#include <cstdlib>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include "ws281x.pb.h"

#include <zmq.hpp>

#include <stdint.h>
#include "server/rpi_ws281x/ws2811.h"

ws281x::WS2811_Data data;
zmq::socket_t * client;
zmq::context_t context (1);
bool serverInited = false;


void WS281XToProto(ws281x::WS2811_Data *request, ws2811_t *ws2811){
    //Populate the protobuf object from the ws2811_t struct
    request->set_freq(ws2811->freq);
    request->set_render_wait_time(ws2811->render_wait_time);
    request->set_dmanum(ws2811->dmanum);

    ws2811_channel_t *ledchannel = ws2811->channel;

    while(ledchannel->gpionum > 0){
        ws281x::WS2811_Channel * channel = request->add_channel();
        channel->set_gpionum(ledchannel->gpionum);
        channel->set_invert(ledchannel->invert);
        channel->set_count(ledchannel->count);
        channel->set_strip_type(ledchannel->strip_type);
        channel->set_brightness(ledchannel->brightness);
        channel->set_wshift(ledchannel->wshift);
        channel->set_rshift(ledchannel->rshift);
        channel->set_gshift(ledchannel->gshift);
        channel->set_bshift(ledchannel->bshift);

        if(ledchannel->count > 0){
            for(int i = 0; i < ledchannel->count; i++){
                if(i < channel->leds_size()){
                    channel->set_leds(i,ledchannel->leds[i]);
                }else{
                    channel->add_leds(ledchannel->leds[i]);
                }
            }
        }

        ledchannel++;
    }
}

ws2811_return_t callMethod(::ws281x::WS2811_Method method, ws2811_t *ws2811){
    ws281x::MethodCall *methodCall = new ws281x::MethodCall();
    methodCall->set_method(method);
    WS281XToProto(methodCall->mutable_data(), ws2811);

    std::string serInit;
    methodCall->SerializeToString(&serInit);

    zmq::message_t msgInit(serInit.size());
    memcpy ((void *) msgInit.data(), serInit.c_str(), serInit.size());
    client->send(msgInit);

    zmq::message_t initResp;
    // //  Wait for response
    client->recv (&initResp);

    ws281x::WS2811Response parsedInitResp;
    parsedInitResp.ParseFromArray(initResp.data(), initResp.size());

//    std::cout << parsedInitResp.result() << std::endl;
    delete methodCall;
    return (ws2811_return_t)(-parsedInitResp.result());
}

ws2811_return_t ws2811_init(ws2811_t *ws2811)
{
    if(!serverInited){
        client = new zmq::socket_t (context, ZMQ_REQ);
        client->setsockopt(ZMQ_IDENTITY, "ZMQ", strlen("ZMQ"));
        char * url = getenv("WS2811_URL");
        client->connect (url ? url : "tcp://127.0.0.1:8879");

        serverInited = true;
    }

    for (int chan = 0; chan < RPI_PWM_CHANNELS; chan++)
    {
        ws2811_channel_t *channel = &ws2811->channel[chan];

        channel->leds = (ws2811_led_t*)malloc(sizeof(ws2811_led_t) * channel->count);
        if (!channel->leds)
        {
            return WS2811_ERROR_OUT_OF_MEMORY;
        }

        memset(channel->leds, 0, sizeof(ws2811_led_t) * channel->count);

        if (!channel->strip_type)
        {
            channel->strip_type=WS2811_STRIP_RGB;
        }

        // Set default uncorrected gamma table
        if (!channel->gamma)
        {
            channel->gamma = (uint8_t*)malloc(sizeof(uint8_t) * 256);
            int x;
            for(x = 0; x < 256; x++){
                channel->gamma[x] = x;
            }
        }

        channel->wshift = (channel->strip_type >> 24) & 0xff;
        channel->rshift = (channel->strip_type >> 16) & 0xff;
        channel->gshift = (channel->strip_type >> 8)  & 0xff;
        channel->bshift = (channel->strip_type >> 0)  & 0xff;
    }

    return callMethod(ws281x::WS2811_Method::WS2811_INIT, ws2811);
}

void ws2811_fini(ws2811_t *ws2811)
{
    callMethod(ws281x::WS2811_Method::WS2811_FINI, ws2811);
}

ws2811_return_t ws2811_wait(ws2811_t *ws2811)
{
    return callMethod(ws281x::WS2811_Method::WS2811_WAIT, ws2811);
}

ws2811_return_t  ws2811_render(ws2811_t *ws2811)
{
    return callMethod(ws281x::WS2811_Method::WS2811_RENDER, ws2811);
}

const char * ws2811_get_return_t_str(const ws2811_return_t state){
    const int index = -state;
    static const char * const ret_state_str[] = { WS2811_RETURN_STATES(WS2811_RETURN_STATES_STRING) };

    if (index < (int)(sizeof(ret_state_str) / sizeof(ret_state_str[0])))
    {
        return ret_state_str[index];
    }

    return "";
}