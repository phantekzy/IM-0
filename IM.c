#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ROWLABELS[12] = {"12", "11", "0", "1", "2", "3",
                                    "4",  "5",  "6", "7", "8", "9"};

typedef struct {
  char c;
  const char *rows;
} CardEntry;

static const CardEntry CARD_TABLE[] = {
    {'A', "12,1"},   {'B', "12,2"},  {'C', "12,3"},   {'D', "12,4"},
    {'E', "12,5"},   {'F', "12,6"},  {'G', "12,7"},   {'H', "12,8"},
    {'I', "12,9"},   {'J', "11,1"},  {'K', "11,2"},   {'L', "11,3"},
    {'M', "11,4"},   {'N', "11,5"},  {'O', "11,6"},   {'P', "11,7"},
    {'Q', "11,8"},   {'R', "11,9"},  {'S', "0,2"},    {'T', "0,3"},
    {'U', "0,4"},    {'V', "0,5"},   {'W', "0,6"},    {'X', "0,7"},
    {'Y', "0,8"},    {'Z', "0,9"},   {'0', "0"},      {'1', "1"},
    {'2', "2"},      {'3', "3"},     {'4', "4"},      {'5', "5"},
    {'6', "6"},      {'7', "7"},     {'8', "8"},      {'9', "9"},
    {' ', ""},       {'&', "12"},    {'-', "11"},     {'/', "0,1"},
    {'.', "12,3,8"}, {',', "0,3,8"}, {'$', "11,3,8"}, {'*', "11,4,8"},
    {'%', "0,4,8"},  {'#', "3,8"},   {'@', "4,8"},    {'\'', "8,5"},
};
#define CARD_TABLE_N (int)(sizeof(CARD_TABLE) / sizeof(CARD_TABLE[0]))

static int row_index(const char *label) {
  for (int i = 0; i < 12; i++)
    if (strcmp(ROWLABELS[i], label) == 0)
      return i;
  return -1;
}

static int char_to_mask(char c) {
  c = (char)toupper((unsigned char)c);
  for (int i = 0; i < CARD_TABLE_N; i++) {
    if (CARD_TABLE[i].c == c) {
      int mask = 0;
      char buf[32];
      strncpy(buf, CARD_TABLE[i].rows, sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = 0;
      if (buf[0] == 0)
        return 0;
      char *tok = strtok(buf, ",");
      while (tok) {
        int ri = row_index(tok);
        if (ri >= 0)
          mask |= (1 << ri);
        tok = strtok(NULL, ",");
      }
      return mask;
    }
  }
  return 0; /* unknown character -> blank column, same as a real keypunch */
}

static char mask_to_char(int mask) {
  if (mask == 0)
    return ' ';
  for (int i = 0; i < CARD_TABLE_N; i++) {
    char buf[32];
    strncpy(buf, CARD_TABLE[i].rows, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    int m = 0;
    if (buf[0] != 0) {
      char *tok = strtok(buf, ",");
      while (tok) {
        int ri = row_index(tok);
        if (ri >= 0)
          m |= (1 << ri);
        tok = strtok(NULL, ",");
      }
    }
    if (m == mask)
      return CARD_TABLE[i].c;
  }
  return '?';
}

#define COLS_PER_CARD 80

static void svg_card(FILE *out, const int *masks, int n) {
  const double W = 737.5, H = 325.0;
  const double left = 28, top = 22; /* margins */
  const double colpitch = 8.7, rowpitch = 25.0;
  const double holew = 4.4, holeh = 12.0;

  fprintf(out,
          "<div class=\"card\"><svg viewBox=\"0 0 %.1f %.1f\" "
          "xmlns=\"http://www.w3.org/2000/svg\">\n",
          W, H);
  fprintf(out,
          "<rect x=\"0\" y=\"0\" width=\"%.1f\" height=\"%.1f\" "
          "class=\"cardstock\"/>\n",
          W, H);

  fprintf(out, "<polygon points=\"0,0 22,0 0,22\" class=\"cardbg\"/>\n");

  for (int col = 0; col < COLS_PER_CARD; col++) {
    int mask = (col < n) ? masks[col] : 0;
    double cx = left + col * colpitch;
    for (int r = 0; r < 12; r++) {
      if (mask & (1 << r)) {
        double cy = top + r * rowpitch;
        fprintf(out,
                "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
                "class=\"hole\"/>\n",
                cx - holew / 2, cy - holeh / 2, holew, holeh);
      }
    }
  }
  fprintf(out, "</svg></div>\n");
}

typedef struct {
  char c;
  const char *code;
} MorseEntry;
static const MorseEntry MORSE_TABLE[] = {
    {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},
    {'E', "."},     {'F', "..-."},  {'G', "--."},   {'H', "...."},
    {'I', ".."},    {'J', ".---"},  {'K', "-.-"},   {'L', ".-.."},
    {'M', "--"},    {'N', "-."},    {'O', "---"},   {'P', ".--."},
    {'Q', "--.-"},  {'R', ".-."},   {'S', "..."},   {'T', "-"},
    {'U', "..-"},   {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},
    {'Y', "-.--"},  {'Z', "--.."},  {'0', "-----"}, {'1', ".----"},
    {'2', "..---"}, {'3', "...--"}, {'4', "....-"}, {'5', "....."},
    {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."},
};
#define MORSE_N (int)(sizeof(MORSE_TABLE) / sizeof(MORSE_TABLE[0]))

static const char *char_to_morse(char c) {
  c = (char)toupper((unsigned char)c);
  for (int i = 0; i < MORSE_N; i++)
    if (MORSE_TABLE[i].c == c)
      return MORSE_TABLE[i].code;
  return NULL;
}
static char morse_to_char(const char *code) {
  for (int i = 0; i < MORSE_N; i++)
    if (strcmp(MORSE_TABLE[i].code, code) == 0)
      return MORSE_TABLE[i].c;
  return '?';
}

static void svg_tape(FILE *out, const char *message) {
  fprintf(out, "<div class=\"tape\">\n");
  double x = 20, y = 18;
  const double rowH = 26, dot = 6, dashW = 18, gap = 7, letterGap = 16,
               wordGap = 26, rightEdge = 740;
  fprintf(
      out,
      "<svg viewBox=\"0 0 760 %.0f\" xmlns=\"http://www.w3.org/2000/svg\">\n",
      (y + rowH * ((double)strlen(message) / 6 + 3)));
  fprintf(out, "<rect x=\"0\" y=\"0\" width=\"760\" height=\"3000\" "
               "class=\"tapebg\"/>\n");

  for (const char *p = message; *p; p++) {
    if (*p == ' ') {
      x += wordGap;
      if (x > rightEdge) {
        x = 20;
        y += rowH;
      }
      continue;
    }
    const char *code = char_to_morse(*p);
    if (!code)
      continue;
    for (const char *m = code; *m; m++) {
      if (x > rightEdge) {
        x = 20;
        y += rowH;
      }
      if (*m == '.')
        fprintf(out,
                "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"%.1f\" class=\"mark\"/>\n",
                x + dot / 2, y, dot / 2);
      else
        fprintf(out,
                "<rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" "
                "rx=\"2\" class=\"mark\"/>\n",
                x, y - dot / 2, dashW, dot);
      x += (*m == '.' ? dot : dashW) + gap;
    }
    x += letterGap;
  }
  fprintf(out,
          "<line x1=\"0\" y1=\"%.1f\" x2=\"760\" y2=\"%.1f\" "
          "class=\"tapecenter\"/>\n",
          0.0, 0.0);
  fprintf(out, "</svg></div>\n");
}

static const char *CSS =
    "body{background:#3a3a3a;font-family:'Courier "
    "New',monospace;margin:0;padding:24px;}"
    ".sheet{background:#e9e2c9;margin:0 auto 24px;box-shadow:0 4px 18px "
    "rgba(0,0,0,.5);}"
    ".card{width:7.375in;height:3.25in;background:#e9e2c9;position:relative;"
    "margin:0 auto 18px;}"
    ".cardstock{fill:#e9e2c9;stroke:#8c8362;stroke-width:1;}"
    ".cardbg{fill:#3a3a3a;}"
    ".hole{fill:#3a3a3a;}"
    ".tape{width:7.9in;background:#efe9d6;margin:0 auto;padding:6px 0;}"
    ".tapebg{fill:#efe9d6;}"
    ".tapecenter{stroke:#c9c0a0;stroke-width:.5;}"
    ".mark{fill:#2a2a2a;}"
    "h1{color:#eee;font-size:14px;text-align:center;letter-spacing:2px;}"
    "@media print{"
    "body{background:#fff;padding:0;}"
    "h1,.nohint{display:none;}"
    ".card{page-break-after:always;box-shadow:none;}"
    ".tape{box-shadow:none;}"
    "}";

static void write_html_header(FILE *out, const char *title) {
  fprintf(
      out,
      "<!DOCTYPE html><html><head><meta "
      "charset=\"utf-8\"><title>%s</title><style>%s</style></head><body>\n<h1 "
      "class=\"nohint\">%s -- print this page (Ctrl/Cmd+P)</h1>\n",
      title, CSS, title);
}
static void write_html_footer(FILE *out) { fprintf(out, "</body></html>\n"); }

static void card_encode(const char *message) {
  int n = (int)strlen(message);
  FILE *html = fopen("card.html", "w");
  FILE *holes = fopen("card.holes", "w");
  write_html_header(html, "PUNCH CARD");

  int pos = 0;
  while (pos < n) {
    int chunk = (n - pos > COLS_PER_CARD) ? COLS_PER_CARD : (n - pos);
    int masks[COLS_PER_CARD];
    memset(masks, 0, sizeof(masks));
    for (int i = 0; i < chunk; i++) {
      masks[i] = char_to_mask(message[pos + i]);
      fprintf(holes, "%d:%d\n", pos + i, masks[i]);
    }
    svg_card(html, masks, chunk);
    pos += chunk;
  }
  write_html_footer(html);
  fclose(html);
  fclose(holes);
  printf("Wrote card.html (print this) and card.holes (the raw hole data -- "
         "the 'key').\n");
}

static void card_decode(const char *holefile) {
  FILE *f = fopen(holefile, "r");
  if (!f) {
    fprintf(stderr, "Cannot open %s\n", holefile);
    exit(1);
  }
  int idx, mask;
  char out[8192];
  int maxidx = -1;
  memset(out, ' ', sizeof(out));
  while (fscanf(f, "%d:%d", &idx, &mask) == 2) {
    if (idx >= 0 && idx < (int)sizeof(out) - 1) {
      out[idx] = mask_to_char(mask);
      if (idx > maxidx)
        maxidx = idx;
    }
  }
  out[maxidx + 1] = 0;
  fclose(f);
  printf("Decoded message: %s\n", out);
}

static void tape_encode(const char *message) {
  FILE *html = fopen("tape.html", "w");
  FILE *morse = fopen("tape.morse", "w");
  write_html_header(html, "TICKER TAPE");
  svg_tape(html, message);
  write_html_footer(html);

  for (const char *p = message; *p; p++) {
    if (*p == ' ') {
      fprintf(morse, "/ ");
      continue;
    }
    const char *code = char_to_morse(*p);
    if (code)
      fprintf(morse, "%s ", code);
  }
  fprintf(morse, "\n");
  fclose(html);
  fclose(morse);
  printf("Wrote tape.html (print this) and tape.morse (the raw code -- the "
         "'key').\n");
}

static void tape_decode(const char *morsefile) {
  FILE *f = fopen(morsefile, "r");
  if (!f) {
    fprintf(stderr, "Cannot open %s\n", morsefile);
    exit(1);
  }
  char tok[256];
  printf("Decoded tape message: ");
  /* Read space-separated morse tokens and '/' for spaces */
  while (fscanf(f, "%255s", tok) == 1) {
    if (strcmp(tok, "/") == 0) {
      putchar(' ');
    } else {
      putchar(morse_to_char(tok));
    }
  }
  putchar('\n');
  fclose(f);
}

int main(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr, "IM-0 CLI by Phantekzy\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s card encode \"Your message here\"\n", argv[0]);
    fprintf(stderr, "  %s card decode card.holes\n", argv[0]);
    fprintf(stderr, "  %s tape encode \"Your message here\"\n", argv[0]);
    fprintf(stderr, "  %s tape decode tape.morse\n", argv[0]);
    return 1;
  }

  const char *mode = argv[1];
  const char *action = argv[2];
  const char *data = argv[3];

  if (strcmp(mode, "card") == 0) {
    if (strcmp(action, "encode") == 0) {
      card_encode(data);
    } else if (strcmp(action, "decode") == 0) {
      card_decode(data);
    } else {
      fprintf(stderr, "Unknown card action: %s\n", action);
      return 1;
    }
  } else if (strcmp(mode, "tape") == 0) {
    if (strcmp(action, "encode") == 0) {
      tape_encode(data);
    } else if (strcmp(action, "decode") == 0) {
      tape_decode(data);
    } else {
      fprintf(stderr, "Unknown tape action: %s\n", action);
      return 1;
    }
  } else {
    fprintf(stderr, "Unknown mode: %s. Use 'card' or 'tape'.\n", mode);
    return 1;
  }

  return 0;
}
