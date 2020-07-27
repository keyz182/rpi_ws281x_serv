#include <zmq.hpp>
#include <string>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <stdarg.h>
#include <getopt.h>
#include <signal.h>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include "ws281x.pb.h"

#include "rpi_ws281x/clk.h"
#include "rpi_ws281x/gpio.h"
#include "rpi_ws281x/dma.h"
#include "rpi_ws281x/pwm.h"
#include "rpi_ws281x/ws2811.h"

using namespace std;

#define VERSION "0.0.1"

char * bind;
bool verbose = false;

#define VERBOSE if(verbose) printf

ws2811_t ledstrings =
{
    .freq = 800000,
    .dmanum = 10,
    .channel =
    {
        [0] =
        {
            .gpionum = 12,
            .invert = 0,
            .count = 100,
            .strip_type = 524304,
            .brightness = 255,
        },
        [1] =
        {
            .gpionum = 0,
            .invert = 0,
            .count = 0,
            .brightness = 0,
        },
    },
};

void ProtoToWS281X(const ws281x::WS2811_Data *request){
    ledstrings.freq = request->freq();
    ledstrings.render_wait_time = request->render_wait_time();
    ledstrings.dmanum = request->dmanum();

    for (int i = 0; i < request->channel_size(); i++) {
        ledstrings.channel[i].gpionum = (int)request->channel(i).gpionum();
        ledstrings.channel[i].invert = (int)request->channel(i).invert();
        ledstrings.channel[i].count = (int)request->channel(i).count();
        ledstrings.channel[i].strip_type = (int)request->channel(i).strip_type();
        ledstrings.channel[i].brightness = (uint8_t)request->channel(i).brightness();

        if(request->channel(i).leds_size() > 0){
            VERBOSE("LED Incoming Count: %d\n", request->channel(i).leds_size());
            VERBOSE("LED Channel Count: %d\n", ledstrings.channel[i].count);
            if(ledstrings.channel[i].leds == NULL){
                ledstrings.channel[i].leds = (ws2811_led_t*)malloc(sizeof(ws2811_led_t) * request->channel(i).leds_size());
                memset(ledstrings.channel[i].leds, 0, sizeof(ws2811_led_t) * request->channel(i).leds_size());
            }
            for(int j = 0; j < ledstrings.channel[i].count; j++){
                uint32_t val = (uint32_t)(request->channel(i).leds(j));
                ledstrings.channel[i].leds[j] = val;
                VERBOSE("%d: %#06x\n", j , ledstrings.channel[i].leds[j]);
            }
        }
    }

    ledstrings.channel[request->channel_size()].gpionum = 0;
    ledstrings.channel[request->channel_size()].invert = 0;
    ledstrings.channel[request->channel_size()].count = 0;
    ledstrings.channel[request->channel_size()].brightness = 0;
}


void parseargs(int argc, char **argv)
{
    int index;
    int c;

    static struct option longopts[] =
            {
                    {"help", no_argument, 0, 'h'},
                    {"bind", required_argument, 0, 'b'},
                    {"verbose", no_argument, 0, 'V'},
                    {"version", no_argument, 0, 'v'},
                    {0, 0, 0, 0}
            };

    while (1)
    {

        index = 0;
        c = getopt_long(argc, argv, "hb:Vv", longopts, &index);

        if (c == -1)
            break;

        switch (c)
        {
            case 0:
                /* handle flag options (array's 3rd field non-0) */
                break;

            case 'h':
                fprintf(stderr, "%s version %s\n", argv[0], VERSION);
                fprintf(stderr, "Usage: %s \n"
                                "-h (--help)    - this information\n"
                                "-b (--bind)    - zmq bind string\n"
                                "-V (--verbose) - Verbose output\n"
                                "-v (--version) - version information\n"
                        , argv[0]);
                exit(-1);

            case 'b':
                if (optarg) {
                    bind = optarg;
                }
                break;

            case 'V':
                verbose = true;
                break;

            case 'v':
                fprintf(stderr, "%s version %s\n", argv[0], VERSION);
                exit(-1);

            case '?':
                /* getopt_long already reported error? */
                exit(-1);

            default:
                exit(-1);
        }
    }
}

int main (int argc, char *argv[]) {
    parseargs(argc, argv);

    // Do some initialisation of the ws2811_t struct
    for (int chan = 0; chan < RPI_PWM_CHANNELS; chan++)
    {
        ws2811_channel_t *channel = &ledstrings.channel[chan];

//        channel->leds = (ws2811_led_t*)malloc(sizeof(ws2811_led_t) * channel->count);
//        if (!channel->leds)
//        {
//            return WS2811_ERROR_OUT_OF_MEMORY;
//        }

//        memset(channel->leds, 0, sizeof(ws2811_led_t) * channel->count);

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

    //Init the ZMQ server
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REP);

    //Allow setting the bind address via env var or cli
    if(bind == NULL){
        bind = getenv("WS2811_BIND");
    }
    socket.bind (bind ? bind : "tcp://*:8879");

    printf("Bound to %s\n", bind ? bind : "tcp://*:8879");

    bool running = true;

    while (running) {
        zmq::message_t request;

        VERBOSE("Waiting for request\n");
        socket.recv (&request);

        //Parse request into protobuf object
        ws281x::MethodCall methodCall;
        methodCall.ParseFromArray(request.data(), request.size());

        if(verbose) {
            std::string str;
            google::protobuf::TextFormat::PrintToString(methodCall, &str);
            VERBOSE("%s\n", str.c_str());
        }

        //Populate the ws2811_t struct from the incoming data
        ProtoToWS281X(&(methodCall.data()));

        ws2811_return_t ret = WS2811_SUCCESS;

        switch(methodCall.method()){
            case ws281x::WS2811_Method::WS2811_INIT:
                {
                    std::cout << "method: " << "init" << std::endl;
                    std::cout << "freq: " << methodCall.data().freq() << std::endl;
                    std::cout << "dmanum: " << methodCall.data().dmanum() << std::endl;
                    std::cout << "gpionum: " << ledstrings.channel[0].gpionum << std::endl;
                    std::cout << "count: " << ledstrings.channel[0].count << std::endl;
                    std::cout << "invert: " << ledstrings.channel[0].invert << std::endl;
                    std::cout << "brightness: " << (int)(ledstrings.channel[0].brightness) << std::endl;
                    std::cout << "strip_type: " << ledstrings.channel[0].strip_type << std::endl;

                    if(ledstrings.channel[0].leds){
                        //Make sure we don't leak
                        free(ledstrings.channel[0].leds);
                    }
                    if ((ret = ws2811_init(&ledstrings)) != WS2811_SUCCESS)
                    {
                        fprintf(stderr, "[RPI_WS281x] ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
                    }

                    break;
                }
            case ws281x::WS2811_Method::WS2811_FINI:
                {
                    VERBOSE("ws2811_fini\n");
                    ws2811_fini(&ledstrings);
                    break;
                }
            case ws281x::WS2811_Method::WS2811_RENDER:
                {
                    VERBOSE("ws2811_render\n");

                    if ((ret = ws2811_render(&ledstrings)) != WS2811_SUCCESS)
                    {
                        fprintf(stderr, "[RPI_WS281x] ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
                    }
                    break;
                }
            case ws281x::WS2811_Method::WS2811_WAIT:
                {
                    VERBOSE("ws2811_wait\n");
                    if ((ret = ws2811_wait(&ledstrings)) != WS2811_SUCCESS)
                    {
                        fprintf(stderr, "[RPI_WS281x] ws2811_wait failed: %s\n", ws2811_get_return_t_str(ret));
                    }
                    break;
                }
            case ws281x::WS2811_Method::WS2811_EXIT:
                {
                    VERBOSE("ws2811_exit\n");

                    for(int i = 0; i < methodCall.data().channel(0).leds_size(); i++){
                        ledstrings.channel[0].leds[i] = 0;
                    }

                    std::cout << "set leds" << std::endl;

                    if ((ret = ws2811_render(&ledstrings)) != WS2811_SUCCESS)
                    {
                        fprintf(stderr, "[RPI_WS281x] ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
                    }

                    running = false;
                    break;
                }
            default:
                std::cout << "method: " << "UNKNOWN!" << std::endl;
        }

        // Send the response back to the client
        ws281x::WS2811Response response;
        response.set_result((ws281x::WS2811_Result)abs(ret));

        std::string serialized_response;
        response.SerializeToString(&serialized_response);

        zmq::message_t reply (serialized_response.size());
        memcpy ((void *) reply.data(), serialized_response.c_str(), serialized_response.size());
        socket.send(reply);
    }
    return 0;
}