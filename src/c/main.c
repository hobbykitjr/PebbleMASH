/**
 * MASH — Fortune telling party game for Pebble
 * Targets: emery (Time 2), gabbro (Round 2)
 *
 * Pick up to 5 categories, choose 4 options each (including a
 * blank for your own secret pick). Notebook paper aesthetic.
 * Random lucky number eliminates options until your future is set!
 */

#include <pebble.h>
#include <stdlib.h>

#define NUM_CATS      8
#define NUM_REAL_OPTS 14
#define NUM_OPTS      15  // 14 + blank
#define PICKS_PER     4
#define MAX_ACTIVE    5

enum { ST_CATS, ST_OPTS, ST_READY, ST_ELIM, ST_FORTUNE };

// ============================================================================
// DATA
// ============================================================================
static const char *s_cat_names[] = {
  "Home", "Career", "Vehicle", "Pet",
  "Kids", "Spouse", "Vacation", "Superpower"
};

static const char *s_options[NUM_CATS][NUM_REAL_OPTS] = {
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
   "Villain", "Time Travlr"},
  // Vacation
  {"Hawaii", "Mars", "Atlantis", "Backyard", "Prison", "Paris",
   "The Moon", "Jungle", "Antarctica", "Bermuda", "Narnia", "Sewers",
   "Cloud Nine", "Volcano"},
  // Superpower
  {"Flying", "Invisible", "Telepathy", "Super Speed", "Time Travel", "Laser Eyes",
   "Shape Shift", "Immortal", "Teleport", "X-Ray", "Freeze Time", "Talk Animal",
   "None", "Curse"}
};

static const char *get_opt(int cat, int idx) {
  if(idx >= NUM_REAL_OPTS) return "________";
  return s_options[cat][idx];
}

static const char *get_opt_fortune(int cat, int idx) {
  if(idx >= NUM_REAL_OPTS) return "Your Pick!";
  return s_options[cat][idx];
}

// ============================================================================
// GLOBALS
// ============================================================================
static Window *s_win;
static Layer  *s_canvas;
static GFont   s_icon_font_20;

static int s_state = ST_CATS;
static int s_cursor = 0;
static int s_scroll = 0;

static bool s_cat_active[NUM_CATS];
static int  s_num_active;

static int  s_picks[NUM_CATS][PICKS_PER];
static int  s_pick_count[NUM_CATS];

static int  s_cur_cat;
static bool s_opt_selected[NUM_OPTS];
static int  s_opt_scroll;

static bool s_eliminated[NUM_CATS][PICKS_PER];
static int  s_elim_number;
static int  s_elim_show_cat;
static int  s_elim_show_item;
static bool s_elim_done;

typedef struct { int cat; int pick; } ElimEntry;
static ElimEntry s_elim_list[40];
static int  s_elim_list_count;
static int  s_elim_pos;

static int s_fortune_scroll;

// ============================================================================
// ELIMINATION LOGIC
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

  for(int i = 0; i < s_elim_number; i++)
    s_elim_pos = (s_elim_pos + 1) % s_elim_list_count;

  int cat = s_elim_list[s_elim_pos].cat;
  int pick = s_elim_list[s_elim_pos].pick;
  s_eliminated[cat][pick] = true;
  s_elim_show_cat = cat;
  s_elim_show_item = pick;

  if(s_elim_list_count > 1)
    s_elim_pos = s_elim_pos % (s_elim_list_count - 1);
  else
    s_elim_pos = 0;

  s_elim_done = all_decided();
  return true;
}

static int get_result(int cat) {
  for(int i = 0; i < PICKS_PER; i++)
    if(!s_eliminated[cat][i]) return s_picks[cat][i];
  return 0;
}

// ============================================================================
// NOTEBOOK DRAWING
// ============================================================================

// Draw notebook paper background with ruled lines
static void draw_notebook(GContext *ctx, int w, int h, int line_start) {
  // Paper
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorFromHEX(0xFFFFAA));
  #else
  graphics_context_set_fill_color(ctx, GColorWhite);
  #endif
  graphics_fill_rect(ctx, GRect(0, 0, w, h), 0, GCornerNone);

  // Blue ruled lines
  #ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorFromHEX(0x5555FF));
  #else
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  #endif
  graphics_context_set_stroke_width(ctx, 1);
  for(int y = line_start; y < h; y += 20)
    graphics_draw_line(ctx, GPoint(0, y), GPoint(w, y));

  // Red margin
  #ifdef PBL_COLOR
  int mx = PBL_IF_ROUND_ELSE(40, 28);
  graphics_context_set_stroke_color(ctx, GColorFromHEX(0xFF5555));
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(mx, 0), GPoint(mx, h));
  #endif
}

// ============================================================================
// CANVAS
// ============================================================================
static void canvas_proc(Layer *l, GContext *ctx) {
  GRect b = layer_get_bounds(l);
  int w = b.size.w, h = b.size.h;
  int pad = PBL_IF_ROUND_ELSE(18, 4);
  int margin = PBL_IF_ROUND_ELSE(46, 34);

  GFont f_lg = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont f_md = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont f_sm = fonts_get_system_font(FONT_KEY_GOTHIC_14);

  // ======== CATEGORY SELECTION ========
  if(s_state == ST_CATS) {
    int title_y = PBL_IF_ROUND_ELSE(pad + 6, pad);
    draw_notebook(ctx, w, h, title_y + 34);

    // Title
    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorFromHEX(0x0000AA));
    #else
    graphics_context_set_text_color(ctx, GColorBlack);
    #endif
    graphics_draw_text(ctx, "M.A.S.H.", f_lg,
      GRect(0, title_y, w, 34),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    int ly = title_y + 36;
    int item_h = 20;
    int total_items = NUM_CATS + 1;
    int visible = (h - ly - PBL_IF_ROUND_ELSE(20, 8)) / item_h;

    if(s_cursor < s_scroll) s_scroll = s_cursor;
    if(s_cursor >= s_scroll + visible) s_scroll = s_cursor - visible + 1;

    for(int vi = 0; vi < visible && s_scroll + vi < total_items; vi++) {
      int idx = s_scroll + vi;
      int iy = ly + vi * item_h;
      bool is_cur = (idx == s_cursor);
      bool is_go = (idx == NUM_CATS);

      if(is_go) {
        if(s_num_active > 0) {
          #ifdef PBL_COLOR
          graphics_context_set_fill_color(ctx, GColorFromHEX(0x005500));
          graphics_fill_rect(ctx, GRect((w-80)/2, iy, 80, item_h-1), 4, GCornersAll);
          graphics_context_set_text_color(ctx, is_cur ? GColorWhite : GColorFromHEX(0x55FF55));
          #else
          graphics_context_set_text_color(ctx, is_cur ? GColorBlack : GColorDarkGray);
          #endif
          char gb[16];
          snprintf(gb, sizeof(gb), "GO! (%d)", s_num_active);
          graphics_draw_text(ctx, gb, f_md,
            GRect((w-80)/2, iy-1, 80, item_h),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
        }
      } else {
        bool filled = (s_pick_count[idx] == PICKS_PER && s_cat_active[idx]);
        // Highlight bar for cursor
        if(is_cur) {
          #ifdef PBL_COLOR
          graphics_context_set_fill_color(ctx, GColorFromHEX(0xDDDDAA));
          #else
          graphics_context_set_fill_color(ctx, GColorLightGray);
          #endif
          graphics_fill_rect(ctx, GRect(margin, iy, w - margin - pad, item_h - 1), 0, GCornerNone);
        }
        #ifdef PBL_COLOR
        graphics_context_set_text_color(ctx, filled ? GColorFromHEX(0x005500) : GColorBlack);
        #else
        graphics_context_set_text_color(ctx, GColorBlack);
        #endif
        char row[28];
        snprintf(row, sizeof(row), "%s", s_cat_names[idx]);
        graphics_draw_text(ctx, row, is_cur ? f_md : f_sm,
          GRect(margin + 4, iy, w - margin - pad - 40, item_h),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        if(filled) {
          #ifdef PBL_COLOR
          graphics_context_set_text_color(ctx, GColorFromHEX(0x005500));
          #endif
          graphics_draw_text(ctx, "4/4", f_sm,
            GRect(margin, iy, w - margin - pad - 4, item_h),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
        }
      }
    }
  }

  // ======== OPTION PICKER ========
  else if(s_state == ST_OPTS) {
    int title_y = PBL_IF_ROUND_ELSE(pad + 6, pad);
    draw_notebook(ctx, w, h, title_y + 24);

    int picked = 0;
    for(int i = 0; i < NUM_OPTS; i++) if(s_opt_selected[i]) picked++;
    char title[28];
    snprintf(title, sizeof(title), "%s (%d/%d)", s_cat_names[s_cur_cat], picked, PICKS_PER);
    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorFromHEX(0x0000AA));
    #else
    graphics_context_set_text_color(ctx, GColorBlack);
    #endif
    graphics_draw_text(ctx, title, f_md,
      GRect(0, title_y, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    int ly = title_y + 26;
    int item_h = 20;
    int visible = (h - ly - PBL_IF_ROUND_ELSE(16, 4)) / item_h;

    if(s_cursor < s_opt_scroll) s_opt_scroll = s_cursor;
    if(s_cursor >= s_opt_scroll + visible) s_opt_scroll = s_cursor - visible + 1;

    for(int vi = 0; vi < visible && s_opt_scroll + vi < NUM_OPTS; vi++) {
      int idx = s_opt_scroll + vi;
      int iy = ly + vi * item_h;
      bool is_cur = (idx == s_cursor);
      bool is_sel = s_opt_selected[idx];

      // Cursor highlight
      if(is_cur) {
        #ifdef PBL_COLOR
        graphics_context_set_fill_color(ctx, GColorFromHEX(0xDDDDAA));
        #else
        graphics_context_set_fill_color(ctx, GColorLightGray);
        #endif
        graphics_fill_rect(ctx, GRect(margin, iy, w - margin - pad, item_h - 1), 0, GCornerNone);
      }

      // Checkmark or dash
      #ifdef PBL_COLOR
      graphics_context_set_text_color(ctx, is_sel ? GColorFromHEX(0x005500) : GColorBlack);
      #else
      graphics_context_set_text_color(ctx, GColorBlack);
      #endif
      const char *check = is_sel ? "*" : " ";
      graphics_draw_text(ctx, check, f_md,
        GRect(margin - 12, iy - 1, 14, item_h),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

      // Option text
      graphics_draw_text(ctx, get_opt(s_cur_cat, idx), is_cur ? f_md : f_sm,
        GRect(margin + 4, iy, w - margin - pad - 8, item_h),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
  }

  // ======== READY ========
  else if(s_state == ST_READY) {
    draw_notebook(ctx, w, h, 40);

    int cy = h / 2 - 50;
    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorFromHEX(0x0000AA));
    #else
    graphics_context_set_text_color(ctx, GColorBlack);
    #endif
    graphics_draw_text(ctx, "M.A.S.H.", f_lg,
      GRect(0, cy, w, 34),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, "Close your eyes...", f_md,
      GRect(0, cy + 40, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    char lucky[20];
    snprintf(lucky, sizeof(lucky), "Lucky #: %d", s_elim_number);
    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorFromHEX(0xAA0000));
    #endif
    graphics_draw_text(ctx, lucky, f_md,
      GRect(0, cy + 66, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, "Press SELECT to", f_sm,
      GRect(0, cy + 92, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "read your fortune!", f_sm,
      GRect(0, cy + 108, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // ======== ELIMINATION ========
  else if(s_state == ST_ELIM) {
    draw_notebook(ctx, w, h, 40);

    int cy = h / 2 - 50;
    // Lucky number
    char lucky[12];
    snprintf(lucky, sizeof(lucky), "#%d", s_elim_number);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, lucky, f_sm,
      GRect(0, cy, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    // Category
    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorFromHEX(0x0000AA));
    #else
    graphics_context_set_text_color(ctx, GColorBlack);
    #endif
    graphics_draw_text(ctx, s_cat_names[s_elim_show_cat], f_md,
      GRect(0, cy + 18, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    // Eliminated item
    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorFromHEX(0xAA0000));
    #else
    graphics_context_set_text_color(ctx, GColorBlack);
    #endif
    int opt_idx = s_picks[s_elim_show_cat][s_elim_show_item];
    graphics_draw_text(ctx, get_opt(s_elim_show_cat, opt_idx), f_lg,
      GRect(pad, cy + 44, w - pad*2, 34),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    // Strikethrough
    #ifdef PBL_COLOR
    graphics_context_set_stroke_color(ctx, GColorFromHEX(0xAA0000));
    #else
    graphics_context_set_stroke_color(ctx, GColorBlack);
    #endif
    graphics_context_set_stroke_width(ctx, 2);
    int line_y = cy + 62;
    graphics_draw_line(ctx, GPoint(w/4, line_y), GPoint(w*3/4, line_y));

    // Status
    graphics_context_set_text_color(ctx, GColorDarkGray);
    if(s_elim_done) {
      graphics_draw_text(ctx, "All decided!", f_md,
        GRect(0, cy + 82, w, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
      graphics_draw_text(ctx, "SELECT: your future!", f_sm,
        GRect(0, cy + 106, w, 16),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    } else {
      int total_rem = 0;
      for(int c = 0; c < NUM_CATS; c++)
        if(s_cat_active[c]) total_rem += remaining_in_cat(c);
      char rem[16];
      snprintf(rem, sizeof(rem), "%d left", total_rem);
      graphics_draw_text(ctx, rem, f_sm,
        GRect(0, cy + 86, w, 16),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
      graphics_draw_text(ctx, "SELECT: next", f_sm,
        GRect(0, cy + 104, w, 16),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
  }

  // ======== FORTUNE ========
  else if(s_state == ST_FORTUNE) {
    int title_y = PBL_IF_ROUND_ELSE(pad + 6, pad);
    draw_notebook(ctx, w, h, title_y + 34);

    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorFromHEX(0x0000AA));
    #else
    graphics_context_set_text_color(ctx, GColorBlack);
    #endif
    graphics_draw_text(ctx, "YOUR FUTURE", f_lg,
      GRect(0, title_y, w, 34),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    int ly = title_y + 38;
    int item_h = 30;
    int visible = (h - ly - PBL_IF_ROUND_ELSE(22, 12)) / item_h;

    int active_cats[MAX_ACTIVE];
    int ac = 0;
    for(int c = 0; c < NUM_CATS; c++)
      if(s_cat_active[c]) active_cats[ac++] = c;

    if(s_fortune_scroll < 0) s_fortune_scroll = 0;
    if(s_fortune_scroll > ac - visible && ac > visible)
      s_fortune_scroll = ac - visible;
    if(s_fortune_scroll < 0) s_fortune_scroll = 0;

    for(int vi = 0; vi < visible && s_fortune_scroll + vi < ac; vi++) {
      int cat = active_cats[s_fortune_scroll + vi];
      int iy = ly + vi * item_h;
      int opt = get_result(cat);

      // Category label (small, grey)
      graphics_context_set_text_color(ctx, GColorDarkGray);
      graphics_draw_text(ctx, s_cat_names[cat], f_sm,
        GRect(margin + 4, iy, w - margin - pad, 16),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

      // Result (bold, dark ink)
      #ifdef PBL_COLOR
      graphics_context_set_text_color(ctx, GColorBlack);
      #endif
      graphics_draw_text(ctx, get_opt_fortune(cat, opt), f_md,
        GRect(margin + 4, iy + 12, w - margin - pad, 20),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }

    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, "SELECT: play again", f_sm,
      GRect(0, h - PBL_IF_ROUND_ELSE(24, 14), w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// ============================================================================
// BUTTONS
// ============================================================================
static void select_click(ClickRecognizerRef ref, void *ctx) {
  if(s_state == ST_CATS) {
    if(s_cursor == NUM_CATS) {
      // GO
      if(s_num_active > 0) {
        for(int c = 0; c < NUM_CATS; c++)
          for(int i = 0; i < PICKS_PER; i++)
            s_eliminated[c][i] = false;
        s_elim_number = (rand() % 6) + 3;
        s_elim_pos = -1;
        s_elim_done = false;
        s_state = ST_READY;
      }
    } else {
      int cat = s_cursor;
      if(s_pick_count[cat] == PICKS_PER && s_cat_active[cat]) {
        // Deactivate filled category
        s_cat_active[cat] = false;
        s_pick_count[cat] = 0;
        s_num_active--;
      } else if(s_num_active < MAX_ACTIVE || s_cat_active[cat]) {
        s_cur_cat = cat;
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
      s_opt_selected[idx] = false;
    } else if(picked < PICKS_PER) {
      s_opt_selected[idx] = true;
      picked++;

      if(picked == PICKS_PER) {
        // Auto-save and return
        int cat = s_cur_cat;
        s_pick_count[cat] = 0;
        for(int i = 0; i < NUM_OPTS; i++)
          if(s_opt_selected[i])
            s_picks[cat][s_pick_count[cat]++] = i;
        if(!s_cat_active[cat]) {
          s_cat_active[cat] = true;
          s_num_active++;
        }
        s_cursor = s_cur_cat;
        s_scroll = 0;
        s_state = ST_CATS;
      } else {
        // Auto-advance cursor to next unselected option
        int next = s_cursor;
        for(int i = 1; i < NUM_OPTS; i++) {
          next = (s_cursor + i) % NUM_OPTS;
          if(!s_opt_selected[next]) break;
        }
        s_cursor = next;
      }
    }
  }
  else if(s_state == ST_READY) {
    do_elimination();
    vibes_short_pulse();
    s_state = ST_ELIM;
  }
  else if(s_state == ST_ELIM) {
    if(s_elim_done) {
      s_fortune_scroll = 0;
      s_state = ST_FORTUNE;
    } else {
      do_elimination();
      vibes_short_pulse();
    }
  }
  else if(s_state == ST_FORTUNE) {
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
    if(s_cursor < NUM_CATS) s_cursor++;
  } else if(s_state == ST_OPTS) {
    if(s_cursor < NUM_OPTS - 1) s_cursor++;
  } else if(s_state == ST_FORTUNE) {
    s_fortune_scroll++;
  }
  if(s_canvas) layer_mark_dirty(s_canvas);
}

static void back_click(ClickRecognizerRef ref, void *ctx) {
  if(s_state == ST_OPTS) {
    int cat = s_cur_cat;
    s_pick_count[cat] = 0;
    for(int i = 0; i < NUM_OPTS; i++)
      if(s_opt_selected[i] && s_pick_count[cat] < PICKS_PER)
        s_picks[cat][s_pick_count[cat]++] = i;
    if(s_pick_count[cat] < PICKS_PER && s_cat_active[cat]) {
      s_cat_active[cat] = false;
      s_num_active--;
    }
    s_cursor = s_cur_cat;
    s_scroll = 0;
    s_state = ST_CATS;
  } else if(s_state == ST_CATS) {
    window_stack_pop(true);
  } else if(s_state == ST_FORTUNE) {
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

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
}

// ============================================================================
// LIFECYCLE
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
