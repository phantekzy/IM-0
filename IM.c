#include <ctype.h>
#include <stdint.h>
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

/* ============================================================
 * Minimal, dependency-free PNG writer/reader.
 *
 * We never need real compression, so every IDAT is written as
 * "stored" (uncompressed) DEFLATE blocks. That means we don't
 * need zlib to write PNGs, AND we can write a tiny matching
 * inflate that only has to understand stored blocks to read
 * them back. This is what lets `decode` accept a .png image
 * directly instead of only the raw .holes/.morse key files.
 *
 * Caveat: because the reader only understands the simple
 * uncompressed stream we produce, it can only read PNGs that
 * this program generated (or an exact byte-for-byte copy of
 * one). If a PNG gets re-saved/re-compressed by another editor
 * it will use real DEFLATE compression and this reader will
 * report it as unsupported rather than crash.
 * ============================================================ */

static uint32_t crc_table[256];
static int crc_table_ready = 0;
static void make_crc_table(void) {
  for (uint32_t n = 0; n < 256; n++) {
    uint32_t c = n;
    for (int k = 0; k < 8; k++)
      c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
    crc_table[n] = c;
  }
  crc_table_ready = 1;
}
static uint32_t crc32_calc(const unsigned char *buf, size_t len) {
  if (!crc_table_ready)
    make_crc_table();
  uint32_t c = 0xffffffffu;
  for (size_t i = 0; i < len; i++)
    c = crc_table[(c ^ buf[i]) & 0xff] ^ (c >> 8);
  return c ^ 0xffffffffu;
}

static uint32_t adler32_calc(const unsigned char *buf, size_t len) {
  uint32_t a = 1, b = 0;
  const uint32_t MOD = 65521;
  for (size_t i = 0; i < len; i++) {
    a = (a + buf[i]) % MOD;
    b = (b + a) % MOD;
  }
  return (b << 16) | a;
}

static void put_be32(unsigned char *p, uint32_t v) {
  p[0] = (unsigned char)(v >> 24);
  p[1] = (unsigned char)(v >> 16);
  p[2] = (unsigned char)(v >> 8);
  p[3] = (unsigned char)v;
}
static uint32_t get_be32(const unsigned char *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void write_png_chunk(FILE *f, const char type[4],
                            const unsigned char *data, uint32_t len) {
  unsigned char lenbuf[4];
  put_be32(lenbuf, len);
  fwrite(lenbuf, 1, 4, f);
  fwrite(type, 1, 4, f);
  if (len)
    fwrite(data, 1, len, f);
  unsigned char *crcbuf = malloc(4 + len);
  memcpy(crcbuf, type, 4);
  if (len)
    memcpy(crcbuf + 4, data, len);
  uint32_t crc = crc32_calc(crcbuf, 4 + len);
  free(crcbuf);
  unsigned char crcout[4];
  put_be32(crcout, crc);
  fwrite(crcout, 1, 4, f);
}

/* 8-bit grayscale PNG, one byte per pixel, filter type "None" on every row */
static int write_png_gray(const char *filename, int w, int h,
                          const unsigned char *pixels) {
  FILE *f = fopen(filename, "wb");
  if (!f)
    return -1;
  static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  fwrite(sig, 1, 8, f);

  unsigned char ihdr[13];
  put_be32(ihdr, (uint32_t)w);
  put_be32(ihdr + 4, (uint32_t)h);
  ihdr[8] = 8;  /* bit depth */
  ihdr[9] = 0;  /* color type: grayscale */
  ihdr[10] = 0; /* compression */
  ihdr[11] = 0; /* filter */
  ihdr[12] = 0; /* interlace */
  write_png_chunk(f, "IHDR", ihdr, 13);

  /* raw = one filter byte (0) + w pixel bytes, per row */
  size_t rawlen = (size_t)h * (1 + (size_t)w);
  unsigned char *raw = malloc(rawlen);
  for (int y = 0; y < h; y++) {
    unsigned char *row = raw + (size_t)y * (1 + w);
    row[0] = 0;
    memcpy(row + 1, pixels + (size_t)y * w, w);
  }

  /* zlib stream: header + stored deflate blocks + adler32 */
  size_t maxblocks = rawlen / 65535 + 1;
  size_t zcap = 2 + rawlen + maxblocks * 5 + 4;
  unsigned char *z = malloc(zcap);
  size_t zi = 0;
  z[zi++] = 0x78;
  z[zi++] = 0x01;
  size_t off = 0;
  while (1) {
    size_t remain = rawlen - off;
    size_t blocklen = remain > 65535 ? 65535 : remain;
    unsigned char bfinal = (blocklen == remain) ? 1 : 0;
    z[zi++] = bfinal;
    uint16_t len16 = (uint16_t)blocklen;
    uint16_t nlen16 = (uint16_t)~len16;
    z[zi++] = (unsigned char)(len16 & 0xff);
    z[zi++] = (unsigned char)(len16 >> 8);
    z[zi++] = (unsigned char)(nlen16 & 0xff);
    z[zi++] = (unsigned char)(nlen16 >> 8);
    memcpy(z + zi, raw + off, blocklen);
    zi += blocklen;
    off += blocklen;
    if (bfinal)
      break;
    if (blocklen == 0) /* rawlen == 0 edge case */
      break;
  }
  uint32_t adler = adler32_calc(raw, rawlen);
  unsigned char abuf[4];
  put_be32(abuf, adler);
  memcpy(z + zi, abuf, 4);
  zi += 4;

  write_png_chunk(f, "IDAT", z, (uint32_t)zi);
  write_png_chunk(f, "IEND", NULL, 0);

  free(raw);
  free(z);
  fclose(f);
  return 0;
}

/* Inflate a stream that only contains stored (uncompressed) DEFLATE
 * blocks, as produced by write_png_gray above. Returns malloc'd buffer
 * of *outlen bytes, or NULL on failure (e.g. real compression used). */
static unsigned char *inflate_stored(const unsigned char *zdata, size_t zlen,
                                     size_t expected, size_t *outlen) {
  (void)expected;
  if (zlen < 2)
    return NULL;
  size_t p = 2; /* skip zlib 2-byte header */
  unsigned char *out = malloc(1);
  size_t oi = 0;
  size_t cap = 1;
  int bfinal = 0;
  while (!bfinal) {
    if (p >= zlen)
      goto fail;
    unsigned char hdr = zdata[p++];
    bfinal = hdr & 1;
    int btype = (hdr >> 1) & 3;
    if (btype != 0) {
      goto fail;
    }
    if (p + 4 > zlen)
      goto fail;
    uint16_t len16 = (uint16_t)(zdata[p] | (zdata[p + 1] << 8));
    p += 4; /* skip LEN + NLEN */
    if (p + len16 > zlen)
      goto fail;
    if (oi + len16 > cap) {
      cap = oi + len16 + 4096;
      unsigned char *grown = realloc(out, cap);
      if (!grown)
        goto fail;
      out = grown;
    }
    memcpy(out + oi, zdata + p, len16);
    oi += len16;
    p += len16;
  }
  *outlen = oi;
  return out;
fail:
  free(out);
  return NULL;
}

/* Reads an 8-bit grayscale (or RGB/RGBA, averaged down to grayscale) PNG
 * that uses only "None" row filters, as produced by write_png_gray. */
static int read_png_gray(const char *filename, int *out_w, int *out_h,
                         unsigned char **out_pixels) {
  FILE *f = fopen(filename, "rb");
  if (!f) {
    fprintf(stderr, "Cannot open %s\n", filename);
    return -1;
  }
  unsigned char sig[8];
  static const unsigned char pngsig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (fread(sig, 1, 8, f) != 8 || memcmp(sig, pngsig, 8) != 0) {
    fprintf(stderr, "%s is not a PNG file\n", filename);
    fclose(f);
    return -1;
  }

  int w = 0, h = 0, bitdepth = 0, colortype = 0;
  unsigned char *idat = NULL;
  size_t idatlen = 0;

  while (1) {
    unsigned char lenbuf[4], type[5] = {0};
    if (fread(lenbuf, 1, 4, f) != 4)
      break;
    uint32_t len = get_be32(lenbuf);
    if (fread(type, 1, 4, f) != 4)
      break;
    unsigned char *data = len ? malloc(len) : NULL;
    if (len && fread(data, 1, len, f) != len) {
      free(data);
      break;
    }
    fseek(f, 4, SEEK_CUR); /* skip CRC */

    if (memcmp(type, "IHDR", 4) == 0 && len >= 13) {
      w = (int)get_be32(data);
      h = (int)get_be32(data + 4);
      bitdepth = data[8];
      colortype = data[9];
    } else if (memcmp(type, "IDAT", 4) == 0) {
      unsigned char *grown = realloc(idat, idatlen + len);
      idat = grown;
      memcpy(idat + idatlen, data, len);
      idatlen += len;
    } else if (memcmp(type, "IEND", 4) == 0) {
      free(data);
      break;
    }
    free(data);
  }
  fclose(f);

  if (w <= 0 || h <= 0 || !idat) {
    fprintf(stderr, "%s: could not read PNG header\n", filename);
    free(idat);
    return -1;
  }
  if (bitdepth != 8 || (colortype != 0 && colortype != 2 && colortype != 6)) {
    fprintf(stderr,
            "%s: unsupported PNG format (need 8-bit grayscale/RGB/RGBA, "
            "e.g. a file this program wrote)\n",
            filename);
    free(idat);
    return -1;
  }
  int channels = (colortype == 0) ? 1 : (colortype == 2) ? 3 : 4;

  size_t rawexpected = (size_t)h * (1 + (size_t)w * channels);
  size_t rawlen = 0;
  unsigned char *raw = inflate_stored(idat, idatlen, rawexpected, &rawlen);
  free(idat);
  if (!raw) {
    fprintf(stderr,
            "%s: this PNG uses real DEFLATE compression (e.g. it was "
            "resaved by another program). This tool can only read images "
            "it wrote itself.\n",
            filename);
    return -1;
  }

  unsigned char *pixels = malloc((size_t)w * h);
  size_t need = (size_t)h * (1 + (size_t)w * channels);
  if (rawlen < need) {
    fprintf(stderr, "%s: truncated pixel data\n", filename);
    free(raw);
    free(pixels);
    return -1;
  }
  for (int y = 0; y < h; y++) {
    unsigned char *row = raw + (size_t)y * (1 + (size_t)w * channels);
    unsigned char filt = row[0];
    if (filt != 0) {
      fprintf(stderr,
              "%s: unsupported row filter (only 'None' filter rows are "
              "supported)\n",
              filename);
      free(raw);
      free(pixels);
      return -1;
    }
    unsigned char *px = row + 1;
    for (int x = 0; x < w; x++) {
      if (channels == 1) {
        pixels[(size_t)y * w + x] = px[x];
      } else {
        const unsigned char *s = px + (size_t)x * channels;
        int gray = (s[0] + s[1] + s[2]) / 3;
        pixels[(size_t)y * w + x] = (unsigned char)gray;
      }
    }
  }
  free(raw);
  *out_w = w;
  *out_h = h;
  *out_pixels = pixels;
  return 0;
}

/* ============================================================
 * Punch card rendering (SVG for print, PNG for a real image file)
 * ============================================================ */

static const double CARD_W = 737.5, CARD_H = 325.0;
static const double CARD_LEFT = 28, CARD_TOP = 22;
static const double CARD_COLPITCH = 8.7, CARD_ROWPITCH = 25.0;
static const double CARD_HOLEW = 4.4, CARD_HOLEH = 12.0;

static void svg_card(FILE *out, const int *masks, int n) {
  fprintf(out,
          "<div class=\"card\"><svg viewBox=\"0 0 %.1f %.1f\" "
          "xmlns=\"http://www.w3.org/2000/svg\">\n",
          CARD_W, CARD_H);
  fprintf(out,
          "<rect x=\"0\" y=\"0\" width=\"%.1f\" height=\"%.1f\" "
          "class=\"cardstock\"/>\n",
          CARD_W, CARD_H);
  fprintf(out, "<polygon points=\"0,0 22,0 0,22\" class=\"cardbg\"/>\n");

  for (int col = 0; col < COLS_PER_CARD; col++) {
    int mask = (col < n) ? masks[col] : 0;
    double cx = CARD_LEFT + col * CARD_COLPITCH;
    for (int r = 0; r < 12; r++) {
      if (mask & (1 << r)) {
        double cy = CARD_TOP + r * CARD_ROWPITCH;
        fprintf(out,
                "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
                "class=\"hole\"/>\n",
                cx - CARD_HOLEW / 2, cy - CARD_HOLEH / 2, CARD_HOLEW,
                CARD_HOLEH);
      }
    }
  }
  fprintf(out, "</svg></div>\n");
}

#define CARD_PXSCALE 4
#define CARD_BG_GRAY 223
#define CARD_HOLE_GRAY 58
#define CARD_THRESHOLD 140

/* Renders one card (up to 80 columns) to an actual PNG file. */
static int png_card(const char *filename, const int *masks, int n) {
  int w = (int)(CARD_W * CARD_PXSCALE);
  int h = (int)(CARD_H * CARD_PXSCALE);
  unsigned char *px = malloc((size_t)w * h);
  memset(px, CARD_BG_GRAY, (size_t)w * h);

  for (int col = 0; col < COLS_PER_CARD; col++) {
    int mask = (col < n) ? masks[col] : 0;
    if (!mask)
      continue;
    double cx = (CARD_LEFT + col * CARD_COLPITCH) * CARD_PXSCALE;
    for (int r = 0; r < 12; r++) {
      if (!(mask & (1 << r)))
        continue;
      double cy = (CARD_TOP + r * CARD_ROWPITCH) * CARD_PXSCALE;
      int x0 = (int)(cx - CARD_HOLEW * CARD_PXSCALE / 2);
      int x1 = (int)(cx + CARD_HOLEW * CARD_PXSCALE / 2);
      int y0 = (int)(cy - CARD_HOLEH * CARD_PXSCALE / 2);
      int y1 = (int)(cy + CARD_HOLEH * CARD_PXSCALE / 2);
      for (int y = y0; y < y1; y++) {
        if (y < 0 || y >= h)
          continue;
        for (int x = x0; x < x1; x++) {
          if (x < 0 || x >= w)
            continue;
          px[(size_t)y * w + x] = CARD_HOLE_GRAY;
        }
      }
    }
  }
  int rc = write_png_gray(filename, w, h, px);
  free(px);
  return rc;
}

/* Reads a card PNG back into up to 80 column masks. Returns columns read. */
static int decode_card_png(const char *filename, int *masks_out) {
  int w, h;
  unsigned char *px;
  if (read_png_gray(filename, &w, &h, &px) != 0)
    return -1;

  int expect_w = (int)(CARD_W * CARD_PXSCALE);
  int expect_h = (int)(CARD_H * CARD_PXSCALE);
  if (w != expect_w || h != expect_h) {
    fprintf(stderr,
            "%s: image is %dx%d, expected %dx%d (a punch card image made "
            "by this program) - can't reliably locate the hole grid\n",
            filename, w, h, expect_w, expect_h);
    free(px);
    return -1;
  }

  for (int col = 0; col < COLS_PER_CARD; col++) {
    int mask = 0;
    double cx = (CARD_LEFT + col * CARD_COLPITCH) * CARD_PXSCALE;
    for (int r = 0; r < 12; r++) {
      double cy = (CARD_TOP + r * CARD_ROWPITCH) * CARD_PXSCALE;
      int x = (int)cx, y = (int)cy;
      if (x < 0 || x >= w || y < 0 || y >= h)
        continue;
      if (px[(size_t)y * w + x] < CARD_THRESHOLD)
        mask |= (1 << r);
    }
    masks_out[col] = mask;
  }
  free(px);
  return COLS_PER_CARD;
}

/* ============================================================
 * Morse tape rendering (SVG for print, PNG for a real image file)
 * ============================================================ */

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

/* --- Slot-grid model used for the PNG tape image, so it can be
 * decoded back deterministically. One slot per dot/dash, plus
 * explicit blank slots for letter/word gaps:
 *   1 blank slot  -> letter separator (no space in output)
 *   2 blank slots -> word separator (one space in output)
 */
typedef enum { SLOT_BLANK, SLOT_DOT, SLOT_DASH } SlotKind;

static int build_tape_slots(const char *message, SlotKind **out_slots) {
  size_t cap = strlen(message) * 6 + 8;
  SlotKind *slots = malloc(cap * sizeof(SlotKind));
  size_t n = 0;
  int first_word = 1;

  const char *p = message;
  while (*p) {
    while (*p == ' ')
      p++;
    if (!*p)
      break;
    if (!first_word) {
      slots[n++] = SLOT_BLANK;
      slots[n++] = SLOT_BLANK;
    }
    first_word = 0;
    int first_letter = 1;
    while (*p && *p != ' ') {
      const char *code = char_to_morse(*p);
      if (code) {
        if (!first_letter)
          slots[n++] = SLOT_BLANK;
        first_letter = 0;
        for (const char *m = code; *m; m++)
          slots[n++] = (*m == '.') ? SLOT_DOT : SLOT_DASH;
      }
      p++;
    }
  }
  *out_slots = slots;
  return (int)n;
}

#define TAPE_PXSCALE 4
#define TAPE_SLOTS_PER_ROW 24
#define TAPE_SLOTW 20.0
#define TAPE_ROWH 40.0
#define TAPE_MARGIN 20.0
#define TAPE_BG_GRAY 237
#define TAPE_MARK_GRAY 42
#define TAPE_THRESHOLD 140
#define TAPE_DOT_R 5.0
#define TAPE_DASH_HALFW 8.5

static int png_tape(const char *filename, const char *message) {
  SlotKind *slots;
  int nslots = build_tape_slots(message, &slots);

  int rows = nslots / TAPE_SLOTS_PER_ROW + 1;
  int w =
      (int)((TAPE_MARGIN * 2 + TAPE_SLOTS_PER_ROW * TAPE_SLOTW) * TAPE_PXSCALE);
  int h = (int)((TAPE_MARGIN * 2 + rows * TAPE_ROWH) * TAPE_PXSCALE);

  unsigned char *px = malloc((size_t)w * h);
  memset(px, TAPE_BG_GRAY, (size_t)w * h);

  for (int i = 0; i < nslots; i++) {
    if (slots[i] == SLOT_BLANK)
      continue;
    int row = i / TAPE_SLOTS_PER_ROW;
    int col = i % TAPE_SLOTS_PER_ROW;
    double cx = (TAPE_MARGIN + (col + 0.5) * TAPE_SLOTW) * TAPE_PXSCALE;
    double cy = (TAPE_MARGIN + (row + 0.5) * TAPE_ROWH) * TAPE_PXSCALE;
    double halfw =
        (slots[i] == SLOT_DOT ? TAPE_DOT_R : TAPE_DASH_HALFW) * TAPE_PXSCALE;
    double halfh = TAPE_DOT_R * TAPE_PXSCALE;
    int x0 = (int)(cx - halfw), x1 = (int)(cx + halfw);
    int y0 = (int)(cy - halfh), y1 = (int)(cy + halfh);
    for (int y = y0; y < y1; y++) {
      if (y < 0 || y >= h)
        continue;
      for (int x = x0; x < x1; x++) {
        if (x < 0 || x >= w)
          continue;
        px[(size_t)y * w + x] = TAPE_MARK_GRAY;
      }
    }
  }
  free(slots);
  int rc = write_png_gray(filename, w, h, px);
  free(px);
  return rc;
}

static char *decode_tape_png(const char *filename) {
  int w, h;
  unsigned char *px;
  if (read_png_gray(filename, &w, &h, &px) != 0)
    return NULL;

  int rows = (int)((h / TAPE_PXSCALE - TAPE_MARGIN * 2) / TAPE_ROWH + 0.5);
  if (rows < 1)
    rows = 1;
  int nslots = rows * TAPE_SLOTS_PER_ROW;

  SlotKind *slots = malloc(nslots * sizeof(SlotKind));
  for (int i = 0; i < nslots; i++) {
    int row = i / TAPE_SLOTS_PER_ROW;
    int col = i % TAPE_SLOTS_PER_ROW;
    double cx = (TAPE_MARGIN + (col + 0.5) * TAPE_SLOTW) * TAPE_PXSCALE;
    double cy = (TAPE_MARGIN + (row + 0.5) * TAPE_ROWH) * TAPE_PXSCALE;
    int cxi = (int)cx, cyi = (int)cy;
    int center_dark = 0, edge_dark = 0;
    if (cxi >= 0 && cxi < w && cyi >= 0 && cyi < h)
      center_dark = px[(size_t)cyi * w + cxi] < TAPE_THRESHOLD;
    int ex = (int)(cx + (TAPE_DASH_HALFW - 2) * TAPE_PXSCALE);
    if (ex >= 0 && ex < w && cyi >= 0 && cyi < h)
      edge_dark = px[(size_t)cyi * w + ex] < TAPE_THRESHOLD;
    if (!center_dark)
      slots[i] = SLOT_BLANK;
    else
      slots[i] = edge_dark ? SLOT_DASH : SLOT_DOT;
  }
  free(px);

  /* trim trailing blanks */
  while (nslots > 0 && slots[nslots - 1] == SLOT_BLANK)
    nslots--;

  char *out = malloc(nslots + 8);
  size_t oi = 0;
  char code[16];
  int codelen = 0;
  int i = 0;
  while (i < nslots) {
    if (slots[i] == SLOT_DOT || slots[i] == SLOT_DASH) {
      if (codelen < 15)
        code[codelen++] = (slots[i] == SLOT_DOT) ? '.' : '-';
      i++;
      continue;
    }
    int j = i;
    while (j < nslots && slots[j] == SLOT_BLANK)
      j++;
    int runlen = j - i;
    if (codelen > 0) {
      code[codelen] = 0;
      out[oi++] = morse_to_char(code);
      codelen = 0;
    }
    if (runlen >= 2)
      out[oi++] = ' ';
    i = j;
  }
  if (codelen > 0) {
    code[codelen] = 0;
    out[oi++] = morse_to_char(code);
  }
  out[oi] = 0;
  free(slots);
  return out;
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
  int cardnum = 0;
  int ncards = n / COLS_PER_CARD + 1;
  while (pos < n) {
    int chunk = (n - pos > COLS_PER_CARD) ? COLS_PER_CARD : (n - pos);
    int masks[COLS_PER_CARD];
    memset(masks, 0, sizeof(masks));
    for (int i = 0; i < chunk; i++) {
      masks[i] = char_to_mask(message[pos + i]);
      fprintf(holes, "%d:%d\n", pos + i, masks[i]);
    }
    svg_card(html, masks, chunk);

    char pngname[64];
    if (ncards > 1)
      snprintf(pngname, sizeof(pngname), "card_%d.png", cardnum + 1);
    else
      snprintf(pngname, sizeof(pngname), "card.png");
    png_card(pngname, masks, chunk);
    printf("Wrote %s\n", pngname);

    pos += chunk;
    cardnum++;
  }
  write_html_footer(html);
  fclose(html);
  fclose(holes);
  printf("Wrote card.html (print this) and card.holes (the raw hole data -- "
         "the 'key').\n");
}

static void card_decode_holes(const char *holefile) {
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

static void card_decode_images(char **files, int nfiles) {
  char out[8192];
  size_t oi = 0;
  for (int i = 0; i < nfiles; i++) {
    int masks[COLS_PER_CARD];
    int got = decode_card_png(files[i], masks);
    if (got < 0)
      exit(1);
    for (int c = 0; c < got && oi < sizeof(out) - 1; c++)
      out[oi++] = mask_to_char(masks[c]);
  }
  while (oi > 0 && out[oi - 1] == ' ')
    oi--;
  out[oi] = 0;
  printf("Decoded message: %s\n", out);
}

static int has_suffix(const char *s, const char *suf) {
  size_t ls = strlen(s), lf = strlen(suf);
  if (lf > ls)
    return 0;
  for (size_t i = 0; i < lf; i++)
    if (tolower((unsigned char)s[ls - lf + i]) != suf[i])
      return 0;
  return 1;
}

static void card_decode(char **files, int nfiles) {
  if (nfiles == 1 && !has_suffix(files[0], ".png")) {
    card_decode_holes(files[0]);
    return;
  }
  for (int i = 0; i < nfiles; i++) {
    if (!has_suffix(files[i], ".png")) {
      fprintf(stderr, "%s: mix of .png and non-.png files isn't supported\n",
              files[i]);
      exit(1);
    }
  }
  card_decode_images(files, nfiles);
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

  png_tape("tape.png", message);
  printf("Wrote tape.png\n");
  printf("Wrote tape.html (print this) and tape.morse (the raw code -- the "
         "'key').\n");
}

static void tape_decode_morse(const char *morsefile) {
  FILE *f = fopen(morsefile, "r");
  if (!f) {
    fprintf(stderr, "Cannot open %s\n", morsefile);
    exit(1);
  }
  char tok[256];
  printf("Decoded tape message: ");
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

static void tape_decode(const char *file) {
  if (has_suffix(file, ".png")) {
    char *msg = decode_tape_png(file);
    if (!msg)
      exit(1);
    printf("Decoded tape message: %s\n", msg);
    free(msg);
  } else {
    tape_decode_morse(file);
  }
}

int main(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr, "IM-0 CLI by Phantekzy\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s card encode \"Your message here\"\n", argv[0]);
    fprintf(stderr, "      -> writes card.html, card.holes, and card.png (or "
                    "card_1.png, card_2.png, ... for longer messages)\n");
    fprintf(stderr, "  %s card decode card.holes\n", argv[0]);
    fprintf(stderr, "  %s card decode card.png [card_2.png ...]\n", argv[0]);
    fprintf(stderr, "  %s tape encode \"Your message here\"\n", argv[0]);
    fprintf(stderr, "      -> writes tape.html, tape.morse, and tape.png\n");
    fprintf(stderr, "  %s tape decode tape.morse\n", argv[0]);
    fprintf(stderr, "  %s tape decode tape.png\n", argv[0]);
    return 1;
  }

  const char *mode = argv[1];
  const char *action = argv[2];

  if (strcmp(mode, "card") == 0) {
    if (strcmp(action, "encode") == 0) {
      card_encode(argv[3]);
    } else if (strcmp(action, "decode") == 0) {
      card_decode(argv + 3, argc - 3);
    } else {
      fprintf(stderr, "Unknown card action: %s\n", action);
      return 1;
    }
  } else if (strcmp(mode, "tape") == 0) {
    if (strcmp(action, "encode") == 0) {
      tape_encode(argv[3]);
    } else if (strcmp(action, "decode") == 0) {
      tape_decode(argv[3]);
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
