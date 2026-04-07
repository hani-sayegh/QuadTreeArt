#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <SDL3/SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/*
todo:
1. aspect ration
2. pq
3. clean up and understand formula better
4. split exponentially like when you just split by 4 each time
5. support other shapes?
   */

typedef struct t_quad
{
  uint32_t color;
  float standard_deviation;
  uint32_t x0;
  uint32_t y0;
  uint32_t x1;
  uint32_t y1;
  struct t_quad *tl;
  struct t_quad *tr;
  struct t_quad *bl;
  struct t_quad *br;
} t_quad;

int pic_x = 900;
int pic_y = 500;

unsigned char *pic_pixels;

float f_quad_area(t_quad q)
{
  return (q.y1 - q.y0) * (q.x1 - q.x0);
}

float scale    = 0;
float window_x = 1920;
float window_y = 1080;

typedef struct
{
  int x;
  int y;
} Vec2;

Vec2 f_start_x()
{
  float p_shown     = 1. / scale;
  float p_shown_not = 1. - p_shown;
  int start         = p_shown_not / 2 * (window_x) + 1;
  return (Vec2){start, start + p_shown * window_x};
}

Vec2 f_start_y()
{
  float p_shown     = scale;
  float p_shown_not = 1. - p_shown;
  int start         = p_shown_not / 2 * (window_y);
  return (Vec2){start, start + p_shown * window_y};
}

#define dbg(x)                                                                 \
  _Generic((x),                                                                \
      float: printf(#x ": %f \n", x),                                          \
      default: printf("unknown type\n"))
int f_window_pos_to_pic_pos(int win_x, int win_y)
{
  // mark  f_window_pos_to_pic_pos
  float u = (float)win_x / (window_x - 1);
  float v = (float)win_y / (window_y - 1);
  if (scale >= 1.0)
  {
    float p_shown     = 1. / scale;
    float p_shown_not = 1. - p_shown;
    u -= p_shown_not / 2;
    u *= scale;

    if (u > 1.0 || u < 0)
    {
      return -1;
    }
  }
  else
  {
    float yScale      = 1. / scale;
    float p_shown     = scale;
    float p_shown_not = 1.0 - p_shown;
    v -= p_shown_not / 2.;
    v *= yScale;
    if (v > 1.0 || v < 0)
    {
      return -1;
    }
  }

  int ix = (int)(u * (pic_x - 1));
  int iy = (int)(v * (pic_y - 1));

  return iy * pic_x + ix;
}

void f_calculate_mean(t_quad *q)
{
  uint32_t histogram[256 * 3] = {};

  for (uint32_t x = q->x0; x < q->x1; ++x)
  {
    for (uint32_t y = q->y0; y < q->y1; ++y)
    {
      for (int i = 0; i < 3; ++i)
      {
        int offset = f_window_pos_to_pic_pos(x, y) * 3 + i;
        if (offset != -1)
        {
          ++histogram[pic_pixels[offset] + (256 * i)];
        }
      }
    }
  }

  q->color       = 0xff000000;
  int finalError = 0;

  float vals[3] = {.299, .587, .114};
  for (int i = 0; i < 3; ++i)
  {
    int count  = 256;
    int sum    = 0;
    int weight = 0;
    for (int j = 0; j < count; ++j)
    {
      weight += j * histogram[j + count * i];
      sum += histogram[j + count * i];
    }

    if (sum)
    {
      int mean = weight / sum;
      float wd = 0;
      for (int j = 0; j < count; ++j)
      {
        float diff = mean - j;
        diff *= diff;
        wd += diff * histogram[j + count * i];
      }
      wd /= sum;
      q->standard_deviation += sqrt(wd) * vals[i];
      q->color |= ((mean) << (8 * (2 - i)));
    }
  }
  q->standard_deviation *= pow(f_quad_area(*q), .25);
}

t_quad *worst;
void f_max_deviation_quad(t_quad *curr)
{
  if (curr->tl)
  {
    f_max_deviation_quad(curr->tl);
    f_max_deviation_quad(curr->tr);
    f_max_deviation_quad(curr->bl);
    f_max_deviation_quad(curr->br);
  }
  else if (worst == NULL ||
           curr->standard_deviation > (worst)->standard_deviation)
  {
    worst = curr;
  }
}

void f_divide(t_quad *curr)
{
  uint32_t midx = curr->x0 + (curr->x1 - curr->x0) / 2;
  uint32_t midy = curr->y0 + (curr->y1 - curr->y0) / 2;

  if (curr->tl == NULL)
  {
    if (midx != curr->x0)
    {

      curr->tl = calloc(1, sizeof(t_quad));
      curr->tr = calloc(1, sizeof(t_quad));
      curr->bl = calloc(1, sizeof(t_quad));
      curr->br = calloc(1, sizeof(t_quad));

      curr->tl->x0 = curr->x0;
      curr->tl->y0 = curr->y0;
      curr->tl->x1 = midx;
      curr->tl->y1 = midy;

      curr->tr->x0 = midx;
      curr->tr->y0 = curr->y0;
      curr->tr->x1 = curr->x1;
      curr->tr->y1 = midy;

      curr->bl->x0 = curr->x0;
      curr->bl->y0 = midy;
      curr->bl->x1 = midx;
      curr->bl->y1 = curr->y1;

      curr->br->x0 = midx;
      curr->br->y0 = midy;
      curr->br->x1 = curr->x1;
      curr->br->y1 = curr->y1;

      f_calculate_mean(curr->br);
      f_calculate_mean(curr->bl);
      f_calculate_mean(curr->tl);
      f_calculate_mean(curr->tr);
    }
  }
  else
  {
    f_divide(curr->tl);
    f_divide(curr->tr);
    f_divide(curr->bl);
    f_divide(curr->br);
  }
}

void f_draw(t_quad q, uint32_t *win_pixels)
{
  uint32_t midx = q.x0 + (q.x1 - q.x0) / 2;
  uint32_t midy = q.y0 + (q.y1 - q.y0) / 2;

  if (q.tl)
  {
    // mark draw black line
    f_draw(*q.tl, win_pixels);
    f_draw(*q.tr, win_pixels);
    f_draw(*q.bl, win_pixels);
    f_draw(*q.br, win_pixels);

    for (uint32_t c = q.x0; c < q.x1; ++c)
    {
      win_pixels[midy * (int)window_x + c] = 0xff000000;
    }

    for (uint32_t r = q.y0; r < q.y1; ++r)
    {
      win_pixels[r * (int)window_x + midx] = 0xff000000;
    }
  }
  else
  {
    for (uint32_t x = q.x0; x < q.x1; ++x)
    {
      for (uint32_t y = q.y0; y < q.y1; ++y)
      {
        int pos = f_window_pos_to_pic_pos(x, y);
        if (pos == -1.0)
        {
          break;
        }

        uint32_t color = 0xff000000;
        for (int i = 0; i < 3; ++i)
        {
          uint32_t offset = pos * 3 + i;
          color <<= 8;
          color |= (pic_pixels[offset]);
        }
        color |= 0xff000000;
        // win_pixels[(int)(y * window_x + x)] = color;
        win_pixels[(int)(y * window_x + x)] = q.color;
      }
    }
  }
}

void f_generate_frame(uint32_t *win_pixels, int frame, int width, int height)
{
  char header[256];
  sprintf(header, "./debug/frame-%d.ppm", frame);
  FILE *f = fopen(header, "wb");
  fprintf(f, "P6\n%d %d \n255\n", width, height);
  for (int r = 0; r < height; ++r)
  {
    for (int c = 0; c < width; ++c)
    {
      char red = win_pixels[r * width + c] >> 16;
      char g   = win_pixels[r * width + c] >> 8;
      char b   = win_pixels[r * width + c];
      fwrite(&red, 1, 1, f);
      fwrite(&g, 1, 1, f);
      fwrite(&b, 1, 1, f);
    }
  }
}

#define _(x) #x, x

int main()
{
  pic_pixels = stbi_load("./in_pic.jpg", &pic_x, &pic_y, 0, 3);

  printf("%s: %d %s: %d\n", _(pic_x), _(pic_y));

  // mark window size
  window_x = 640;
  window_y = 640;

  scale = (float)window_x / window_y;
  scale /= (float)pic_x / pic_y;
  dbg(scale);

  if (!pic_pixels)
  {
    printf("Failed to load image: %s\n", stbi_failure_reason());
    exit(0);
  }

  t_quad root = {};
  Vec2 start  = scale < 1.0 ? f_start_y() : f_start_x();
  root.x0     = 0;
  root.y0     = 0;
  root.x1     = window_x;
  root.y1     = window_y;
  if (scale < 1.0)
  {
    root.y0 = start.x;
    root.y1 = start.y;
  }
  else
  {
    root.x0 = start.x;
    root.x1 = start.y;
  }

  // f_calculate_mean(&root);

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  SDL_Window *window     = SDL_CreateWindow("Hello", window_x, window_y, 0);
  SDL_Renderer *renderer = SDL_CreateRenderer(window, 0);
  SDL_Texture *texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING, window_x, window_y);

  uint32_t *win_pixels = malloc(window_x * window_y * sizeof(uint32_t));
  int divideCount      = 1;
  bool render          = true;
  int frame            = 0;
  while (true)
  {
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
      if (e.type == SDL_EVENT_QUIT)
      {
        exit(0);
      }

      if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)
      {
        exit(0);
      }

      if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_J)
      {
        // exponential divide
        for (int count = 0; count < divideCount; ++count)
        {
          worst = NULL;
          f_max_deviation_quad(&root);
          f_divide(worst);
        }
        if (divideCount < 1024)
        {
          divideCount *= 2;
        }
        render = true;
      }

      if (render)
      {
        f_draw(root, win_pixels);
        // f_generate_frame(win_pixels, frame++, pic_x, pic_y);
      }

      SDL_UpdateTexture(texture, 0, win_pixels, window_x * sizeof(uint32_t));
      SDL_RenderTexture(renderer, texture, 0, 0);
      SDL_RenderPresent(renderer);
    }
  }
  return 0;
}
