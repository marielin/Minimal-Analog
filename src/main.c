#include <pebble.h>

#define ANTIALIASING true

#define TICK_RADIUS            3
#define HAND_MARGIN_S          14
#define HAND_MARGIN_M          20
#define HAND_MARGIN_H          42
#define SHADOW_OFFSET          2

#define ANIMATION_DURATION     750
#define ANIMATION_DELAY        0

#define DATE_RECT_RIGHT GRect(90, 77, 70, 40)
#define DATE_RECT_TOP GRect(50, 48, 80, 40)
#define DATE_RECT_BOTTOM GRect(50, 118, 80, 40)

#define LOGO_RECT GRect(80, 140, 19, 6)

#define DATE_POS_RIGHT 0
#define DATE_POS_BOTTOM 1
#define DATE_POS_TOP 2

typedef struct {
	int hours;
	int minutes;
	int seconds;
} Time;

typedef struct {
	int day_of_week;
	int month;
	int day;
} Date;

typedef enum {
	RIGHT,
	LEFT,
	BOTTOM
} Date_position;

static Window *s_main_window;
static Layer *bg_canvas_layer, *s_canvas_layer, *shadow_canvas_layer;
static TextLayer *s_date_layer;

static GBitmap *s_logo;
static BitmapLayer *s_logo_layer;

static GPoint s_center, second_hand_outer, minute_hand_outer, hour_hand_outer;
static Time s_last_time;
static int animpercent = 0, whwidth = 7, shwidth = 2;
static bool s_animating = false, shadows = true, debug = false, btvibe = true;

static int date_position = DATE_POS_RIGHT;

static GColor gcolorbg;
static GColor gcolors;
static GColor gcolorm;
static GColor gcolorh;
static GColor gcolorp;
static GColor gcolorshadow;
static GColor gcolort;

static char date_buffer[16] = "Mon Day ##";

/*************************** AnimationImplementation **************************/

static void animation_started(Animation *anim, void *context) {
	s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
	s_animating = false;
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
	Animation *anim = animation_create();
	animation_set_duration(anim, duration);
	animation_set_delay(anim, delay);
	animation_set_curve(anim, AnimationCurveEaseInOut);
	animation_set_implementation(anim, implementation);
	if (handlers) {
		animation_set_handlers(anim, (AnimationHandlers) {
			.started = animation_started,
			.stopped = animation_stopped
		}, NULL);
	}
	animation_schedule(anim);
}

/************************************ UI **************************************/

static int32_t get_angle_for_second(int second) {
	// Progress through 60 seconds, out of 360 degrees
	return (second * 360) / 60;
}

static int32_t get_angle_for_minute(int minute, int second) {
	// Progress through 60 minutes, out of 360 degrees
	return ((minute * 360) / 60) + (get_angle_for_second(second) / 60);
}

static int32_t get_angle_for_hour(int hour, int minute, int second) {
	// Progress through 12 hours, out of 360 degrees
	return ((hour * 360) / 12) + get_angle_for_minute(minute, second) / 12;
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
	// Store time
	// debug = true;
	if (debug) {
		// use dummy time for emulator
		s_last_time.seconds = 20;
		s_last_time.hours = 6;
		s_last_time.minutes = 15;
	} else {
		s_last_time.hours = tick_time->tm_hour;
		s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
		s_last_time.minutes = tick_time->tm_min;
		s_last_time.seconds = tick_time->tm_sec;
	}

	// Redraw time
	if (s_canvas_layer) {
		layer_mark_dirty(s_canvas_layer);
	}

	if (changed & MINUTE_UNIT) {
		date_position = DATE_POS_RIGHT;
		int minute_angle = (int)get_angle_for_minute(s_last_time.minutes, s_last_time.seconds);
		int hour_angle = (int)get_angle_for_hour(s_last_time.hours, s_last_time.minutes, s_last_time.seconds);

		if (((minute_angle > 70) && (minute_angle < 110)) || ((hour_angle > 70) && (hour_angle < 110))) {
			date_position++;

			if (((minute_angle > 150) && (minute_angle < 210)) || ((hour_angle > 150) && (hour_angle < 210))) {
				date_position++;
			}
		}

		if (s_date_layer) {
			if (date_position == DATE_POS_RIGHT) {
				text_layer_set_text_alignment(s_date_layer, GTextAlignmentRight);
				layer_set_frame(text_layer_get_layer(s_date_layer), DATE_RECT_RIGHT);
			} else if (date_position == DATE_POS_TOP) {
				text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
				layer_set_frame(text_layer_get_layer(s_date_layer), DATE_RECT_TOP);
			} else if (date_position == DATE_POS_BOTTOM) {
				text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
				layer_set_frame(text_layer_get_layer(s_date_layer), DATE_RECT_BOTTOM);
			}
		}
	}

	if (changed & DAY_UNIT) {
		strftime(date_buffer, sizeof(date_buffer), "%b %d", tick_time);

		if (s_date_layer) {
			text_layer_set_text(s_date_layer, date_buffer);
		}
	}
}

static void handle_bluetooth(bool connected) {
	if (btvibe && !connected) {
		static uint32_t const segments[] = { 200, 200, 50, 150, 200 };
		VibePattern pat = {
			.durations = segments,
			.num_segments = ARRAY_LENGTH(segments),
		};
		vibes_enqueue_custom_pattern(pat);
	}
}

static void bg_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	graphics_context_set_fill_color(ctx, gcolorbg);
	graphics_fill_rect(ctx, bounds, 0, GCornerNone);
	graphics_context_set_antialiased(ctx, ANTIALIASING);

	/*
	graphics_context_set_stroke_color(ctx, gcolort);
	for(int i = 0; i < 60; i++) {
		int angle = (i * 360) / 60;

		GRect outer_width = grect_inset(bounds, GEdgeInsets(0));
		GPoint outer_edge = gpoint_from_polar(outer_width, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(angle));

		if (i % 5 != 0) {
			graphics_context_set_stroke_width(ctx, 1);
			GRect inner_width = grect_inset(bounds, GEdgeInsets(8));
			GPoint inner_edge = gpoint_from_polar(inner_width, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(angle));
			graphics_draw_line(ctx, inner_edge, outer_edge);
		} else {
			graphics_context_set_stroke_width(ctx, 2);
			GRect inner_width = grect_inset(bounds, GEdgeInsets(10));
			GPoint inner_edge = gpoint_from_polar(inner_width, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(angle));
			graphics_draw_line(ctx, inner_edge, outer_edge);
		}
	}
	*/
}

static void shadow_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	GRect bounds_h = bounds;
	bounds_h.size.w = bounds_h.size.h;
	bounds_h.origin.x -= (bounds_h.size.w-bounds.size.w)/2;
	int maxradius = bounds_h.size.w;
	if (bounds_h.size.h < maxradius) { maxradius = bounds_h.size.h; }
	maxradius /= 2;
	int animradius = maxradius-((maxradius*animpercent)/100);

	int outer_m = animradius+HAND_MARGIN_M;
	int outer_h = animradius+HAND_MARGIN_H;
	int outer_s = animradius+HAND_MARGIN_S;

	if (outer_m < HAND_MARGIN_M) {
		outer_m = HAND_MARGIN_M;
	}
	if (outer_h < HAND_MARGIN_H) {
		outer_h = HAND_MARGIN_H;
	}
	if (outer_s < HAND_MARGIN_S) {
		outer_s = HAND_MARGIN_S;
	}
	if (outer_m > maxradius) {
		outer_m = maxradius;
	}
	if (outer_h > maxradius) {
		outer_h = maxradius;
	}
	if (outer_s > maxradius) {
		outer_s = maxradius;
	}

	GRect bounds_mo = grect_inset(bounds_h, GEdgeInsets(outer_m));
	GRect bounds_ho = grect_inset(bounds_h, GEdgeInsets(outer_h));
	GRect bounds_so = grect_inset(bounds_h, GEdgeInsets(outer_s));
	graphics_context_set_antialiased(ctx, ANTIALIASING);

	// Use current time while animating
	Time mode_time = s_last_time;

	// Adjust for minutes through the hour
	float hour_deg = get_angle_for_hour(mode_time.hours, mode_time.minutes, mode_time.seconds);
	float minute_deg = get_angle_for_minute(mode_time.minutes, mode_time.seconds);
	float second_deg = get_angle_for_second(mode_time.seconds);

	second_hand_outer = gpoint_from_polar(bounds_so, GOvalScaleModeFillCircle, DEG_TO_TRIGANGLE(second_deg));
	minute_hand_outer = gpoint_from_polar(bounds_mo, GOvalScaleModeFillCircle, DEG_TO_TRIGANGLE(minute_deg));
	hour_hand_outer = gpoint_from_polar(bounds_ho, GOvalScaleModeFillCircle, DEG_TO_TRIGANGLE(hour_deg));

	if (shadows) {
		graphics_context_set_stroke_color(ctx, gcolorshadow);
		graphics_context_set_stroke_width(ctx, whwidth);

		hour_hand_outer.y += SHADOW_OFFSET;
		s_center.y += SHADOW_OFFSET;
		graphics_draw_line(ctx, s_center, hour_hand_outer);

		minute_hand_outer.y += SHADOW_OFFSET+1;
		s_center.y += 1;
		graphics_draw_line(ctx, s_center, minute_hand_outer);

		second_hand_outer.y += SHADOW_OFFSET+2;
		s_center.y += 1;
		graphics_context_set_stroke_width(ctx, shwidth);
		graphics_draw_line(ctx, s_center, second_hand_outer);

		hour_hand_outer.y -= SHADOW_OFFSET;
		minute_hand_outer.y -= SHADOW_OFFSET+1;
		second_hand_outer.y -= SHADOW_OFFSET+2;

		s_center.y -= SHADOW_OFFSET+2;
	}

	// if (animpercent < 100) {
	// 	layer_set_frame(text_layer_get_layer(s_date_layer), GRect(0, 180 - (60 * animpercent) / 100, 180, 40));
	// }
}

static void update_proc(Layer *layer, GContext *ctx) {
	graphics_context_set_stroke_color(ctx, gcolorh);
	graphics_context_set_stroke_width(ctx, whwidth);
	graphics_draw_line(ctx, s_center, hour_hand_outer);

	graphics_context_set_stroke_color(ctx, gcolorm);
	graphics_context_set_stroke_width(ctx, whwidth);
	graphics_draw_line(ctx, s_center, minute_hand_outer);

	graphics_context_set_stroke_color(ctx, gcolors);
	graphics_context_set_stroke_width(ctx, shwidth);
	graphics_draw_line(ctx, s_center, second_hand_outer);

	graphics_context_set_fill_color(ctx, gcolorp);
	graphics_fill_circle(ctx, s_center, whwidth/4);
}

static void window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect window_bounds = layer_get_bounds(window_layer);

	s_center = grect_center_point(&window_bounds);
	s_center.x -= 1;
	s_center.y -= 1;

	s_logo = gbitmap_create_with_resource(RESOURCE_ID_LOGO);
	s_logo_layer = bitmap_layer_create(LOGO_RECT);

	bitmap_layer_set_compositing_mode(s_logo_layer, GCompOpSet);
	bitmap_layer_set_bitmap(s_logo_layer, s_logo);

	bg_canvas_layer = layer_create(window_bounds);
	s_canvas_layer = layer_create(window_bounds);
	shadow_canvas_layer = layer_create(window_bounds);
	s_date_layer = text_layer_create(DATE_RECT_RIGHT);

	text_layer_set_text(s_date_layer, date_buffer);
	text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_color(s_date_layer, GColorDarkGray);
	text_layer_set_background_color(s_date_layer, GColorClear);

	if (date_position == DATE_POS_RIGHT) {
		text_layer_set_text_alignment(s_date_layer, GTextAlignmentRight);
		layer_set_frame(text_layer_get_layer(s_date_layer), DATE_RECT_RIGHT);
	} else if (date_position == DATE_POS_TOP) {
		text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
		layer_set_frame(text_layer_get_layer(s_date_layer), DATE_RECT_TOP);
	} else if (date_position == DATE_POS_BOTTOM) {
		text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
		layer_set_frame(text_layer_get_layer(s_date_layer), DATE_RECT_BOTTOM);
	}

	layer_set_update_proc(bg_canvas_layer, bg_update_proc);
	layer_set_update_proc(shadow_canvas_layer, shadow_update_proc);
	layer_set_update_proc(s_canvas_layer, update_proc);
	layer_add_child(window_layer, bg_canvas_layer);
	layer_add_child(bg_canvas_layer, shadow_canvas_layer);
	layer_add_child(bg_canvas_layer, text_layer_get_layer(s_date_layer));
	layer_add_child(bg_canvas_layer, bitmap_layer_get_layer(s_logo_layer));
	layer_add_child(bg_canvas_layer, s_canvas_layer);
}

static void window_unload(Window *window) {
	layer_destroy(bg_canvas_layer);
	layer_destroy(s_canvas_layer);
	text_layer_destroy(s_date_layer);
	gbitmap_destroy(s_logo);
	bitmap_layer_destroy(s_logo_layer);
}

/*********************************** App **************************************/

static int anim_percentage(AnimationProgress dist_normalized, int max) {
	return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}

static void radius_update(Animation *anim, AnimationProgress dist_normalized) {
	animpercent = anim_percentage(dist_normalized, 100);
	layer_mark_dirty(s_canvas_layer);
}

static void init() {
	srand(time(NULL));

  // keep lit only in emulator
	if (watch_info_get_model()==WATCH_INFO_MODEL_UNKNOWN) {
		debug = true;
	}

	gcolorbg = GColorWhite;
	gcolors = GColorBulgarianRose;
	gcolorm = GColorBlack;
	gcolorh = GColorBlack;
	gcolorp = GColorWhite;
	gcolorshadow = GColorLightGray;
	gcolort = GColorDarkGray;

	time_t t = time(NULL);
	struct tm *time_now = localtime(&t);
	tick_handler(time_now, DAY_UNIT | MINUTE_UNIT);

	s_main_window = window_create();
	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});
	window_stack_push(s_main_window, true);

	tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

	if (debug) {
		light_enable(true);
	}

	handle_bluetooth(connection_service_peek_pebble_app_connection());

	connection_service_subscribe((ConnectionHandlers) {
		.pebble_app_connection_handler = handle_bluetooth
	});

	// app_message_register_inbox_received(inbox_received_handler);
	// app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

    // Prepare animations
	AnimationImplementation radius_impl = {
		.update = radius_update
	};
	animate(ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);
}

static void deinit() {
	window_destroy(s_main_window);
}

int main() {
	init();
	app_event_loop();
	deinit();
}
