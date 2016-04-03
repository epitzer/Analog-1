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
#define UPDATE_INTERVAL_SEC 20

static Window *main_window;
static Layer *canvas_layer;
static AppTimer *timer;
static struct tm *current_time;
static BatteryChargeState current_battery;
static bool current_connection;
static GColor colors[11];

static void connection_handler(bool is_connected) {
  current_connection = is_connected;
  if (is_connected) {
    APP_LOG(APP_LOG_LEVEL_INFO, "bluetooth is now connected");
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "bluetooth is now disconnected");
    if (current_time->tm_hour > 6 && current_time->tm_hour < 23) // TODO: check quiet time status
      vibes_short_pulse();
  }
  layer_mark_dirty(canvas_layer);
}

static void battery_handler(BatteryChargeState battery_charge_state) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "battery charge state changed to %d", battery_charge_state.charge_percent);
  current_battery = battery_charge_state;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "copied battery charge state");
}

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
  timer = app_timer_register((UPDATE_INTERVAL_SEC-current_time->tm_sec%UPDATE_INTERVAL_SEC)*1000, timer_callback, NULL); // re-schedule
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  layer_mark_dirty(canvas_layer);
}  

static void canvas_update_proc(Layer *layer, GContext *gc) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_stroke_color(gc, GColorRed);
  graphics_context_set_fill_color(gc, GColorBlue);
  graphics_context_set_stroke_width(gc, 5);
  graphics_context_set_antialiased(gc, true); // default
  
  GPoint center = GPoint(bounds.origin.x+bounds.size.w/2, bounds.origin.y+bounds.size.h/2);
  uint16_t radius = MIN(bounds.size.w, bounds.size.h)/2;
  APP_LOG(APP_LOG_LEVEL_INFO, "time %02d:%02d:%02d", current_time->tm_hour, current_time->tm_min, current_time->tm_sec);
  int32_t hour_angle = TRIG_MAX_ANGLE*((current_time->tm_hour%12)/12.0+current_time->tm_min/720.0-0.25);
  int32_t minute_angle = TRIG_MAX_ANGLE*(current_time->tm_min/60.0+current_time->tm_sec/3600.0-0.25);
  int32_t second_angle = TRIG_MAX_ANGLE*((current_time->tm_sec/UPDATE_INTERVAL_SEC*UPDATE_INTERVAL_SEC)/60.0);
  APP_LOG(APP_LOG_LEVEL_INFO, "time angles %d° %d° %d°", (int)(360.0*hour_angle/TRIG_MAX_ANGLE), (int)(360.0*minute_angle/TRIG_MAX_ANGLE), (int)(360.0*second_angle/TRIG_MAX_ANGLE));
  
  GPoint index_tip(int32_t angle, double radius_percentage) {
    return GPoint(center.x+radius*radius_percentage*cos_lookup(angle)/TRIG_MAX_RATIO, center.y+radius*radius_percentage*sin_lookup(angle)/TRIG_MAX_RATIO);
  }
  
  // dial
  graphics_context_set_stroke_color(gc, GColorWhite);
  graphics_context_set_fill_color(gc, GColorWhite);
  graphics_context_set_stroke_width(gc, 1);
  for (int i = 1; i<=12; i++) {
    double width = i % 3 == 0 ? 0.8 : 0.33;
    int32_t angle1 = TRIG_MAX_ANGLE*(double)(i/12.0-width/180.0);
    int32_t angle2 = TRIG_MAX_ANGLE*(double)(i/12.0+width/180.0);
    graphics_fill_radial(gc, bounds, GOvalScaleModeFitCircle, i % 3 == 0 ? TICK_LENGTH*1.2 : TICK_LENGTH*0.8, angle1, angle2);
  }
  
  // hour
  graphics_context_set_stroke_color(gc, GColorRed);
  graphics_context_set_stroke_width(gc, HOUR_THICKNESS);
  graphics_draw_line(gc, center, index_tip(hour_angle, HOUR_LENGTH));
  
  // minute
  graphics_context_set_stroke_color(gc, GColorWhite);
  graphics_context_set_stroke_width(gc, MINUTE_THICKNESS);
  graphics_draw_line(gc, center, index_tip(minute_angle, MINUTE_LENGTH));
  
  // second
  if (UPDATE_INTERVAL_SEC == 1) {
    graphics_context_set_stroke_color(gc, GColorYellow);
    graphics_context_set_stroke_width(gc, 1);
    graphics_draw_line(gc, center, index_tip(second_angle, SECOND_LENGTH));
  } else if (UPDATE_INTERVAL_SEC < 60) {
    graphics_context_set_fill_color(gc, GColorWindsorTan);
    for (int i = 0; i<UPDATE_INTERVAL_SEC/5; i++) {
      graphics_fill_radial(gc, bounds, GOvalScaleModeFitCircle, 1, second_angle+TRIG_MAX_ANGLE/60*(1+i*5), second_angle+TRIG_MAX_ANGLE/60*(4+i*5));
    }
  }
  
  // hub / bluetooth
  graphics_context_set_stroke_color(gc, GColorWhite);
  graphics_context_set_stroke_width(gc, 2);
  graphics_draw_circle(gc, center, 3);
  graphics_context_set_fill_color(gc, current_connection ? GColorDukeBlue : GColorMelon);
  graphics_fill_circle(gc, center, 3);
  
  // date
  char text[12];
  strftime(text, sizeof(text), "%b %d\n%a", current_time);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GRect text_bounds = GRect(bounds.origin.x, bounds.origin.y-7, bounds.size.w / 2, bounds.size.h);
  //GSize text_size = graphics_text_layout_get_content_size(text, font, bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter);
  graphics_context_set_text_color(gc, GColorCeleste);
  graphics_draw_text(gc, text, font, text_bounds, GTextOverflowModeWordWrap,  GTextAlignmentLeft, NULL);
  
  // battery
  graphics_context_set_stroke_width(gc, 1);
  int n_segments = (current_battery.charge_percent+4)/10;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "nr of battery segments %d", n_segments);
  int segment_length = bounds.size.w/10;
  for (int i = 0; i<n_segments; i++) {
    if (i == 0) graphics_context_set_stroke_color(gc, GColorRed);
    else if (i == 1) graphics_context_set_stroke_color(gc, GColorChromeYellow);
    else if (i == 2) graphics_context_set_stroke_color(gc, GColorYellow);
    else if (i < 8) graphics_context_set_stroke_color(gc, GColorGreen);
    else graphics_context_set_stroke_color(gc, GColorMintGreen);
    graphics_draw_line(gc,
                       GPoint(bounds.origin.x+segment_length*i,       bounds.origin.y+bounds.size.h-1),
                       GPoint(bounds.origin.x+segment_length*(i+1)-2, bounds.origin.y+bounds.size.h-1));
  }
  if (n_segments == 0) {
    graphics_context_set_stroke_color(gc, GColorRed);
    graphics_context_set_stroke_width(gc, 4);
    graphics_draw_line(gc,
                       GPoint(bounds.origin.x, bounds.origin.y+bounds.size.h-1),
                       GPoint(bounds.origin.x, bounds.origin.y+bounds.size.h-1));
  }
  if (current_battery.is_plugged) {
    if (current_battery.is_charging) 
      graphics_context_set_stroke_color(gc, GColorPictonBlue);
    else
      graphics_context_set_stroke_color(gc, GColorBrilliantRose);
    graphics_context_set_stroke_width(gc, 4);
    graphics_draw_line(gc,
                       GPoint(bounds.origin.x+bounds.size.w-1, bounds.origin.y+bounds.size.h-1),
                       GPoint(bounds.origin.x+bounds.size.w-1, bounds.origin.y+bounds.size.h-1));
  }
  
  // steps
  int steps = health_service_sum_today(HealthMetricStepCount);
  uint32_t n_step_segments = steps/1000;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "%d steps today -> %d segments, %d pixels", steps, (int)n_step_segments, (int)n_step_segments*12);
  graphics_context_set_stroke_width(gc, 1);
  for (uint32_t i = 0; i<n_step_segments; i++) {
    graphics_context_set_stroke_color(gc, colors[i < sizeof(colors) ? i : (sizeof(colors)-1)]);
    graphics_draw_line(gc,
                       GPoint(bounds.origin.x+bounds.size.w-1, bounds.origin.y+bounds.size.h-1-i*12),
                       GPoint(bounds.origin.x+bounds.size.w-1, bounds.origin.y+bounds.size.h-1-i*12-10));
  }
  graphics_context_set_stroke_width(gc, 2);
  graphics_context_set_stroke_color(gc, GColorBrightGreen);
  graphics_draw_line(gc,
                     GPoint(bounds.origin.x+bounds.size.w-1, bounds.origin.y+bounds.size.h-1-10*12),
                     GPoint(bounds.origin.x+bounds.size.w-1, bounds.origin.y+bounds.size.h-1-10*12));
}

static void main_window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "loading main window...");
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  canvas_layer = layer_create(bounds);
  layer_set_update_proc(canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, canvas_layer);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "peeking at battery charge state...");
  battery_handler(battery_state_service_peek());
  APP_LOG(APP_LOG_LEVEL_DEBUG, "subscribing to battery state service...");
  battery_state_service_subscribe(battery_handler);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "subscribing to app connection service...");
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = connection_handler,
    .pebblekit_connection_handler = NULL
  });
  APP_LOG(APP_LOG_LEVEL_DEBUG, "getting current bluetooth connection status...");
  current_connection = connection_service_peek_pebble_app_connection();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "updating time...");
  update_time();
  if (UPDATE_INTERVAL_SEC == 1) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "registering update timer at second interval");
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  } else if (UPDATE_INTERVAL_SEC >= 60) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "registering update timer at minute interval");
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  } else {
    int update_time = (UPDATE_INTERVAL_SEC-current_time->tm_sec%UPDATE_INTERVAL_SEC);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "registering update timer at next %d second step in %ds...", UPDATE_INTERVAL_SEC, update_time);
    timer = app_timer_register(update_time*1000, timer_callback, NULL);
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "loading complete.");
}

static void main_window_unload(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "unloading main window...");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "unsubscribing from app connection service...");
  connection_service_unsubscribe();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "unregistering battery state service...");
  battery_state_service_unsubscribe();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "canceling update timer...");
  app_timer_cancel(timer);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "destroying canvas...");
  layer_destroy(canvas_layer);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "unloading complete.");
}

static void init() {
  APP_LOG(APP_LOG_LEVEL_INFO, "init");
  main_window = window_create();
  window_set_background_color(main_window, GColorBlack);
  window_set_window_handlers(main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(main_window, true); // animated
  APP_LOG(APP_LOG_LEVEL_DEBUG, "init complete.");
}

static void deinit() {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "de-init...");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "destroying main window...");
  window_destroy(main_window);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "de-init complete.");
}

int main() {
  colors[0] = GColorRed;
  colors[1] = GColorOrange;
  colors[2] = GColorChromeYellow;
  colors[3] = GColorLimerick;
  colors[4] = GColorBrass;
  colors[5] = GColorKellyGreen;
  colors[6] = GColorIslamicGreen;
  colors[7] = GColorMalachite;
  colors[8] = GColorMediumAquamarine;
  colors[9] = GColorElectricBlue;
  colors[10] = GColorCeleste;
  init();
  app_event_loop();
  deinit();
}