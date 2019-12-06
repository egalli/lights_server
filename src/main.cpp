
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/ip.h>
#include <signal.h>
#include <unistd.h>

#include <ws2811.h>

constexpr uint64_t kRenderWaitTime = 0;
constexpr int kDMANumber = 10;
constexpr int kGPIOPin = 18;
constexpr int kLEDCount = 100;
constexpr int kInvert = 0;
constexpr uint8_t kMaxBrightness = 200;
constexpr uint16_t kServerPort = 11687; // LIGHT

namespace
{

ws2811_t ledstring{kRenderWaitTime,
                   nullptr,
                   nullptr,
                   WS2811_TARGET_FREQ,
                   kDMANumber,
                   {{kGPIOPin, kInvert, kLEDCount, WS2811_STRIP_RGB, nullptr, kMaxBrightness}, {}}};

void reset_leds()
{
    std::cout << "Resetting LEDs" << std::endl;
    std::fill_n(ledstring.channel[0].leds, kLEDCount, 0);
    ws2811_render(&ledstring);
}

void setup_handlers()
{
    struct sigaction sa = {};
    sa.sa_handler = [](int sig) {
        std::cout << ((sig == SIGINT) ? "INT" : "TERM") << " signal received" << std::endl;
    
        reset_leds();
        ws2811_fini(&ledstring);
        exit(0);
    };

    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

}; // namespace

int main()
{

    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        std::cerr << "socket creation failed: " << strerror(errno) << std::endl;
        return -1;
    }

    {
        struct sockaddr_in servaddr
        {
            AF_INET, htons(kServerPort),
            {
                INADDR_ANY
            }
        };

        std::cout << "Listening for updates" << std::endl;
        if (bind(sockfd, reinterpret_cast<const struct sockaddr *>(&servaddr), sizeof(servaddr)) <
            0)
        {
            std::cerr << "bind failed: " << strerror(errno) << std::endl;
            return -1;
        }
    }

    {
        ws2811_return_t ret;
        if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS)
        {
            std::cerr << "ws2811_init failed: " << ws2811_get_return_t_str(ret) << std::endl;
            return ret;
        }
        setup_handlers();
    }

    reset_leds();

    for (;;)
    {
        struct sockaddr_in cliaddr
        {};
        socklen_t len = sizeof(struct sockaddr_in);
        int n;
        if ((n = recvfrom(sockfd, reinterpret_cast<char *>(ledstring.channel[0].leds),
                     kLEDCount * sizeof(ws2811_led_t), MSG_WAITALL,
                     reinterpret_cast<struct sockaddr *>(&cliaddr), &len)) < 0)
        {
            std::cerr << "recvfrom failed: " << strerror(errno) << std::endl;
            break;
        }
        ws2811_render(&ledstring);
    }

    reset_leds();

    ws2811_fini(&ledstring);

    return 0;
}
