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

int w = 900;
int h = 500;

unsigned char *data;

float f_quad_area(t_quad q)
{
  return (q.y1 - q.y0) * (q.x1 - q.x0);
}

void f_calculate_mean(t_quad *q)
{
  uint32_t histogram[256 * 3] = {};

  for (uint32_t r = q->y0; r < q->y1; ++r)
  {
    for (uint32_t c = q->x0; c < q->x1; ++c)
    {
      for (int i = 0; i < 3; ++i)
      {
        uint32_t offset = r * w * 3 + c * 3 + i;
        ++histogram[data[offset] + (256 * i)];
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
  else if (worst == 0 || curr->standard_deviation > (worst)->standard_deviation)
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

void f_draw(t_quad q, uint32_t *pixels)
{
  uint32_t midx = q.x0 + (q.x1 - q.x0) / 2;
  uint32_t midy = q.y0 + (q.y1 - q.y0) / 2;

  if (q.tl)
  {
    f_draw(*q.tl, pixels);
    f_draw(*q.tr, pixels);
    f_draw(*q.bl, pixels);
    f_draw(*q.br, pixels);

    for (uint32_t c = q.x0; c < q.x1; ++c)
    {
      pixels[midy * w + c] = 0xff000000;
    }

    for (uint32_t r = q.y0; r < q.y1; ++r)
    {
      pixels[r * w + midx] = 0xff000000;
    }
  }
  else
  {
    for (uint32_t r = q.y0; r < q.y1; ++r)
    {
      for (uint32_t c = q.x0; c < q.x1; ++c)
      {
        pixels[r * w + c] = q.color;
      }
    }
  }
}

int main()
{

  data = stbi_load("./owl.jpg", &w, &h, 0, 3);
  if (!data)
  {
    printf("Failed to load image: %s\n", stbi_failure_reason());
    exit(0);
  }

  t_quad root = {};
  root.x0     = 0;
  root.y0     = 0;
  root.x1     = w;
  root.y1     = h;
  f_calculate_mean(&root);

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  SDL_Window *window     = SDL_CreateWindow("Hello", w, h, 0);
  SDL_Renderer *renderer = SDL_CreateRenderer(window, 0);
  SDL_Texture *texture   = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING, w, h);

  uint32_t *pixels = malloc(w * h * sizeof(uint32_t));
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
        for (int i = 0; i < 10; ++i)
        {

          worst = 0;
          f_max_deviation_quad(&root);
          f_divide(worst);
        }
      }

      for (int r = 0; r < h; ++r)
      {
        for (int c = 0; c < w; ++c)
        {

          int channels = 3;
          int color    = 0xff000000;

          for (int i = 0; i < channels; ++i)
          {
            color |=
                (data[r * w * channels + c * channels + i] << (8 * (2 - i)));
          }

          pixels[r * w + c] = color;
          pixels[r * w + c] = root.color;
        }
      }

      f_draw(root, pixels);

      SDL_UpdateTexture(texture, 0, pixels, w * sizeof(uint32_t));
      SDL_RenderTexture(renderer, texture, 0, 0);
      SDL_RenderPresent(renderer);
    }
  }
  return 0;
}
