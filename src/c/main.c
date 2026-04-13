/**
 * MASH — Fortune telling party game for Pebble
 * Targets: emery (Time 2), gabbro (Round 2)
 *
 * Pick up to 5 categories, choose 4 options each.
 * Close your eyes, press SELECT. A random number eliminates
 * options one by one until your future is revealed!
 */

#include <pebble.h>
#include <stdlib.h>

#define NUM_CATS     8
#define NUM_OPTS     14
#define PICKS_PER    4
#define MAX_ACTIVE   5

enum { ST_CATS, ST_OPTS, ST_READY, ST_ELIM, ST_FORTUNE };

// ============================================================================
// CATEGORY & OPTION DATA
// ============================================================================
static const char *s_cat_names[] = {
  "Home", "Career", "Vehicle", "Pet",
  "Kids", "Spouse", "Vacation", "Superpower"
};

static const char *s_options[NUM_CATS][NUM_OPTS] = {
  // Home
  {"Mansion", "Apartment", "Shack", "House", "Castle", "Treehouse",
   "Van", "Igloo", "Houseboat", "Cave", "Penthouse", "Tent",
   "RV", "Cardboard Box"},
  // Career
  {"Doctor", "Astronaut", "Clown", "CEO", "Teacher", "Janitor",
   "Spy", "YouTuber", "Pilot", "Chef", "Cowboy", "Wizard",
   "Rock Star", "Mime"},
  // Vehicle
  {"Sports Car", "Bicycle", "Helicopter", "Skateboard", "Limo", "Horse",
   "Rocket", "Bus", "Unicycle", "Tank", "Submarine", "Segway",
   "Scooter", "Jet Ski"},
  // Pet
  {"Dog", "Cat", "Dragon", "Goldfish", "Parrot", "Snake",
   "Unicorn", "Spider", "Hamster", "Llama", "T-Rex", "Rock",
   "Monkey", "Ferret"},
  // Kids
  {"0", "1", "2", "3", "5", "7",
   "10", "12", "100", "Twins", "Triplets", "A Dozen",
   "None", "Too Many"},
  // Spouse
  {"Celebrity", "Neighbor", "Robot", "Alien", "Best Friend", "Pirate",
   "Ninja", "Zombie", "Mermaid", "Ghost", "Royalty", "Stranger",
   "Villain", "Time Travel"},
  // Vacation
  {"Hawaii", "Mars", "Atlantis", "Backyard", "Prison", "Paris",
   "The Moon", "Jungle", "Antarctica", "Bermuda", "Narnia", "Sewers",
   "Cloud Nine", "Volcano"},
  // Superpower
  {"Flying", "Invisible", "Telepathy", "Super Speed", "Time Travel", "Laser Eyes",
   "Shape Shift", "Immortal", "Teleport", "X-Ray", "Freeze Time", "Talk Animal",
   "None", "Curse"}
};

// ============================================================================
// GLOBALS
// ============================================================================
static Window *s_win;
static Layer  *s_canvas;
static GFont   s_icon_font_20;

static int s_state = ST_CATS;
static int s_cursor = 0;
static int s_scroll = 0;

// Category selection
static bool s_cat_active[NUM_CATS];  // category is in the game
static int  s_num_active;            // how many active (max 5)

// Option picks per category
static int  s_picks[NUM_CATS][PICKS_PER]; // option indices chosen
static int  s_pick_count[NUM_CATS];       // how many picked (0-4)

// Option picker state
static int  s_cur_cat;          // which category we're picking for
static bool s_opt_selected[NUM_OPTS]; // temp selection state in picker
static int  s_opt_scroll;

// Elimination
static bool s_eliminated[NUM_CATS][PICKS_PER];
static int  s_elim_number;      // random 3-8
static int  s_elim_show_cat;    // category of last eliminated
static int  s_elim_show_item;   // pick index of last eliminated
static bool s_elim_done;        // all categories decided

// Flat list for elimination counting
typedef struct { int cat; int pick; } ElimEntry;
static ElimEntry s_elim_list[40];
static int  s_elim_list_count;
static int  s_elim_pos;         // position in list for counting

// Fortune scroll
static int s_fortune_scroll;

// ============================================================================
// HELPERS
// ============================================================================
static int remaining_in_cat(int cat) {
  int c = 0;
  for(int i = 0; i < PICKS_PER; i++)
    if(!s_eliminated[cat][i]) c++;
  return c;
}

static bool all_decided(void) {
  for(int c = 0; c < NUM_CATS; c++)
    if(s_cat_active[c] && remaining_in_cat(c) > 1) return false;
  return true;
}

static void rebuild_elim_list(void) {
  s_elim_list_count = 0;
  for(int c = 0; c < NUM_CATS; c++) {
    if(!s_cat_active[c]) continue;
    if(remaining_in_cat(c) <= 1) continue;
    for(int i = 0; i < PICKS_PER; i++) {
      if(!s_eliminated[c][i]) {
        s_elim_list[s_elim_list_count].cat = c;
        s_elim_list[s_elim_list_count].pick = i;
        s_elim_list_count++;
      }
    }
  }
}

static bool do_elimination(void) {
  rebuild_elim_list();
  if(s_elim_list_count == 0) return false;

  // Count forward elim_number steps
  for(int i = 0; i < s_elim_number; i++)
    s_elim_pos = (s_elim_pos + 1) % s_elim_list_count;

  int cat = s_elim_list[s_elim_pos].cat;
  int pick = s_elim_list[s_elim_pos].pick;
  s_eliminated[cat][pick] = true;
  s_elim_show_cat = cat;
  s_elim_show_item = pick;

  // Adjust position for shortened list
  if(s_elim_list_count > 1)
    s_elim_pos = s_elim_pos % (s_elim_list_count - 1);
  else
    s_elim_pos = 0;

  s_elim_done = all_decided();
  return true;
}

// Get the surviving option index for a category
static int get_result(int cat) {
  for(int i = 0; i < PICKS_PER; i++)
    if(!s_eliminated[cat][i]) return s_picks[cat][i];
  return 0;
}

// ============================================================================
// DRAWING
// ============================================================================
static void canvas_proc(Layer *l, GContext *ctx) {
  GRect b = layer_get_bounds(l);
  int w = b.size.w, h = b.size.h;
  int pad = PBL_IF_ROUND_ELSE(18, 4);

  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorFromHEX(0x220044));
  #else
  graphics_context_set_fill_color(ctx, GColorBlack);
  #endif
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  GFont f_lg = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont f_md = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont f_sm = fonts_get_system_font(FONT_KEY_GOTHIC_14);

  // ======== CATEGORY SELECTION ========
  if(s_state == ST_CATS) {
    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorYellow);
    #else
    graphics_context_set_text_color(ctx, GColorWhite);
    #endif
    int title_y = PBL_IF_ROUND_ELSE(pad + 6, pad);
    graphics_draw_text(ctx, "M.A.S.H.", f_lg,
      GRect(0, title_y, w, 34),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    int ly = title_y + 34;
    int item_h = 20;
    int total_items = NUM_CATS + 1; // +1 for GO
    int visible = (h - ly - PBL_IF_ROUND_ELSE(20, 10)) / item_h;

    // Adjust scroll
    if(s_cursor < s_scroll) s_scroll = s_cursor;
    if(s_cursor >= s_scroll + visible) s_scroll = s_cursor - visible + 1;

    for(int vi = 0; vi < visible && s_scroll + vi < total_items; vi++) {
      int idx = s_scroll + vi;
      int iy = ly + vi * item_h;
      bool is_cur = (idx == s_cursor);
      bool is_go = (idx == NUM_CATS);

      if(is_go) {
        // GO button
        if(s_num_active > 0) {
          #ifdef PBL_COLOR
          graphics_context_set_fill_color(ctx, is_cur ? GColorGreen : GColorFromHEX(0x003300));
          #else
          graphics_context_set_fill_color(ctx, is_cur ? GColorWhite : GColorDarkGray);
          #endif
          int gx = (w - 80) / 2;
          graphics_fill_rect(ctx, GRect(gx, iy, 80, item_h - 2), 4, GCornersAll);
          #ifdef PBL_COLOR
          graphics_context_set_text_color(ctx, is_cur ? GColorBlack : GColorLightGray);
          #else
          graphics_context_set_text_color(ctx, is_cur ? GColorBlack : GColorWhite);
          #endif
          char gb[16];
          snprintf(gb, sizeof(gb), "GO! (%d)", s_num_active);
          graphics_draw_text(ctx, gb, f_md,
            GRect(gx, iy - 1, 80, item_h),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
        }
      } else {
        // Category row
        bool active = s_cat_active[idx];
        bool filled = (s_pick_count[idx] == PICKS_PER);
        #ifdef PBL_COLOR
        graphics_context_set_text_color(ctx,
          is_cur ? GColorYellow : (filled ? GColorGreen : GColorWhite));
        #else
        graphics_context_set_text_color(ctx, is_cur ? GColorWhite : GColorLightGray);
        #endif
        char row[32];
        if(filled)
          snprintf(row, sizeof(row), "%s [%s%s%d]", is_cur ? ">" : " ",
            s_cat_names[idx], " ", s_pick_count[idx]);
        else
          snprintf(row, sizeof(row), "%s %s", is_cur ? ">" : " ", s_cat_names[idx]);
        graphics_draw_text(ctx, row, is_cur ? f_md : f_sm,
          GRect(pad + 8, iy, w - pad*2 - 16, item_h),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        // Show pick count on right
        if(filled) {
          graphics_draw_text(ctx, "4/4", f_sm,
            GRect(pad + 8, iy, w - pad*2 - 16, item_h),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
        }
      }
    }
  }

  // ======== OPTION PICKER ========
  else if(s_state == ST_OPTS) {
    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorCyan);
    #else
    graphics_context_set_text_color(ctx, GColorWhite);
    #endif
    int title_y = PBL_IF_ROUND_ELSE(pad + 6, pad);
    int picked = 0;
    for(int i = 0; i < NUM_OPTS; i++) if(s_opt_selected[i]) picked++;
    char title[24];
    snprintf(title, sizeof(title), "%s (%d/%d)", s_cat_names[s_cur_cat], picked, PICKS_PER);
    graphics_draw_text(ctx, title, f_md,
      GRect(0, title_y, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    int ly = title_y + 24;
    int item_h = 20;
    int visible = (h - ly - PBL_IF_ROUND_ELSE(16, 6)) / item_h;

    if(s_cursor < s_opt_scroll) s_opt_scroll = s_cursor;
    if(s_cursor >= s_opt_scroll + visible) s_opt_scroll = s_cursor - visible + 1;

    for(int vi = 0; vi < visible && s_opt_scroll + vi < NUM_OPTS; vi++) {
      int idx = s_opt_scroll + vi;
      int iy = ly + vi * item_h;
      bool is_cur = (idx == s_cursor);
      bool is_sel = s_opt_selected[idx];

      #ifdef PBL_COLOR
      if(is_sel) {
        graphics_context_set_fill_color(ctx, GColorFromHEX(0x004400));
        graphics_fill_rect(ctx, GRect(pad + 4, iy, w - pad*2 - 8, item_h - 1), 2, GCornersAll);
      }
      graphics_context_set_text_color(ctx,
        is_cur ? GColorYellow : (is_sel ? GColorGreen : GColorWhite));
      #else
      graphics_context_set_text_color(ctx, is_cur ? GColorWhite : GColorLightGray);
      #endif
      char row[32];
      snprintf(row, sizeof(row), "%s%s%s",
        is_cur ? "> " : "  ",
        is_sel ? "* " : "  ",
        s_options[s_cur_cat][idx]);
      graphics_draw_text(ctx, row, is_cur ? f_md : f_sm,
        GRect(pad + 4, iy, w - pad*2 - 8, item_h),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
  }

  // ======== READY ========
  else if(s_state == ST_READY) {
    int cy = h / 2 - 40;
    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorYellow);
    #else
    graphics_context_set_text_color(ctx, GColorWhite);
    #endif
    graphics_draw_text(ctx, "M.A.S.H.", f_lg,
      GRect(0, cy, w, 34),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "Close your eyes...", f_md,
      GRect(0, cy + 40, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "Press SELECT to", f_sm,
      GRect(0, cy + 66, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "reveal your future!", f_sm,
      GRect(0, cy + 82, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // ======== ELIMINATION ========
  else if(s_state == ST_ELIM) {
    int cy = h / 2 - 50;
    // Category name
    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorCyan);
    #else
    graphics_context_set_text_color(ctx, GColorWhite);
    #endif
    graphics_draw_text(ctx, s_cat_names[s_elim_show_cat], f_md,
      GRect(0, cy, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    // Eliminated item (struck through)
    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorRed);
    #endif
    int opt_idx = s_picks[s_elim_show_cat][s_elim_show_item];
    graphics_draw_text(ctx, s_options[s_elim_show_cat][opt_idx], f_lg,
      GRect(0, cy + 28, w, 34),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    // Strikethrough line
    #ifdef PBL_COLOR
    graphics_context_set_stroke_color(ctx, GColorRed);
    #else
    graphics_context_set_stroke_color(ctx, GColorWhite);
    #endif
    graphics_context_set_stroke_width(ctx, 2);
    int line_y = cy + 46;
    graphics_draw_line(ctx, GPoint(w/4, line_y), GPoint(w*3/4, line_y));

    // Status
    graphics_context_set_text_color(ctx, GColorWhite);
    if(s_elim_done) {
      graphics_draw_text(ctx, "All decided!", f_md,
        GRect(0, cy + 70, w, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
      graphics_draw_text(ctx, "SELECT: see your future!", f_sm,
        GRect(0, cy + 94, w, 16),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    } else {
      char rem[16];
      int total_rem = 0;
      for(int c = 0; c < NUM_CATS; c++)
        if(s_cat_active[c]) total_rem += remaining_in_cat(c);
      snprintf(rem, sizeof(rem), "%d remaining", total_rem);
      graphics_draw_text(ctx, rem, f_sm,
        GRect(0, cy + 74, w, 16),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
      graphics_draw_text(ctx, "SELECT: next", f_sm,
        GRect(0, cy + 92, w, 16),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
  }

  // ======== FORTUNE ========
  else if(s_state == ST_FORTUNE) {
    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorYellow);
    #else
    graphics_context_set_text_color(ctx, GColorWhite);
    #endif
    int title_y = PBL_IF_ROUND_ELSE(pad + 6, pad);
    graphics_draw_text(ctx, "YOUR FUTURE", f_lg,
      GRect(0, title_y, w, 34),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    int ly = title_y + 36;
    int item_h = 30;
    int visible = (h - ly - PBL_IF_ROUND_ELSE(20, 10)) / item_h;

    // Count active categories
    int active_cats[MAX_ACTIVE];
    int ac = 0;
    for(int c = 0; c < NUM_CATS; c++)
      if(s_cat_active[c]) active_cats[ac++] = c;

    if(s_fortune_scroll < 0) s_fortune_scroll = 0;
    if(s_fortune_scroll > ac - visible) s_fortune_scroll = ac - visible;
    if(s_fortune_scroll < 0) s_fortune_scroll = 0;

    for(int vi = 0; vi < visible && s_fortune_scroll + vi < ac; vi++) {
      int cat = active_cats[s_fortune_scroll + vi];
      int iy = ly + vi * item_h;
      int opt = get_result(cat);

      // Category label
      #ifdef PBL_COLOR
      graphics_context_set_text_color(ctx, GColorCyan);
      #else
      graphics_context_set_text_color(ctx, GColorWhite);
      #endif
      graphics_draw_text(ctx, s_cat_names[cat], f_sm,
        GRect(pad + PBL_IF_ROUND_ELSE(20, 8), iy, w - pad*2, 16),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

      // Result
      #ifdef PBL_COLOR
      graphics_context_set_text_color(ctx, GColorYellow);
      #else
      graphics_context_set_text_color(ctx, GColorWhite);
      #endif
      graphics_draw_text(ctx, s_options[cat][opt], f_md,
        GRect(pad + PBL_IF_ROUND_ELSE(20, 8), iy + 12, w - pad*2, 20),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }

    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "SELECT: play again", f_sm,
      GRect(0, h - PBL_IF_ROUND_ELSE(26, 16), w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// ============================================================================
// BUTTON HANDLERS
// ============================================================================
static void select_click(ClickRecognizerRef ref, void *ctx) {
  if(s_state == ST_CATS) {
    if(s_cursor == NUM_CATS) {
      // GO button
      if(s_num_active > 0) {
        // Init elimination
        for(int c = 0; c < NUM_CATS; c++)
          for(int i = 0; i < PICKS_PER; i++)
            s_eliminated[c][i] = false;
        s_elim_number = (rand() % 6) + 3; // 3-8
        s_elim_pos = -1; // will advance on first elimination
        s_elim_done = false;
        s_state = ST_READY;
      }
    } else {
      // Enter category option picker
      int cat = s_cursor;
      if(s_pick_count[cat] == PICKS_PER && s_cat_active[cat]) {
        // Already filled — deactivate
        s_cat_active[cat] = false;
        s_pick_count[cat] = 0;
        s_num_active--;
      } else if(s_num_active < MAX_ACTIVE || s_cat_active[cat]) {
        // Enter option picker
        s_cur_cat = cat;
        // Restore previous selections
        for(int i = 0; i < NUM_OPTS; i++) s_opt_selected[i] = false;
        for(int i = 0; i < s_pick_count[cat]; i++)
          s_opt_selected[s_picks[cat][i]] = true;
        s_cursor = 0;
        s_opt_scroll = 0;
        s_state = ST_OPTS;
      }
    }
  }
  else if(s_state == ST_OPTS) {
    int idx = s_cursor;
    int picked = 0;
    for(int i = 0; i < NUM_OPTS; i++) if(s_opt_selected[i]) picked++;

    if(s_opt_selected[idx]) {
      // Deselect
      s_opt_selected[idx] = false;
    } else if(picked < PICKS_PER) {
      // Select
      s_opt_selected[idx] = true;
      picked++;

      // If 4 picked, auto-save and return
      if(picked == PICKS_PER) {
        int cat = s_cur_cat;
        s_pick_count[cat] = 0;
        for(int i = 0; i < NUM_OPTS; i++) {
          if(s_opt_selected[i]) {
            s_picks[cat][s_pick_count[cat]++] = i;
          }
        }
        if(!s_cat_active[cat]) {
          s_cat_active[cat] = true;
          s_num_active++;
        }
        s_cursor = s_cur_cat;
        s_scroll = 0;
        s_state = ST_CATS;
      }
    }
  }
  else if(s_state == ST_READY) {
    // Start elimination
    do_elimination();
    s_state = ST_ELIM;
  }
  else if(s_state == ST_ELIM) {
    if(s_elim_done) {
      s_fortune_scroll = 0;
      s_state = ST_FORTUNE;
    } else {
      do_elimination();
    }
  }
  else if(s_state == ST_FORTUNE) {
    // Play again — reset everything
    for(int c = 0; c < NUM_CATS; c++) {
      s_cat_active[c] = false;
      s_pick_count[c] = 0;
    }
    s_num_active = 0;
    s_cursor = 0;
    s_scroll = 0;
    s_state = ST_CATS;
  }
  if(s_canvas) layer_mark_dirty(s_canvas);
}

static void up_click(ClickRecognizerRef ref, void *ctx) {
  if(s_state == ST_CATS) {
    if(s_cursor > 0) s_cursor--;
  } else if(s_state == ST_OPTS) {
    if(s_cursor > 0) s_cursor--;
  } else if(s_state == ST_FORTUNE) {
    if(s_fortune_scroll > 0) s_fortune_scroll--;
  }
  if(s_canvas) layer_mark_dirty(s_canvas);
}

static void down_click(ClickRecognizerRef ref, void *ctx) {
  if(s_state == ST_CATS) {
    if(s_cursor < NUM_CATS) s_cursor++; // NUM_CATS = GO position
  } else if(s_state == ST_OPTS) {
    if(s_cursor < NUM_OPTS - 1) s_cursor++;
  } else if(s_state == ST_FORTUNE) {
    s_fortune_scroll++;
  }
  if(s_canvas) layer_mark_dirty(s_canvas);
}

static void back_click(ClickRecognizerRef ref, void *ctx) {
  if(s_state == ST_OPTS) {
    // Save partial selections and return to cats
    int cat = s_cur_cat;
    s_pick_count[cat] = 0;
    for(int i = 0; i < NUM_OPTS; i++) {
      if(s_opt_selected[i] && s_pick_count[cat] < PICKS_PER)
        s_picks[cat][s_pick_count[cat]++] = i;
    }
    // If partially filled, deactivate
    if(s_pick_count[cat] < PICKS_PER && s_cat_active[cat]) {
      s_cat_active[cat] = false;
      s_num_active--;
    }
    s_cursor = s_cur_cat;
    s_scroll = 0;
    s_state = ST_CATS;
    if(s_canvas) layer_mark_dirty(s_canvas);
  } else if(s_state == ST_CATS) {
    window_stack_pop(true);
  } else if(s_state == ST_FORTUNE) {
    // Play again
    for(int c = 0; c < NUM_CATS; c++) {
      s_cat_active[c] = false;
      s_pick_count[c] = 0;
    }
    s_num_active = 0;
    s_cursor = 0;
    s_scroll = 0;
    s_state = ST_CATS;
    if(s_canvas) layer_mark_dirty(s_canvas);
  }
}

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
}

// ============================================================================
// WINDOW & LIFECYCLE
// ============================================================================
static void win_load(Window *w) {
  Layer *wl = window_get_root_layer(w);
  GRect b = layer_get_bounds(wl);
  s_canvas = layer_create(b);
  layer_set_update_proc(s_canvas, canvas_proc);
  layer_add_child(wl, s_canvas);
  window_set_click_config_provider(w, click_config);
}

static void win_unload(Window *w) {
  if(s_canvas) { layer_destroy(s_canvas); s_canvas = NULL; }
}

static void init(void) {
  srand(time(NULL));
  s_icon_font_20 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_ICON_FONT_20));
  s_win = window_create();
  window_set_background_color(s_win, GColorBlack);
  window_set_window_handlers(s_win, (WindowHandlers){.load=win_load,.unload=win_unload});
  window_stack_push(s_win, true);
}

static void deinit(void) {
  window_destroy(s_win);
  fonts_unload_custom_font(s_icon_font_20);
}

int main(void) { init(); app_event_loop(); deinit(); return 0; }
