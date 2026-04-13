/**
 * MASH — Fortune telling party game for Pebble
 * Targets: emery (Time 2), gabbro (Round 2)
 */

#include <pebble.h>
#include <stdlib.h>

#define NUM_CATS      8
#define NUM_REAL_OPTS 14
#define NUM_OPTS      15
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
  {"Mansion","Apartment","Shack","House","Castle","Treehouse",
   "Van","Igloo","Houseboat","Cave","Penthouse","Tent","RV","Cardboard Box"},
  {"Doctor","Astronaut","Clown","CEO","Teacher","Janitor",
   "Spy","YouTuber","Pilot","Chef","Cowboy","Wizard","Rock Star","Mime"},
  {"Sports Car","Bicycle","Helicopter","Skateboard","Limo","Horse",
   "Rocket","Bus","Unicycle","Tank","Submarine","Segway","Scooter","Jet Ski"},
  {"Dog","Cat","Dragon","Goldfish","Parrot","Snake",
   "Unicorn","Spider","Hamster","Llama","T-Rex","Rock","Monkey","Ferret"},
  {"0","1","2","3","5","7",
   "10","12","100","Twins","Triplets","A Dozen","None","Too Many"},
  {"Celebrity","Neighbor","Robot","Alien","Best Friend","Pirate",
   "Ninja","Zombie","Mermaid","Ghost","Royalty","Stranger","Villain","Time Travlr"},
  {"Hawaii","Mars","Atlantis","Backyard","Prison","Paris",
   "The Moon","Jungle","Antarctica","Bermuda","Narnia","Sewers","Cloud Nine","Volcano"},
  {"Flying","Invisible","Telepathy","Super Speed","Time Travel","Laser Eyes",
   "Shape Shift","Immortal","Teleport","X-Ray","Freeze Time","Talk Animal","None","Curse"}
};

static const char *get_opt(int cat, int idx) {
  return (idx >= NUM_REAL_OPTS) ? "________" : s_options[cat][idx];
}
static const char *get_opt_fortune(int cat, int idx) {
  return (idx >= NUM_REAL_OPTS) ? "Your Pick!" : s_options[cat][idx];
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

// Spinning number animation
static AppTimer *s_spin_timer;
static int  s_spin_number;   // currently displayed spinning number
static bool s_spinning;      // is the number spinning?

// ============================================================================
// ELIMINATION LOGIC
// ============================================================================
static int remaining_in_cat(int cat) {
  int c = 0;
  for(int i = 0; i < PICKS_PER; i++) if(!s_eliminated[cat][i]) c++;
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
    if(!s_cat_active[c] || remaining_in_cat(c) <= 1) continue;
    for(int i = 0; i < PICKS_PER; i++)
      if(!s_eliminated[c][i]) {
        s_elim_list[s_elim_list_count].cat = c;
        s_elim_list[s_elim_list_count].pick = i;
        s_elim_list_count++;
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
  s_elim_pos = (s_elim_list_count > 1) ? s_elim_pos % (s_elim_list_count - 1) : 0;
  s_elim_done = all_decided();
  return true;
}

static int get_result(int cat) {
  for(int i = 0; i < PICKS_PER; i++) if(!s_eliminated[cat][i]) return s_picks[cat][i];
  return 0;
}

// ============================================================================
// SPIN TIMER
// ============================================================================
static void spin_tick(void *data) {
  if(!s_spinning) return;
  s_spin_number = (rand() % 6) + 3; // 3-8
  if(s_canvas) layer_mark_dirty(s_canvas);
  s_spin_timer = app_timer_register(80, spin_tick, NULL);
}

static void start_spinning(void) {
  s_spinning = true;
  s_spin_number = (rand() % 6) + 3;
  s_spin_timer = app_timer_register(80, spin_tick, NULL);
}

static void stop_spinning(void) {
  s_spinning = false;
  if(s_spin_timer) { app_timer_cancel(s_spin_timer); s_spin_timer = NULL; }
  s_elim_number = s_spin_number;
}

// ============================================================================
// NOTEBOOK DOODLES (drawn in pencil gray)
// ============================================================================
static void doodle_color(GContext *ctx) {
  #ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorFromHEX(0xAAAA55));
  graphics_context_set_fill_color(ctx, GColorFromHEX(0xAAAA55));
  #else
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_context_set_fill_color(ctx, GColorLightGray);
  #endif
  graphics_context_set_stroke_width(ctx, 1);
}

// The "Cool S" / Stussy S (~12x20px)
static void draw_cool_s(GContext *ctx, int x, int y) {
  doodle_color(ctx);
  // Top 3 lines
  graphics_draw_line(ctx, GPoint(x-3, y-10), GPoint(x-3, y-4));
  graphics_draw_line(ctx, GPoint(x,   y-10), GPoint(x,   y-4));
  graphics_draw_line(ctx, GPoint(x+3, y-10), GPoint(x+3, y-4));
  // Top cap
  graphics_draw_line(ctx, GPoint(x-3, y-10), GPoint(x, y-13));
  graphics_draw_line(ctx, GPoint(x+3, y-10), GPoint(x, y-13));
  // Middle cross
  graphics_draw_line(ctx, GPoint(x-3, y-4), GPoint(x+3, y+4));
  graphics_draw_line(ctx, GPoint(x+3, y-4), GPoint(x-3, y+4));
  // Bottom 3 lines
  graphics_draw_line(ctx, GPoint(x-3, y+4), GPoint(x-3, y+10));
  graphics_draw_line(ctx, GPoint(x,   y+4), GPoint(x,   y+10));
  graphics_draw_line(ctx, GPoint(x+3, y+4), GPoint(x+3, y+10));
  // Bottom cap
  graphics_draw_line(ctx, GPoint(x-3, y+10), GPoint(x, y+13));
  graphics_draw_line(ctx, GPoint(x+3, y+10), GPoint(x, y+13));
}

// Stick figure (~14x20px)
static void draw_stickfig(GContext *ctx, int x, int y) {
  doodle_color(ctx);
  graphics_draw_circle(ctx, GPoint(x, y-8), 3); // head
  graphics_draw_line(ctx, GPoint(x, y-5), GPoint(x, y+4)); // body
  graphics_draw_line(ctx, GPoint(x, y-2), GPoint(x-5, y-6)); // left arm
  graphics_draw_line(ctx, GPoint(x, y-2), GPoint(x+5, y-6)); // right arm
  graphics_draw_line(ctx, GPoint(x, y+4), GPoint(x-4, y+10)); // left leg
  graphics_draw_line(ctx, GPoint(x, y+4), GPoint(x+4, y+10)); // right leg
}

// Heart (~10x10px)
static void draw_heart(GContext *ctx, int x, int y) {
  doodle_color(ctx);
  graphics_fill_circle(ctx, GPoint(x-3, y-2), 3);
  graphics_fill_circle(ctx, GPoint(x+3, y-2), 3);
  for(int i = 0; i < 6; i++) {
    int hw = 5 - i;
    if(hw > 0)
      graphics_fill_rect(ctx, GRect(x-hw, y+i, hw*2+1, 1), 0, GCornerNone);
  }
}

// 3D cube (~14x16px)
static void draw_cube(GContext *ctx, int x, int y) {
  doodle_color(ctx);
  // Front face
  graphics_draw_rect(ctx, GRect(x-5, y-3, 10, 10));
  // Back edges
  graphics_draw_line(ctx, GPoint(x-5, y-3), GPoint(x-1, y-7));
  graphics_draw_line(ctx, GPoint(x+5, y-3), GPoint(x+9, y-7));
  graphics_draw_line(ctx, GPoint(x-1, y-7), GPoint(x+9, y-7));
  graphics_draw_line(ctx, GPoint(x+5, y+7), GPoint(x+9, y+3));
  graphics_draw_line(ctx, GPoint(x+9, y-7), GPoint(x+9, y+3));
}

// Star (~12x12px)
static void draw_star(GContext *ctx, int x, int y) {
  doodle_color(ctx);
  // 5-pointed star via lines
  graphics_draw_line(ctx, GPoint(x, y-7), GPoint(x+2, y-2));
  graphics_draw_line(ctx, GPoint(x+2, y-2), GPoint(x+7, y-2));
  graphics_draw_line(ctx, GPoint(x+7, y-2), GPoint(x+3, y+2));
  graphics_draw_line(ctx, GPoint(x+3, y+2), GPoint(x+5, y+7));
  graphics_draw_line(ctx, GPoint(x+5, y+7), GPoint(x, y+4));
  graphics_draw_line(ctx, GPoint(x, y+4), GPoint(x-5, y+7));
  graphics_draw_line(ctx, GPoint(x-5, y+7), GPoint(x-3, y+2));
  graphics_draw_line(ctx, GPoint(x-3, y+2), GPoint(x-7, y-2));
  graphics_draw_line(ctx, GPoint(x-7, y-2), GPoint(x-2, y-2));
  graphics_draw_line(ctx, GPoint(x-2, y-2), GPoint(x, y-7));
}

// Smiley (~10x10px)
static void draw_smiley(GContext *ctx, int x, int y) {
  doodle_color(ctx);
  graphics_draw_circle(ctx, GPoint(x, y), 6);
  graphics_fill_circle(ctx, GPoint(x-2, y-2), 1);
  graphics_fill_circle(ctx, GPoint(x+2, y-2), 1);
  // Smile arc (approximated)
  graphics_draw_line(ctx, GPoint(x-3, y+2), GPoint(x-1, y+4));
  graphics_draw_line(ctx, GPoint(x-1, y+4), GPoint(x+1, y+4));
  graphics_draw_line(ctx, GPoint(x+1, y+4), GPoint(x+3, y+2));
}

// Flower (~12x12px)
static void draw_flower(GContext *ctx, int x, int y) {
  doodle_color(ctx);
  // Petals
  graphics_draw_circle(ctx, GPoint(x, y-4), 3);
  graphics_draw_circle(ctx, GPoint(x+4, y-1), 3);
  graphics_draw_circle(ctx, GPoint(x+2, y+3), 3);
  graphics_draw_circle(ctx, GPoint(x-2, y+3), 3);
  graphics_draw_circle(ctx, GPoint(x-4, y-1), 3);
  // Center
  graphics_fill_circle(ctx, GPoint(x, y), 2);
}

// Draw 1-2 random doodles based on a seed
static void draw_doodles(GContext *ctx, int w, int h, int seed) {
  // Use seed to pick consistent doodles per screen
  int r = seed * 7 + 13;
  int num = 1 + (r % 2); // 1-2 doodles
  void (*doodle_fns[])(GContext*, int, int) = {
    draw_cool_s, draw_stickfig, draw_heart,
    draw_cube, draw_star, draw_smiley, draw_flower
  };
  int num_fns = 7;

  for(int d = 0; d < num; d++) {
    int fn_idx = (r + d * 31) % num_fns;
    // Position: right margin area or bottom corners
    int dx, dy;
    if(d == 0) {
      dx = w - PBL_IF_ROUND_ELSE(38, 18);
      dy = h * 30 / 100 + ((r >> 2) % 60);
    } else {
      dx = PBL_IF_ROUND_ELSE(30, 14);
      dy = h * 60 / 100 + ((r >> 4) % 40);
    }
    doodle_fns[fn_idx](ctx, dx, dy);
  }
}

// ============================================================================
// NOTEBOOK DRAWING
// ============================================================================
static void draw_notebook(GContext *ctx, int w, int h, int line_start) {
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorFromHEX(0xFFFFAA));
  #else
  graphics_context_set_fill_color(ctx, GColorWhite);
  #endif
  graphics_fill_rect(ctx, GRect(0, 0, w, h), 0, GCornerNone);

  #ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorFromHEX(0x5555FF));
  #else
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  #endif
  graphics_context_set_stroke_width(ctx, 1);
  for(int y = line_start; y < h; y += 20)
    graphics_draw_line(ctx, GPoint(0, y), GPoint(w, y));

  #ifdef PBL_COLOR
  int mx = PBL_IF_ROUND_ELSE(40, 28);
  graphics_context_set_stroke_color(ctx, GColorFromHEX(0xFF5555));
  graphics_draw_line(ctx, GPoint(mx, 0), GPoint(mx, h));
  #endif
}

// Draw a down arrow indicator for scrollable lists
static void draw_scroll_arrow(GContext *ctx, int w, int y) {
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  #else
  graphics_context_set_fill_color(ctx, GColorBlack);
  #endif
  int cx = w / 2;
  for(int i = 0; i < 4; i++)
    graphics_fill_rect(ctx, GRect(cx - 4 + i, y + i, 9 - i*2, 1), 0, GCornerNone);
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
    draw_doodles(ctx, w, h, 1);

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
    int total_items = NUM_CATS + 1; // +1 for GO
    int visible = (h - ly - PBL_IF_ROUND_ELSE(20, 8)) / item_h;

    if(s_cursor < s_scroll) s_scroll = s_cursor;
    if(s_cursor >= s_scroll + visible) s_scroll = s_cursor - visible + 1;

    for(int vi = 0; vi < visible && s_scroll + vi < total_items; vi++) {
      int idx = s_scroll + vi;
      int iy = ly + vi * item_h;
      bool is_cur = (idx == s_cursor);
      bool is_go = (idx == NUM_CATS);

      if(is_go) {
        if(s_num_active >= 2) {
          // Pencil-shaped GO button
          int bw = 90, bx = (w - bw) / 2;
          #ifdef PBL_COLOR
          // Pencil body (yellow)
          graphics_context_set_fill_color(ctx, GColorFromHEX(0xFFCC00));
          graphics_fill_rect(ctx, GRect(bx + 10, iy, bw - 20, item_h - 1), 2, GCornersAll);
          // Pencil tip (triangle-ish)
          graphics_context_set_fill_color(ctx, GColorFromHEX(0xFFCC00));
          graphics_fill_rect(ctx, GRect(bx + 4, iy + 2, 10, item_h - 5), 0, GCornerNone);
          graphics_context_set_fill_color(ctx, GColorBlack);
          // Tip point
          for(int ti = 0; ti < 4; ti++)
            graphics_fill_rect(ctx, GRect(bx + ti, iy + item_h/2 - 2 + ti, 1, 4 - ti*2 > 0 ? 4 - ti*2 : 1), 0, GCornerNone);
          // Eraser end (pink)
          graphics_context_set_fill_color(ctx, GColorFromHEX(0xFF9999));
          graphics_fill_rect(ctx, GRect(bx + bw - 16, iy + 1, 12, item_h - 3), 2, GCornersAll);
          // Highlight border
          if(is_cur) {
            graphics_context_set_stroke_color(ctx, GColorWhite);
            graphics_context_set_stroke_width(ctx, 2);
            graphics_draw_round_rect(ctx, GRect(bx + 2, iy - 1, bw - 4, item_h + 1), 3);
          }
          // Text on pencil
          graphics_context_set_text_color(ctx, GColorBlack);
          #else
          graphics_context_set_fill_color(ctx, is_cur ? GColorWhite : GColorLightGray);
          graphics_fill_rect(ctx, GRect(bx, iy, bw, item_h - 1), 4, GCornersAll);
          graphics_context_set_text_color(ctx, GColorBlack);
          #endif
          char gb[16]; snprintf(gb, sizeof(gb), "GO! (%d)", s_num_active);
          graphics_draw_text(ctx, gb, f_md,
            GRect(bx, iy - 1, bw, item_h),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
        }
      } else {
        bool filled = (s_pick_count[idx] == PICKS_PER && s_cat_active[idx]);
        // White highlight for cursor
        if(is_cur) {
          graphics_context_set_fill_color(ctx, GColorWhite);
          graphics_fill_rect(ctx, GRect(margin, iy, w - margin - pad, item_h - 1), 0, GCornerNone);
        }
        #ifdef PBL_COLOR
        graphics_context_set_text_color(ctx, filled ? GColorFromHEX(0x005500) : GColorBlack);
        #else
        graphics_context_set_text_color(ctx, GColorBlack);
        #endif
        graphics_draw_text(ctx, s_cat_names[idx], is_cur ? f_md : f_sm,
          GRect(margin + 4, iy, w - margin - pad - 40, item_h),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        if(filled) {
          graphics_draw_text(ctx, "4/4", f_sm,
            GRect(margin, iy, w - margin - pad - 4, item_h),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
        }
      }
    }
    // Down arrow if more items below
    if(s_scroll + visible < total_items)
      draw_scroll_arrow(ctx, w, ly + visible * item_h);
  }

  // ======== OPTION PICKER ========
  else if(s_state == ST_OPTS) {
    int title_y = PBL_IF_ROUND_ELSE(pad + 6, pad);
    draw_notebook(ctx, w, h, title_y + 24);
    draw_doodles(ctx, w, h, s_cur_cat + 10);

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

      // White highlight for cursor
      if(is_cur) {
        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_fill_rect(ctx, GRect(margin, iy, w - margin - pad, item_h - 1), 0, GCornerNone);
      }

      // Check or blank
      #ifdef PBL_COLOR
      graphics_context_set_text_color(ctx, is_sel ? GColorFromHEX(0x005500) : GColorBlack);
      #else
      graphics_context_set_text_color(ctx, GColorBlack);
      #endif
      graphics_draw_text(ctx, is_sel ? "*" : " ", f_md,
        GRect(margin - 12, iy - 1, 14, item_h),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

      graphics_draw_text(ctx, get_opt(s_cur_cat, idx), is_cur ? f_md : f_sm,
        GRect(margin + 4, iy, w - margin - pad - 8, item_h),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
    // Down arrow
    if(s_opt_scroll + visible < NUM_OPTS)
      draw_scroll_arrow(ctx, w, ly + visible * item_h);
  }

  // ======== READY (spinning number) ========
  else if(s_state == ST_READY) {
    draw_notebook(ctx, w, h, 40);
    draw_doodles(ctx, w, h, 20);
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

    // Spinning number display
    char num_buf[4];
    snprintf(num_buf, sizeof(num_buf), "%d", s_spin_number);
    #ifdef PBL_COLOR
    graphics_context_set_text_color(ctx, GColorFromHEX(0xAA0000));
    #endif
    graphics_draw_text(ctx, num_buf, f_lg,
      GRect(0, cy + 64, w, 34),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, "Press SELECT to stop!", f_sm,
      GRect(0, cy + 100, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // ======== ELIMINATION ========
  else if(s_state == ST_ELIM) {
    draw_notebook(ctx, w, h, 40);
    draw_doodles(ctx, w, h, 30 + s_elim_show_cat);
    int cy = h / 2 - 50;

    // Lucky number
    char lucky[12]; snprintf(lucky, sizeof(lucky), "#%d", s_elim_number);
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
    graphics_draw_line(ctx, GPoint(w/4, cy + 62), GPoint(w*3/4, cy + 62));

    // Show remaining per category
    graphics_context_set_text_color(ctx, GColorDarkGray);
    if(s_elim_done) {
      #ifdef PBL_COLOR
      graphics_context_set_text_color(ctx, GColorFromHEX(0x005500));
      #endif
      graphics_draw_text(ctx, "Your fate is sealed!", f_md,
        GRect(0, cy + 82, w, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
      graphics_context_set_text_color(ctx, GColorDarkGray);
      graphics_draw_text(ctx, "SELECT: reveal future!", f_sm,
        GRect(0, cy + 106, w, 16),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    } else {
      // Show how many categories still undecided
      int cats_left = 0;
      for(int c = 0; c < NUM_CATS; c++)
        if(s_cat_active[c] && remaining_in_cat(c) > 1) cats_left++;
      char rem[24]; snprintf(rem, sizeof(rem), "%d categor%s left",
        cats_left, cats_left == 1 ? "y" : "ies");
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
    draw_doodles(ctx, w, h, 50);

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

    int active_cats[MAX_ACTIVE]; int ac = 0;
    for(int c = 0; c < NUM_CATS; c++)
      if(s_cat_active[c]) active_cats[ac++] = c;

    if(s_fortune_scroll < 0) s_fortune_scroll = 0;
    if(ac > visible && s_fortune_scroll > ac - visible) s_fortune_scroll = ac - visible;
    if(s_fortune_scroll < 0) s_fortune_scroll = 0;

    for(int vi = 0; vi < visible && s_fortune_scroll + vi < ac; vi++) {
      int cat = active_cats[s_fortune_scroll + vi];
      int iy = ly + vi * item_h;
      int opt = get_result(cat);

      graphics_context_set_text_color(ctx, GColorDarkGray);
      graphics_draw_text(ctx, s_cat_names[cat], f_sm,
        GRect(margin + 4, iy, w - margin - pad, 16),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      graphics_context_set_text_color(ctx, GColorBlack);
      graphics_draw_text(ctx, get_opt_fortune(cat, opt), f_md,
        GRect(margin + 4, iy + 12, w - margin - pad, 20),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }

    if(ac > visible && s_fortune_scroll + visible < ac)
      draw_scroll_arrow(ctx, w, ly + visible * item_h);

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
      if(s_num_active >= 2) {
        for(int c = 0; c < NUM_CATS; c++)
          for(int i = 0; i < PICKS_PER; i++) s_eliminated[c][i] = false;
        s_elim_pos = -1;
        s_elim_done = false;
        start_spinning();
        s_state = ST_READY;
      }
    } else {
      int cat = s_cursor;
      if(s_pick_count[cat] == PICKS_PER && s_cat_active[cat]) {
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
        // Save and return
        int cat = s_cur_cat;
        s_pick_count[cat] = 0;
        for(int i = 0; i < NUM_OPTS; i++)
          if(s_opt_selected[i]) s_picks[cat][s_pick_count[cat]++] = i;
        if(!s_cat_active[cat]) { s_cat_active[cat] = true; s_num_active++; }
        // Jump to next unfilled category or GO
        s_cursor = NUM_CATS; // default to GO
        s_scroll = 0;
        for(int c = s_cur_cat + 1; c < NUM_CATS; c++) {
          if(!s_cat_active[c] || s_pick_count[c] < PICKS_PER) {
            s_cursor = c; break;
          }
        }
        s_state = ST_CATS;
      } else {
        // Auto-advance to next unselected
        for(int i = 1; i < NUM_OPTS; i++) {
          int next = (s_cursor + i) % NUM_OPTS;
          if(!s_opt_selected[next]) { s_cursor = next; break; }
        }
      }
    }
  }
  else if(s_state == ST_READY) {
    stop_spinning();
    do_elimination();
    s_state = ST_ELIM;
  }
  else if(s_state == ST_ELIM) {
    if(s_elim_done) {
      s_fortune_scroll = 0;
      s_state = ST_FORTUNE;
    } else {
      do_elimination();
      if(s_elim_done) vibes_long_pulse();
    }
  }
  else if(s_state == ST_FORTUNE) {
    for(int c = 0; c < NUM_CATS; c++) { s_cat_active[c] = false; s_pick_count[c] = 0; }
    s_num_active = 0;
    s_cursor = 0; s_scroll = 0;
    s_state = ST_CATS;
  }
  if(s_canvas) layer_mark_dirty(s_canvas);
}

static void up_click(ClickRecognizerRef ref, void *ctx) {
  if(s_state == ST_CATS) {
    // Wrap around
    int total = NUM_CATS + (s_num_active >= 2 ? 1 : 0);
    s_cursor = (s_cursor + total - 1) % total;
  } else if(s_state == ST_OPTS) {
    s_cursor = (s_cursor + NUM_OPTS - 1) % NUM_OPTS;
  } else if(s_state == ST_FORTUNE) {
    if(s_fortune_scroll > 0) s_fortune_scroll--;
  }
  if(s_canvas) layer_mark_dirty(s_canvas);
}

static void down_click(ClickRecognizerRef ref, void *ctx) {
  if(s_state == ST_CATS) {
    int total = NUM_CATS + (s_num_active >= 2 ? 1 : 0);
    s_cursor = (s_cursor + 1) % total;
  } else if(s_state == ST_OPTS) {
    s_cursor = (s_cursor + 1) % NUM_OPTS;
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
      s_cat_active[cat] = false; s_num_active--;
    }
    s_cursor = s_cur_cat; s_scroll = 0;
    s_state = ST_CATS;
  } else if(s_state == ST_CATS) {
    window_stack_pop(true);
  } else if(s_state == ST_FORTUNE) {
    for(int c = 0; c < NUM_CATS; c++) { s_cat_active[c] = false; s_pick_count[c] = 0; }
    s_num_active = 0; s_cursor = 0; s_scroll = 0;
    s_state = ST_CATS;
  } else if(s_state == ST_READY) {
    stop_spinning();
    s_cursor = 0; s_scroll = 0;
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
  if(s_spinning) stop_spinning();
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
