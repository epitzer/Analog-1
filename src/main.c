#include <pebble.h>
#define PI 3.14159265
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

#define TICK_LENGTH 10.0
#define TICK_THICKNESS 3.0
#define HOUR_LENGTH 0.45
#define HOUR_THICKNESS 6.0
#define MINUTE_LENGTH 0.75
#define MINUTE_THICKNESS 2.0
#define SECOND_LENGTH 0.9
#define SECOND_THICKNESS 1.0

static Window *main_window;
static Layer *canvas_layer;
static AppTimer *timer;
struct tm *current_time;

static void update_time() {
  time_t temp = time(NULL); 
  current_time = localtime(&temp);

  // static char buffer[8];
  // strftime(buffer, sizeof(buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  // text_layer_set_text(time_layer, buffer);
  //hour = tick_time->tm_hour;
  //minute = tick_time->tm_min;
  //second = tick_time->tm_sec;
}

static void timer_callback(void *context) {
  update_time();
  layer_mark_dirty(canvas_layer);
  timer = app_timer_register(5000-current_time->tm_sec%5*1000, timer_callback, NULL); // re-schedule
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  //update_time();
  //layer_mark_dirty(canvas_layer);
}  

static void canvas_update_proc(Layer *layer, GContext *gc) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_stroke_color(gc, GColorRed);
  graphics_context_set_fill_color(gc, GColorBlue);
  graphics_context_set_stroke_width(gc, 5);
  graphics_context_set_antialiased(gc, true); // default
  
  GPoint center = GPoint(bounds.origin.x+bounds.size.w/2, bounds.origin.y+bounds.size.h/2);
  uint16_t radius = MIN(bounds.size.w, bounds.size.h)/2;
  //APP_LOG(APP_LOG_LEVEL_INFO, "time %02d:%02d:%02d", hour, minute, second);
  int32_t hour_angle = TRIG_MAX_ANGLE*(double)(current_time->tm_hour%12)/60+current_time->tm_min/720;
  int32_t minute_angle = TRIG_MAX_ANGLE*(double)current_time->tm_min/60+current_time->tm_sec/3600;
  int32_t second_angle = TRIG_MAX_ANGLE*(double)current_time->tm_sec/60;
  //APP_LOG(APP_LOG_LEVEL_INFO, "time angles %d° %d° %d°", (int)hour_angle, (int)minute_angle, (int)second_angle);
  
  GPoint index_tip(int32_t angle, double radius_percentage) {
    return GPoint(center.x+radius*radius_percentage*cos_lookup(angle)/TRIG_MAX_RATIO, center.y+radius*radius_percentage*sin_lookup(angle)/TRIG_MAX_RATIO);
  }
  
  graphics_context_set_stroke_color(gc, GColorWhite);
  graphics_context_set_fill_color(gc, GColorWhite);
  graphics_context_set_stroke_width(gc, 1);
  for (int i = 1; i<=12; i++) {
    double width = i % 3 == 0 ? 0.8 : 0.33;
    int32_t angle1 = TRIG_MAX_ANGLE*(double)(i/12.0-width/180);
    int32_t angle2 = TRIG_MAX_ANGLE*(double)(i/12.0+width/180);
    graphics_fill_radial(gc, bounds, GOvalScaleModeFitCircle, i % 3 == 0 ? TICK_LENGTH*1.2 : TICK_LENGTH*0.8, angle1, angle2);
  }
  
  graphics_context_set_stroke_color(gc, GColorRed);
  graphics_context_set_stroke_width(gc, HOUR_THICKNESS);
  graphics_draw_line(gc, center, index_tip(hour_angle, HOUR_LENGTH));
  
  graphics_context_set_stroke_color(gc, GColorWhite);
  graphics_context_set_stroke_width(gc, MINUTE_THICKNESS);
  graphics_draw_line(gc, center, index_tip(minute_angle, MINUTE_LENGTH));
  
  graphics_context_set_fill_color(gc, GColorYellow);
  // graphics_draw_line(gc, center, index_tip(second_angle, SECOND_LENGTH));
  graphics_fill_radial(gc, bounds, GOvalScaleModeFitCircle, TICK_LENGTH/4, second_angle+TRIG_MAX_ANGLE/60, second_angle+TRIG_MAX_ANGLE/60*4);
  
  graphics_context_set_stroke_color(gc, GColorWhite);
  graphics_context_set_fill_color(gc, GColorWhite);
  graphics_context_set_stroke_width(gc, 2);
  graphics_draw_circle(gc, center, 3);
  graphics_fill_circle(gc, center, 3);
  
  char text[12];
  strftime(text, sizeof(text), "%b %d\n%a", current_time);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GRect text_bounds = GRect(bounds.origin.x, bounds.origin.y-7, bounds.size.w / 2, bounds.size.h);
  GSize text_size = graphics_text_layout_get_content_size(text, font, bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter);
  graphics_context_set_stroke_color(gc, GColorCeleste);
  graphics_context_set_fill_color(gc, GColorCeleste);
  graphics_draw_text(gc, text, font, text_bounds, GTextOverflowModeWordWrap,  GTextAlignmentLeft, NULL);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  canvas_layer = layer_create(bounds);
  layer_set_update_proc(canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, canvas_layer);
  layer_mark_dirty(canvas_layer);
}

static void main_window_unload(Window *window) {
  layer_destroy(canvas_layer);
}

static void init() {
  main_window = window_create();
  window_set_background_color(main_window, GColorBlack);
  window_set_window_handlers(main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
  window_stack_push(main_window, true); // animated
  update_time();
  //tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  timer = app_timer_register(5000-current_time->tm_sec%5*1000, timer_callback, NULL);
}

static void deinit() {
  app_timer_cancel(timer);
  window_destroy(main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}