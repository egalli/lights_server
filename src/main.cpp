
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/ip.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

#include <ws2811.h>

constexpr uint64_t kRenderWaitTime = 0;
constexpr int kDMANumber = 10;
constexpr int kGPIOPin0 = 18; // PWM0
constexpr int kGPIOPin1 = 13; // PWM1
constexpr int kLEDCount0 = 50;
constexpr int kLEDCount1 = 100;
constexpr int kInvert = 0;
constexpr uint8_t kMaxBrightness = 200;
constexpr uint16_t kServerPort = 11687; // LIGHT

constexpr time_t kTimeout = 5;

namespace
{

ws2811_t ledstring{kRenderWaitTime,
                   nullptr,
                   nullptr,
                   WS2811_TARGET_FREQ,
                   kDMANumber,
                   {
                       {kGPIOPin0, kInvert, kLEDCount0, WS2811_STRIP_RGB, nullptr, kMaxBrightness},
                       {kGPIOPin1, kInvert, kLEDCount1, WS2811_STRIP_RGB, nullptr, kMaxBrightness},
                   }};

void reset_leds()
{
    std::cout << "Resetting LEDs" << std::endl;
    std::fill_n(ledstring.channel[0].leds, kLEDCount0, 0);
    std::fill_n(ledstring.channel[1].leds, kLEDCount1, 0);
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

void update_lights(const std::array<ws2811_led_t, kLEDCount0 + kLEDCount1> &buff)
{
    if (kLEDCount0 > 0)
    {
        std::copy_n(buff.begin(), kLEDCount0, ledstring.channel[0].leds);
    }
    if (kLEDCount1 > 0)
    {
        std::copy_n(buff.begin() + kLEDCount0, kLEDCount1, ledstring.channel[1].leds);
    }
    ws2811_render(&ledstring);
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
        sockaddr_in servaddr{AF_INET, htons(kServerPort), {INADDR_ANY}};

        std::cout << "Listening for updates" << std::endl;
        if (bind(sockfd, reinterpret_cast<const sockaddr *>(&servaddr), sizeof(servaddr)) < 0)
        {
            std::cerr << "bind failed: " << strerror(errno) << std::endl;
            return -1;
        }
        timeval tv{};
        tv.tv_sec = kTimeout;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            std::cerr << "setsockopt failed: " << strerror(errno) << std::endl;
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

    std::array<ws2811_led_t, kLEDCount0 + kLEDCount1> buff{};
    bool zero = true;
    sockaddr_in cliaddr{};
    socklen_t len = sizeof(sockaddr_in);
    for (;;)
    {

        int n;
        if ((n = recvfrom(sockfd, reinterpret_cast<char *>(buff.data()),
                          buff.size() * sizeof(ws2811_led_t), MSG_WAITALL,
                          reinterpret_cast<sockaddr *>(&cliaddr), &len)) < 0)
        {
            if (!zero)
            {
                std::cerr << "No packets received for " << kTimeout << " seconds, resetting lights."
                          << std::endl;
                std::fill(buff.begin(), buff.end(), 0);
                zero = true;
                update_lights(buff);
            }
            continue;
        }
        zero = false;
        update_lights(buff);
    }

    reset_leds();

    ws2811_fini(&ledstring);

    return 0;
}
