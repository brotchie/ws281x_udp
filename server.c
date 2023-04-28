#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ws2811.h"

#define UDP_PORT 5005

#define PANEL_WIDTH 32
#define PANEL_HEIGHT 8
#define WIDTH 32
#define HEIGHT 24
#define DMA 10
#define GPIO_PIN 18
#define LED_COUNT (WIDTH * HEIGHT)
#define STRIP_TYPE WS2811_STRIP_GRB
#define TARGET_FREQ WS2811_TARGET_FREQ
#define BRIGHTNESS 20

ws2811_t ledstring = {
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
        {
            [0] =
                {
                    .gpionum = GPIO_PIN,
                    .invert = 0,
                    .count = LED_COUNT,
                    .strip_type = STRIP_TYPE,
                    .brightness = BRIGHTNESS,
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

int width = WIDTH;
int height = HEIGHT;

ws2811_led_t *matrix;

void render() {
  ws2811_return_t ret;
  int x, y;
  for (x = 0; x < width; x++) {
    for (y = 0; y < height; y++) {
      ledstring.channel[0].leds[(y * width) + x] = matrix[y * width + x];
    }
  }
  if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
    fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
    exit(EXIT_FAILURE);
  }
}

int coord(uint8_t x, uint8_t y) {
  if (y < 8 || y >= 16) {
    int offset = (PANEL_HEIGHT * PANEL_WIDTH) * (y / 8);
    y %= PANEL_HEIGHT;
    if (x % 2 == 0) {
      return offset + 255 - (x * PANEL_HEIGHT) - 7 + y;
    } else {
      return offset + 255 - (x * PANEL_HEIGHT) - y;
    }
  } else {
    int offset = (PANEL_HEIGHT * PANEL_WIDTH);
    y %= PANEL_HEIGHT;
    if (x % 2 == 1) {
      return offset + (x * PANEL_HEIGHT) + 7 - y;
    } else {
      return offset + (x * PANEL_HEIGHT) + y;
    }
  }
}

int main(int argc, char *argv[]) {
  int x, y;
  ws2811_return_t ret;

  matrix = malloc(sizeof(ws2811_led_t) * width * height);
  if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
    fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
    return ret;
  }
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1) {
    fprintf(stderr, "failed to create socket %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
  struct sockaddr_in address;

  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(UDP_PORT);
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
    fprintf(stderr, "failed to bind socket %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  char buffer[8192];
  struct sockaddr_storage src_addr;
  socklen_t src_addr_len = sizeof(src_addr);

  while (1) {
    ssize_t count = recvfrom(fd, buffer, sizeof(buffer), 0,
                             (struct sockaddr *)&src_addr, &src_addr_len);
    if (count % 7 != 0) {
      fprintf(stderr, "packet size should be a multiple of 7: %d", count);
      continue;
    }
    for (int i = 0; i < count; i += 7) {
	  // Leading zero byte - Push matrix buffer to LEDs via DMA.
      if (buffer[i] == 0x00) {
        render();
      }
	  // Leading one byte - Set the color of an individual pixel in matrix buffer.
      if (buffer[i] == 0x01) {
        uint8_t x = buffer[i + 1];
        uint8_t y = buffer[i + 2];
        uint8_t r = buffer[i + 3];
        uint8_t g = buffer[i + 4];
        uint8_t b = buffer[i + 5];
        uint8_t w = buffer[i + 6];
        if (x >= WIDTH || y >= HEIGHT) {
          // Ignore out-of-bounds pixels.
          continue;
        }
        matrix[coord(x, y)] = (r << 16) | (g << 8) | b;
      }
    }
  }
}
