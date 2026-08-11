#include <stdio.h>

#include "board.h"
#include "uart_printf.h"
#include "stm32f769i_discovery_lcd.h"
#include "stm32f769i_discovery_ts.h"

/* ------------------------------------------------------------------------ */
/* Screen layout: the bottom FPS_BAND rows are a status band (FPS + touch),
 * animations are clipped to ANIM_H. */
#define SCREEN_W     (BSP_LCD_GetXSize())     /* 800 */
#define SCREEN_H     (BSP_LCD_GetYSize())     /* 480 */
#define FPS_BAND     24
#define ANIM_H       (SCREEN_H - FPS_BAND)
#define BACK_COLOR   LCD_COLOR_BLACK

/* The LTDC framebuffer is a plain global array in the on-board SDRAM (the
 * `.sdram` linker section). No fixed address: .data/.bss/heap also live in the
 * SDRAM, so a hardcoded base (0xC0000000) would collide with them. The app
 * passes this array's address to the LTDC layer config. */
#define FB_W         800
#define FB_H         480
static uint32_t framebuffer[FB_W * FB_H] __attribute__((section(".sdram")));
#define FB_ADDR      ((uint32_t)(uintptr_t)framebuffer)

/* On-board LEDs (LD1 PJ13, LD2 PJ5, LD3 PA12), high active. */
#define LD1_PORT GPIOJ
#define LD1_PIN  GPIO_PIN_13
#define LD2_PORT GPIOJ
#define LD2_PIN  GPIO_PIN_5
#define LD3_PORT GPIOA
#define LD3_PIN  GPIO_PIN_12

/* ------------------------------------------------------------------------ */
/* The shared board layer opens the FMC + SDRAM (write-through cacheable), so
 * the LTDC (a separate bus master) sees CPU writes to the framebuffer without
 * cache maintenance - no extra MPU config needed here. */

/* ------------------------------------------------------------------------ */
/* The LCD BSP enables the LTDC / DMA2D / DSI NVIC lines; give them real
 * handlers (the startup weak defaults are an infinite loop). */
extern LTDC_HandleTypeDef hltdc_discovery;
extern DSI_HandleTypeDef hdsi_discovery;
extern DMA2D_HandleTypeDef hdma2d_discovery;

void LTDC_IRQHandler(void)   { HAL_LTDC_IRQHandler(&hltdc_discovery); }
void DMA2D_IRQHandler(void)  { HAL_DMA2D_IRQHandler(&hdma2d_discovery); }
void DSI_IRQHandler(void)    { HAL_DSI_IRQHandler(&hdsi_discovery); }

/* ------------------------------------------------------------------------ */
/* FPS: g_frames is incremented once per rendered animation frame;
 * status_update() redraws the on-screen FPS + touch line once per second.  */
static volatile uint32_t g_frames;
static uint32_t          g_last_frames;
static uint32_t          g_fps_last_tick;

/* Touch state (polled each frame). */
static TS_StateTypeDef g_ts;
static uint16_t g_last_x = 0xFFFF;
static uint16_t g_last_y = 0xFFFF;

static void fps_frame(void)
{
    g_frames++;
}

/* Paint the reserved bottom band (solid, behind the status text). */
static void paint_band(void)
{
    BSP_LCD_SetTextColor(BACK_COLOR);
    BSP_LCD_SetBackColor(BACK_COLOR);
    BSP_LCD_FillRect(0, ANIM_H, SCREEN_W, FPS_BAND);
}

static void band_text(const char *s)
{
    BSP_LCD_SetFont(&Font12);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetBackColor(BACK_COLOR);
    BSP_LCD_DisplayStringAt(2, ANIM_H + 6, (uint8_t *)s, LEFT_MODE);
}

static void status_update(void)
{
    uint32_t now = HAL_GetTick();
    if (now - g_fps_last_tick >= 1000)
    {
        uint32_t fps = g_frames - g_last_frames;
        g_last_frames = g_frames;
        g_fps_last_tick = now;

        char buf[48];
        if (g_ts.touchDetected)
        {
            snprintf(buf, sizeof(buf), "FPS:%03lu | T:%u,%u",
                     (unsigned long)fps, (unsigned)g_ts.touchX[0], (unsigned)g_ts.touchY[0]);
        }
        else
        {
            snprintf(buf, sizeof(buf), "FPS:%03lu | T:--",
                     (unsigned long)fps);
        }
        band_text(buf);
    }
}

/* ------------------------------------------------------------------------ */
/* Touch: polled each frame; a marker follows the finger and the coordinates
 * are printed to the console. Coordinates are clamped so the marker circle
 * (radius 12) stays fully inside the framebuffer (BSP fills are unclipped). */
static void touch_poll(void)
{
    uint16_t clamp_lo = 13;
    uint16_t clamp_hi_x = SCREEN_W - 1 - 12;
    uint16_t clamp_hi_y = SCREEN_H - 1 - 12;

    BSP_TS_GetState(&g_ts);

    if (g_ts.touchDetected > 0)
    {
        uint16_t x = g_ts.touchX[0];
        uint16_t y = g_ts.touchY[0];
        if (x < clamp_lo) x = clamp_lo;
        if (y < clamp_lo) y = clamp_lo;
        if (x > clamp_hi_x) x = clamp_hi_x;
        if (y > clamp_hi_y) y = clamp_hi_y;

        if (x != g_last_x || y != g_last_y)
        {
            printf("Touch: X=%u Y=%u\r\n", (unsigned)x, (unsigned)y);
            g_last_x = x;
            g_last_y = y;
        }

        /* white ring + red dot so the marker is visible on any background */
        BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
        BSP_LCD_FillCircle(x, y, 12);
        BSP_LCD_SetTextColor(LCD_COLOR_RED);
        BSP_LCD_FillCircle(x, y, 8);
    }
    else if (g_last_x != 0xFFFF)
    {
        printf("Touch: released\r\n");
        g_last_x = 0xFFFF;
        g_last_y = 0xFFFF;
    }
}

/* Delay helper that keeps the touch marker + status live. */
static void delay_with_overlay(uint32_t ms)
{
    uint32_t start = HAL_GetTick();
    do
    {
        touch_poll();
        status_update();
        HAL_Delay(20);
    } while (HAL_GetTick() - start < ms);
}

/* ------------------------------------------------------------------------ */
/* Floating & bouncing shapes.                                               */
typedef enum { SHAPE_SQUARE, SHAPE_CIRCLE, SHAPE_TRIANGLE } shape_kind_t;

typedef struct
{
    shape_kind_t kind;
    int          x, y;    /* center */
    int          vx, vy;
    int          size;    /* half size / radius */
    uint32_t     color;
} shape_t;

static const uint32_t shape_palette[] = {
    LCD_COLOR_RED, LCD_COLOR_GREEN, LCD_COLOR_BLUE, LCD_COLOR_YELLOW,
    LCD_COLOR_CYAN, LCD_COLOR_MAGENTA, LCD_COLOR_WHITE, LCD_COLOR_ORANGE,
};

static void init_shapes(shape_t *s, int n)
{
    static const shape_kind_t kinds[3] = { SHAPE_SQUARE, SHAPE_CIRCLE, SHAPE_TRIANGLE };
    for (int i = 0; i < n; i++)
    {
        s[i].kind  = kinds[i % 3];
        s[i].x     = 40 + (i * 103) % (SCREEN_W - 80);
        s[i].y     = 40 + (i * 197) % (ANIM_H - 120);
        s[i].vx    = (i % 2 ? 1 : -1) * (3 + (i % 4));
        s[i].vy    = (i % 3 ? 1 : -1) * (3 + (i % 5));
        s[i].size  = 20 + (i % 4) * 8;
        s[i].color = shape_palette[i % (sizeof(shape_palette) / sizeof(shape_palette[0]))];
    }
}

static void draw_shape(const shape_t *s)
{
    BSP_LCD_SetTextColor(s->color);
    switch (s->kind)
    {
    case SHAPE_SQUARE:
        BSP_LCD_FillRect((uint16_t)(s->x - s->size), (uint16_t)(s->y - s->size),
                         (uint16_t)(2 * s->size), (uint16_t)(2 * s->size));
        break;
    case SHAPE_CIRCLE:
        BSP_LCD_FillCircle((uint16_t)s->x, (uint16_t)s->y, (uint16_t)s->size);
        break;
    default:
    {
        int r = s->size;
        int x0 = s->x,      y0 = s->y - r;              /* top            */
        int x1 = s->x - r,  y1 = s->y + (r * 8) / 10;   /* bottom left    */
        int x2 = s->x + r,  y2 = s->y + (r * 8) / 10;   /* bottom right   */
        BSP_LCD_DrawLine((uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)y1);
        BSP_LCD_DrawLine((uint16_t)x1, (uint16_t)y1, (uint16_t)x2, (uint16_t)y2);
        BSP_LCD_DrawLine((uint16_t)x2, (uint16_t)y2, (uint16_t)x0, (uint16_t)y0);
        break;
    }
    }
}

static void step_shape(shape_t *s)
{
    s->x += s->vx;
    s->y += s->vy;
    int r = s->size;
    if (s->x - r < 0)             { s->x = r;                s->vx = -s->vx; }
    if (s->x + r > SCREEN_W - 1)  { s->x = SCREEN_W - 1 - r; s->vx = -s->vx; }
    if (s->y - r < 0)             { s->y = r;                s->vy = -s->vy; }
    if (s->y + r > ANIM_H - 1)    { s->y = ANIM_H - 1 - r;   s->vy = -s->vy; }
}

static void shapes_demo(uint32_t ms)
{
    enum { N = 10 };
    shape_t shapes[N];
    init_shapes(shapes, N);

    paint_band();

    uint32_t start = HAL_GetTick();
    do
    {
        BSP_LCD_SetTextColor(BACK_COLOR);
        BSP_LCD_FillRect(0, 0, SCREEN_W, ANIM_H);   /* clear animation area */
        for (int i = 0; i < N; i++)
        {
            step_shape(&shapes[i]);
            draw_shape(&shapes[i]);
        }
        fps_frame();
        touch_poll();
        status_update();
    } while (HAL_GetTick() - start < ms);
}

/* ------------------------------------------------------------------------ */
/* Pure colors, one after the other.                                         */
static void colors_demo(uint32_t ms_per_color)
{
    static const uint32_t colors[] = {
        LCD_COLOR_RED, LCD_COLOR_GREEN, LCD_COLOR_BLUE, LCD_COLOR_YELLOW,
        LCD_COLOR_CYAN, LCD_COLOR_MAGENTA, LCD_COLOR_WHITE, LCD_COLOR_BLACK,
    };

    for (unsigned i = 0; i < sizeof(colors) / sizeof(colors[0]); i++)
    {
        BSP_LCD_Clear(colors[i]);
        paint_band();
        printf("[LCD] pure color %u\r\n", (unsigned)i + 1);
        delay_with_overlay(ms_per_color);
    }
}

/* ------------------------------------------------------------------------ */
/* Animated gradient: hue sweeps the full color wheel over `ms`. The band is
 * skipped - the gradient only fills the animation area (direct FB writes).  */
static uint32_t hsv_to_argb8888(int h, int s, int v)
{
    /* h: 0..3600 (0.1 deg), s/v: 0..255 */
    int region = (h / 600) % 6;
    int fpart  = h % 600;
    int p = v * (255 - s) / 255;
    int q = v * (255 - (s * fpart) / 600) / 255;
    int t = v * (255 - (s * (600 - fpart)) / 600) / 255;
    int r, g, b;
    switch (region)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default:r = v; g = p; b = q; break;
    }
    return 0xFF000000UL | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void draw_gradient(int hue_a, int hue_b)
{
    volatile uint32_t *fb = (volatile uint32_t *)FB_ADDR;

    for (int y = 0; y < ANIM_H; y++)
    {
        int frac = y * 1000 / ANIM_H;               /* 0..1000 across height */
        int hue  = hue_a + (hue_b - hue_a) * frac / 1000;
        uint32_t c = hsv_to_argb8888(hue, 255, 255);
        for (int x = 0; x < SCREEN_W; x++)
        {
            fb[y * SCREEN_W + x] = c;
        }
    }
}

static void gradient_demo(uint32_t ms)
{
    paint_band();

    uint32_t start = HAL_GetTick();
    uint32_t t = 0;
    do
    {
        int hue_a = (int)(t * 3600 / ms);           /* full sweep over ms */
        int hue_b = hue_a + 1800;                   /* complementary      */
        if (hue_b >= 3600) hue_b -= 3600;
        draw_gradient(hue_a, hue_b);
        fps_frame();
        touch_poll();
        status_update();
        HAL_Delay(16);                              /* pace to ~60 fps    */
        t = HAL_GetTick() - start;
    } while (t < ms);
}

/* ------------------------------------------------------------------------ */
/* LED test: toggle the three on-board LEDs while the screen stays live.     */
static void led_test(void)
{
    __HAL_RCC_GPIOJ_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = LD1_PIN;
    HAL_GPIO_Init(LD1_PORT, &gpio);
    gpio.Pin = LD2_PIN;
    HAL_GPIO_Init(LD2_PORT, &gpio);
    gpio.Pin = LD3_PIN;
    HAL_GPIO_Init(LD3_PORT, &gpio);

    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetFont(&Font24);
    BSP_LCD_DisplayStringAt(0, ANIM_H / 2 - 20, (uint8_t *)"LED test",
                            CENTER_MODE);

    printf("[LCD] LEDs ON\r\n");
    HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LD2_PORT, LD2_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LD3_PORT, LD3_PIN, GPIO_PIN_SET);
    delay_with_overlay(3000);

    printf("[LCD] LEDs OFF\r\n");
    HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LD2_PORT, LD2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LD3_PORT, LD3_PIN, GPIO_PIN_RESET);
    delay_with_overlay(3000);
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    HAL_Init();
    Board_Init();   /* inits the on-board SDRAM (LCD framebuffer) too */

    printf("\r\n=== lcd_touch_test (QSPI) on STM32F769NI @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);

    /* MIPI DSI video-mode LCD (OTM8009A 800x480) + SDRAM framebuffer. */
    if (BSP_LCD_Init() != LCD_OK)
    {
        printf("LCD: init FAILED\r\n");
        while (1)
        {
        }
    }
    BSP_LCD_LayerDefaultInit(0, FB_ADDR);
    BSP_LCD_Clear(LCD_COLOR_BLACK);
    printf("LCD: %u x %u, MIPI DSI video mode (OTM8009A)\r\n",
           (unsigned)BSP_LCD_GetXSize(), (unsigned)BSP_LCD_GetYSize());

    /* FT6206 capacitive touch controller on I2C4. */
    if (BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize()) != TS_OK)
    {
        printf("TS: init FAILED (FT6206 not found?)\r\n");
        while (1)
        {
        }
    }
    printf("TS: FT6206 OK\r\n");

    paint_band();

    while (1)
    {
        printf("[LCD] phase: shapes\r\n");
        shapes_demo(5000);

        printf("[LCD] phase: pure colors\r\n");
        colors_demo(4000);

        printf("[LCD] phase: gradient\r\n");
        gradient_demo(5000);

        printf("[LCD] phase: LED test\r\n");
        led_test();
    }
}
