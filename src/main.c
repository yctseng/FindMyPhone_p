#include <pebble.h>
#include "animated_action_layer.h"
    
#define KEY_BUTTON_UP   0
#define KEY_BUTTON_DOWN 1
#define KEY_BUTTON_SELECT 2
#define KEY_QUERY_MODE 3

#define REPEAT_INTERVAL_MS 50

#define STRING_TITLE     "Find my phone"
#define STRING_VIBRATE   "Vibrating..."
#define STRING_STOP      "Stoped!!"
#define STRING_RINGING   "Ringing..."

#define KEY_MODE  1
#define STRING_MODE_INIT "MODE_INIT"
#define STRING_MODE_STOP "MODE_STOP"
#define STRING_MODE_VIBRATE "MODE_VIBRATE"
#define STRING_MODE_RINGING "MODE_RINGING"

#define MODE_INIT    0
#define MODE_STOP    1
#define MODE_VIBRATE 2
#define MODE_RINGING 3

static int s_current_mode = MODE_INIT;
static int s_last_mode = MODE_STOP;

static Window *s_main_window;
static TextLayer *s_output_layer;
static TextLayer *s_error_msg_layer;
static TextLayer *s_time_layer;

static ActionBarLayer *s_action_bar;
static GBitmap *s_icon_vibrate, *s_icon_stop, *s_icon_ringtone;

static bool s_is_standby = false;
static AppTimer *s_standby_timer = NULL;
static AppTimer *s_init_update_timer = NULL;
#define STANDBY_TIMEOUT 3000
#define INIT_UPDATE_TIMEOUT 500

static void send(int key, int value) {
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    dict_write_int(iter, key, &value, sizeof(int), true);

    app_message_outbox_send();
}

static void set_current_mode(int mode) {
    s_last_mode = s_current_mode;
    s_current_mode = mode;
    if(mode == MODE_STOP) {
        text_layer_set_text(s_output_layer, STRING_STOP);
    }
    else if(mode == MODE_VIBRATE) {
        text_layer_set_text(s_output_layer, STRING_VIBRATE);
    }
    else if(mode == MODE_RINGING) {
        text_layer_set_text(s_output_layer, STRING_RINGING);
    }
    else {
        text_layer_set_text(s_output_layer, STRING_TITLE);
    }
}

static void cancel_standby() {
    printf("It's NOT in standby mode. Going to show_actionbar");
    s_is_standby = false;
    show_actionbar(s_action_bar);
    printf("s_is_standby:[%s]", s_is_standby ? "true":"false");
    if(s_standby_timer != NULL) {
        app_timer_cancel(s_standby_timer);
        s_standby_timer = NULL;
    }
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    Tuple *t = dict_read_first(iter);
    while(t != NULL) {
        // Process this Tuple
		
        printf("int32:[%ld] cstring:[%s]", t->value->int32, t->value->cstring);
        if (strcmp(t->value->cstring, STRING_MODE_STOP) == 0) {
            set_current_mode(MODE_STOP);
        }
        else if(strcmp(t->value->cstring, STRING_MODE_VIBRATE) == 0) {
            cancel_standby();
            set_current_mode(MODE_VIBRATE);
        }
        else if(strcmp(t->value->cstring, STRING_MODE_RINGING) == 0) {
            cancel_standby();
            set_current_mode(MODE_RINGING);
        }
        else {
            set_current_mode(MODE_INIT);
        }
		
        // Finally, get the next Tuple
        t = dict_read_next(iter);
    }
}

static void outbox_sent_handler(DictionaryIterator *iter, void *context) {
    // Ready for next command
    //text_layer_set_text(s_output_layer, "Find my phone");
    printf("outbox_sent_handler");
    text_layer_set_text(s_error_msg_layer, "");
}


static void init_update_timer_callback(void *data) {
    // Query the status from APP in case it is already ringing. 
    printf("Ken Query sent!!!!!\n");
    send(KEY_QUERY_MODE, 0);
}

static void standby_timer_callback(void *data) {
    printf("standby_timer_callback!!! hide_actionbar");
    hide_actionbar(s_action_bar);
    s_is_standby = true;
    //text_layer_set_text(s_output_layer, STRING_TITLE);
    printf("s_is_standby:[%s]", s_is_standby ? "true":"false");
}

static void outbox_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *context) {
    printf("outbox_failed_handler");
    text_layer_set_text(s_error_msg_layer, "Send failed!");
    s_current_mode = s_last_mode;
    set_current_mode(s_current_mode);
    
    if(s_current_mode == MODE_STOP || s_current_mode == MODE_INIT) {
        if(s_standby_timer != NULL) {
            s_standby_timer = app_timer_register(STANDBY_TIMEOUT, standby_timer_callback, NULL);
        }
        else {
            app_timer_reschedule(s_standby_timer, STANDBY_TIMEOUT);
        }
    }
	
    APP_LOG(APP_LOG_LEVEL_ERROR, "Fail reason: %d", (int)reason);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    printf("up_click_handler");
    // Handle standby
    if(s_is_standby) {
        cancel_standby();
        
        s_standby_timer = app_timer_register(STANDBY_TIMEOUT, standby_timer_callback, NULL);
        printf("s_standby_timer registered");
        //return;
    }
    else {
        if(s_standby_timer != NULL) {
            app_timer_cancel(s_standby_timer);
            s_standby_timer = NULL;
        }
    }
    //
    
    if(s_current_mode != MODE_VIBRATE) {
        set_current_mode(MODE_VIBRATE);
        send(KEY_BUTTON_UP, 0);
    }
    else {
        printf("Already in MODE_VIBRATE. skip this event.");
    }
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
    printf("select_click_handler");
    // Handle standby
    if(s_is_standby) {
        cancel_standby();
        
        s_standby_timer = app_timer_register(STANDBY_TIMEOUT, standby_timer_callback, NULL);
        printf("s_standby_timer registered 1"); 
        //return;
    }
    else {
        if(s_standby_timer != NULL) {
            if( !app_timer_reschedule(s_standby_timer, STANDBY_TIMEOUT)) {
                s_standby_timer = NULL;
                printf("reschedule app_timer failed. s_standby_timer set to NULL");
            }
            printf("app_timer rescheduled");
        }
        else {
            s_standby_timer = app_timer_register(STANDBY_TIMEOUT, standby_timer_callback, NULL);
            printf("s_standby_timer registered 2"); 
        }
    }
    //
  
    if(s_current_mode != MODE_STOP) {
    set_current_mode(MODE_STOP);
        send(KEY_BUTTON_SELECT, 0);
    }
    else {
        printf("Already in MODE_STOP. skip this event.");
    }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
    printf("down_click_handler");
    // Handle standby
    if(s_is_standby) {
        cancel_standby();
        
        s_standby_timer = app_timer_register(STANDBY_TIMEOUT, standby_timer_callback, NULL);
        printf("s_standby_timer registered"); 
        //return;
    }
    else {
        if(s_standby_timer != NULL) {
            app_timer_cancel(s_standby_timer);
            s_standby_timer = NULL;
        }
    }
    //
    
    if(s_current_mode != MODE_RINGING) {
        set_current_mode(MODE_RINGING);
        send(KEY_BUTTON_DOWN, 0);
    }
    else {
        printf("Already in MODE_VIBRATE. skip this event.");
    }
}


static void click_config_provider(void *context) {
    window_single_repeating_click_subscribe(BUTTON_ID_UP, REPEAT_INTERVAL_MS, up_click_handler);
    window_single_repeating_click_subscribe(BUTTON_ID_SELECT, REPEAT_INTERVAL_MS, select_click_handler);
    window_single_repeating_click_subscribe(BUTTON_ID_DOWN, REPEAT_INTERVAL_MS, down_click_handler);
}


static void update_time() {
    // Get a tm structure
    time_t temp = time(NULL); 
    struct tm *tick_time = localtime(&temp);

    // Create a long-lived buffer
    static char buffer[] = "00:00";

    // Write the current hours and minutes into the buffer
    if(clock_is_24h_style() == true) {
        // Use 24 hour format
        strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
    } else {
        // Use 12 hour format
        strftime(buffer, sizeof("00:00"), "%I:%M", tick_time);
    }

    // Display this time on the TextLayer
    text_layer_set_text(s_time_layer, buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time();
}


static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    s_error_msg_layer = text_layer_create(GRect(10, 5, bounds.size.w, bounds.size.h));
    s_output_layer = text_layer_create(GRect(10, 20, bounds.size.w, bounds.size.h));
    s_time_layer = text_layer_create(GRect(5, 55, 139, 50));
	
    text_layer_set_text(s_error_msg_layer, "");
    text_layer_set_text(s_output_layer, STRING_TITLE);
    
    text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_background_color(s_time_layer, GColorClear);
    text_layer_set_text_color(s_time_layer, GColorBlack);
    text_layer_set_text(s_time_layer, "00:00");

	
    text_layer_set_text_alignment(s_error_msg_layer, GTextAlignmentLeft);
    text_layer_set_text_alignment(s_output_layer, GTextAlignmentLeft);
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentLeft);
    
    layer_add_child(window_layer, text_layer_get_layer(s_error_msg_layer));
    layer_add_child(window_layer, text_layer_get_layer(s_output_layer));
    layer_add_child(window_get_root_layer(s_main_window), text_layer_get_layer(s_time_layer)) ;	
	
    s_action_bar = action_bar_layer_create();
    action_bar_layer_add_to_window(s_action_bar, window);
    action_bar_layer_set_click_config_provider(s_action_bar, click_config_provider);
	    
    action_bar_layer_set_icon(s_action_bar, BUTTON_ID_UP, s_icon_vibrate);
    action_bar_layer_set_icon(s_action_bar, BUTTON_ID_SELECT, s_icon_stop);
    action_bar_layer_set_icon(s_action_bar, BUTTON_ID_DOWN, s_icon_ringtone);

    // Register with TickTimerService
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    update_time();
    
   
    // Register timeout callback for standby mode to hide actionbar
    s_standby_timer = app_timer_register(STANDBY_TIMEOUT, standby_timer_callback, NULL);
    s_init_update_timer = app_timer_register(INIT_UPDATE_TIMEOUT, init_update_timer_callback, NULL);
}

static void main_window_unload(Window *window) {
    text_layer_destroy(s_error_msg_layer);
    text_layer_destroy(s_output_layer);
    text_layer_destroy(s_time_layer);
    action_bar_layer_destroy(s_action_bar);
}

static void init(void) {
    s_main_window = window_create();
    s_icon_vibrate = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_VIBRATE);
    s_icon_stop = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_STOP);
    s_icon_ringtone = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_RINGTONE);
    
    // Open AppMessage
    app_message_register_outbox_sent(outbox_sent_handler);
    app_message_register_outbox_failed(outbox_failed_handler);
    app_message_register_inbox_received(inbox_received_handler);
    app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
    
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload,
    });
    window_stack_push(s_main_window, true);


}

static void deinit(void) {
    window_destroy(s_main_window);
    gbitmap_destroy(s_icon_vibrate);
    gbitmap_destroy(s_icon_stop);
    gbitmap_destroy(s_icon_ringtone);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
