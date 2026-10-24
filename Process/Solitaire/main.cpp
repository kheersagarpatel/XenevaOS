/**
 * Solitaire for XenevaOS
 * Developed by Knowivate / Kheersagar Patel
 * Copyright (c) 2026 Knowivate. All rights reserved.
 */

#include <stdint.h>
#include <_xeneva.h>
#include <stdio.h>
#include <sys/_keproc.h>
#include <sys/_kefile.h>
#include <sys/iocodes.h>
#include <sys/_ketime.h>
#include <stdlib.h>
#include <string.h>
#include <chitralekha.h>
#include <draw.h>
#include <font.h>
#include <widgets/window.h>
#include <widgets/base.h>

#include "solitaire.h"

#define WINDOW_WIDTH  720
#define WINDOW_HEIGHT 520

#define COLOR_TABLE_GREEN      0xFF0B6623
#define COLOR_TOPBAR_BG        0xFF072410
#define COLOR_TOPBAR_BORDER    0xFF0E7A2C
#define COLOR_SLOT_OUTLINE     0xFF147A33
#define COLOR_CARD_WHITE       0xFFFCFCFC
#define COLOR_CARD_BORDER      0xFF666666
#define COLOR_CARD_SELECT_GLOW 0xFFFFD700
#define COLOR_RED_SUIT         0xFFD32F2F
#define COLOR_BLACK_SUIT       0xFF1A1A1A
#define COLOR_CARD_BACK_BASE   0xFF0A1830
#define COLOR_CARD_BACK_GOLD   0xFFD4AF37

// Layout coordinates for 720px width
#define LAYOUT_STOCK_X       40
#define LAYOUT_WASTE_X       134
#define LAYOUT_FOUND_BASE_X  322
#define LAYOUT_FOUND_STEP    94
#define LAYOUT_TOP_Y         78
#define LAYOUT_TABLEAU_BASE_X 40
#define LAYOUT_TABLEAU_STEP  94
#define LAYOUT_TABLEAU_BASE_Y 200

#define MAX_CARD_ANIMATIONS  8
#define CARD_ANIM_FRAMES     10

typedef struct _card_anim_ {
    bool active;
    Card cards[13];
    int count;
    float startX, startY;
    float targetX, targetY;
    int currentFrame;
    int totalFrames;
    int dstPileType;
    int dstPileIndex;
} CardAnimation;

ChitralekhaApp* app = NULL;
ChWindow* mainWin = NULL;

static int frameCounter = 0;
static uint32_t lastClickFrame = 0;
static int lastClickX = -1;
static int lastClickY = -1;
static CardAnimation activeAnims[MAX_CARD_ANIMATIONS];

static void GetPileCardPos(int pileType, int pileIndex, int cardIndex, int* outX, int* outY) {
    if (pileType == PILE_STOCK) {
        *outX = LAYOUT_STOCK_X;
        *outY = LAYOUT_TOP_Y;
    } else if (pileType == PILE_WASTE) {
        *outX = LAYOUT_WASTE_X;
        *outY = LAYOUT_TOP_Y;
    } else if (pileType == PILE_FOUNDATION) {
        *outX = LAYOUT_FOUND_BASE_X + pileIndex * LAYOUT_FOUND_STEP;
        *outY = LAYOUT_TOP_Y;
    } else if (pileType == PILE_TABLEAU) {
        *outX = LAYOUT_TABLEAU_BASE_X + pileIndex * LAYOUT_TABLEAU_STEP;
        int curY = LAYOUT_TABLEAU_BASE_Y;
        if (pileIndex >= 0 && pileIndex < TABLEAU_COUNT) {
            CardStack* pile = &game.tableau[pileIndex];
            for (int r = 0; r < cardIndex && r < pile->count; r++) {
                curY += pile->cards[r].faceUp ? 24 : 14;
            }
        }
        *outY = curY;
    } else {
        *outX = 0;
        *outY = 0;
    }
}

static void StartCardAnimation(const Card* cards, int count, int srcX, int srcY, int dstX, int dstY, int dstPileType, int dstPileIndex) {
    int slot = -1;
    for (int i = 0; i < MAX_CARD_ANIMATIONS; i++) {
        if (!activeAnims[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) slot = 0;

    CardAnimation* anim = &activeAnims[slot];
    anim->active = true;
    anim->count = count > 13 ? 13 : count;
    for (int i = 0; i < anim->count; i++) {
        anim->cards[i] = cards[i];
    }
    anim->startX = (float)srcX;
    anim->startY = (float)srcY;
    anim->targetX = (float)dstX;
    anim->targetY = (float)dstY;
    anim->currentFrame = 0;
    anim->totalFrames = CARD_ANIM_FRAMES;
    anim->dstPileType = dstPileType;
    anim->dstPileIndex = dstPileIndex;
}

static void UpdateCardAnimations() {
    for (int i = 0; i < MAX_CARD_ANIMATIONS; i++) {
        if (activeAnims[i].active) {
            activeAnims[i].currentFrame++;
            if (activeAnims[i].currentFrame >= activeAnims[i].totalFrames) {
                activeAnims[i].active = false;
            }
        }
    }
}

static int GetAnimatingCountForPile(int pileType, int pileIndex) {
    int count = 0;
    for (int i = 0; i < MAX_CARD_ANIMATIONS; i++) {
        if (activeAnims[i].active &&
            activeAnims[i].dstPileType == pileType &&
            activeAnims[i].dstPileIndex == pileIndex) {
            count += activeAnims[i].count;
        }
    }
    return count;
}

// Procedural precision vector suit rendering
static void DrawSuitGlyph(ChCanvas* canv, int suit, int cx, int cy, int size, uint32_t color) {
    if (suit == CARD_SUIT_HEARTS) {
        // Anatomical Heart: two rounded top lobes + smooth taper to sharp bottom point
        int r = (size * 7) / 20;
        int lobeY = cy - size / 4;
        ChDrawFilledCircle(canv, cx - r, lobeY, r, color);
        ChDrawFilledCircle(canv, cx + r, lobeY, r, color);

        int bottomY = cy + size;
        for (int y = lobeY; y <= bottomY; y++) {
            int progress = bottomY - y;
            int span = (progress * (size * 2)) / (bottomY - lobeY + 1);
            if (span > 0) {
                ChDrawHorizontalLine(canv, cx - span / 2, y, span, color);
            }
        }
    } else if (suit == CARD_SUIT_DIAMONDS) {
        // High-precision 4-point Diamond Rhombus
        int halfH = size;
        int halfW = (size * 72) / 100;
        for (int dy = -halfH; dy <= halfH; dy++) {
            int dist = dy < 0 ? -dy : dy;
            int w = halfW * (halfH - dist) / halfH;
            if (w > 0) {
                ChDrawHorizontalLine(canv, cx - w, cy + dy, w * 2 + 1, color);
            }
        }
    } else if (suit == CARD_SUIT_CLUBS) {
        // Precision 3-leaf Clover with tapered pedestal stem
        int r = (size * 7) / 20;
        // Top leaf
        ChDrawFilledCircle(canv, cx, cy - (r * 11) / 10, r, color);
        // Bottom-left leaf
        ChDrawFilledCircle(canv, cx - r + 1, cy + r / 4, r, color);
        // Bottom-right leaf
        ChDrawFilledCircle(canv, cx + r - 1, cy + r / 4, r, color);
        // Central disc bridging leaves
        ChDrawFilledCircle(canv, cx, cy, (r * 3) / 4, color);

        // Flared pedestal stem
        int stemH = (size * 6) / 10;
        int stemTop = cy + r / 3;
        for (int dy = 0; dy <= stemH; dy++) {
            int sw = 1 + (dy * dy * 4) / (stemH * stemH + 1);
            ChDrawHorizontalLine(canv, cx - sw, stemTop + dy, sw * 2 + 1, color);
        }
        // Base bar
        ChDrawHorizontalLine(canv, cx - r, stemTop + stemH, r * 2 + 1, color);
    } else if (suit == CARD_SUIT_SPADES) {
        // Precision Gothic Spade: sharp needle tip, curved outer body, dual base lobes, flared stem
        int r = (size * 7) / 20;
        int tipY = cy - size;
        int waistY = cy + r / 3;

        // Needle top tapering smoothly
        for (int y = tipY; y <= waistY; y++) {
            int progress = y - tipY;
            int span = (progress * (size * 2)) / (waistY - tipY + 1);
            if (span > 0) {
                ChDrawHorizontalLine(canv, cx - span / 2, y, span, color);
            }
        }
        // Dual bottom lobes
        ChDrawFilledCircle(canv, cx - r + 1, waistY, r, color);
        ChDrawFilledCircle(canv, cx + r - 1, waistY, r, color);
        ChDrawFilledCircle(canv, cx, waistY - r / 4, r, color);

        // Flared pedestal stem
        int stemH = (size * 6) / 10;
        int stemTop = waistY + r / 2;
        for (int dy = 0; dy <= stemH; dy++) {
            int sw = 1 + (dy * dy * 4) / (stemH * stemH + 1);
            ChDrawHorizontalLine(canv, cx - sw, stemTop + dy, sw * 2 + 1, color);
        }
        // Base bar
        ChDrawHorizontalLine(canv, cx - r, stemTop + stemH, r * 2 + 1, color);
    }
}

static void DrawRoundedRect(ChCanvas* canv, int x, int y, int w, int h, int r, uint32_t color) {
    if (r <= 0) {
        ChDrawRect(canv, x, y, w, h, color);
        return;
    }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    // Central rectangular blocks
    ChDrawRect(canv, x, y + r, w, h - 2 * r, color);
    ChDrawRect(canv, x + r, y, w - 2 * r, r, color);
    ChDrawRect(canv, x + r, y + h - r, w - 2 * r, r, color);
    // 4 corner filled circles
    ChDrawFilledCircle(canv, x + r, y + r, r, color);
    ChDrawFilledCircle(canv, x + w - r - 1, y + r, r, color);
    ChDrawFilledCircle(canv, x + r, y + h - r - 1, r, color);
    ChDrawFilledCircle(canv, x + w - r - 1, y + h - r - 1, r, color);
}

// Royal Court center art for King, Queen, Jack, and Ace
static void DrawCourtFigure(ChCanvas* canv, Card card, int cx, int cy, uint32_t suitColor) {
    if (card.rank == CARD_RANK_KING) {
        // Regal Crown (Gold & Suit Color)
        int crownW = 32;
        int topY = cy - 18;

        // Crown Band
        DrawRoundedRect(canv, cx - crownW / 2, cy - 2, crownW, 7, 2, 0xFFD4AF37);
        ChDrawRect(canv, cx - 12, cy - 1, 4, 3, 0xFFE74C3C);
        ChDrawRect(canv, cx - 2, cy - 1, 4, 3, 0xFF3498DB);
        ChDrawRect(canv, cx + 8, cy - 1, 4, 3, 0xFFE74C3C);

        // 5 Crown Spires
        ChDrawFilledCircle(canv, cx - 13, topY + 4, 2, 0xFFD4AF37);
        ChDrawFilledCircle(canv, cx - 7, topY + 7, 2, 0xFFD4AF37);
        ChDrawFilledCircle(canv, cx, topY, 3, 0xFFF1C40F);
        ChDrawFilledCircle(canv, cx + 7, topY + 7, 2, 0xFFD4AF37);
        ChDrawFilledCircle(canv, cx + 13, topY + 4, 2, 0xFFD4AF37);

        // Crown fill body
        for (int dy = 0; dy < 14; dy++) {
            int w = crownW - dy;
            ChDrawHorizontalLine(canv, cx - w / 2, cy - 16 + dy, w, 0xFFD4AF37);
        }

        // Central royal suit emblem
        DrawSuitGlyph(canv, card.suit, cx, cy + 16, 10, suitColor);

    } else if (card.rank == CARD_RANK_QUEEN) {
        // Elegant Jeweled Tiara
        int topY = cy - 16;
        DrawRoundedRect(canv, cx - 14, cy - 2, 28, 6, 2, 0xFFD4AF37);

        // 3 Arched Spire Pearls
        ChDrawFilledCircle(canv, cx - 10, topY + 4, 2, 0xFFFFFFFF);
        ChDrawFilledCircle(canv, cx, topY, 3, 0xFFF1C40F);
        ChDrawFilledCircle(canv, cx + 10, topY + 4, 2, 0xFFFFFFFF);

        for (int dy = 0; dy < 12; dy++) {
            int w = 26 - dy * 2;
            if (w > 0) {
                ChDrawHorizontalLine(canv, cx - w / 2, cy - 14 + dy, w, 0xFFE5C158);
            }
        }

        // Center suit emblem
        DrawSuitGlyph(canv, card.suit, cx, cy + 15, 10, suitColor);

    } else if (card.rank == CARD_RANK_JACK) {
        // Chivalric Knight's Heraldic Shield
        int shW = 26, shH = 26;
        int shX = cx - shW / 2;
        int shY = cy - 16;

        DrawRoundedRect(canv, shX, shY, shW, shH, 4, 0xFF2C3E50);
        ChDrawRectUnfilled(canv, shX, shY, shW, shH, 0xFFD4AF37);
        ChDrawRectUnfilled(canv, shX + 2, shY + 2, shW - 4, shH - 4, 0xFFF1C40F);

        // Shield cross division
        ChDrawVerticalLine(canv, cx, shY + 2, shH - 4, 0xFFD4AF37);
        ChDrawHorizontalLine(canv, shX + 2, cy - 3, shW - 4, 0xFFD4AF37);

        // Center suit emblem
        DrawSuitGlyph(canv, card.suit, cx, cy + 18, 10, suitColor);

    } else if (card.rank == CARD_RANK_ACE) {
        // Majestic Ace Centerpiece with Gold Filigree Surround
        ChDrawCircleUnfilled(canv, cx, cy, 22, 0x50D4AF37);
        ChDrawCircleUnfilled(canv, cx, cy, 24, 0x30D4AF37);
        DrawSuitGlyph(canv, card.suit, cx, cy, 18, suitColor);

    } else {
        // Numeric Card (2..10): Large, crisp center suit
        DrawSuitGlyph(canv, card.suit, cx, cy, 14, suitColor);
    }
}

static void DrawCardBack(ChCanvas* canv, int x, int y, int w, int h) {
    // Royal Midnight Navy base
    DrawRoundedRect(canv, x, y, w, h, 6, COLOR_CARD_BACK_BASE);
    ChDrawRectUnfilled(canv, x, y, w, h, COLOR_CARD_BORDER);

    // Double Gold Border
    ChDrawRectUnfilled(canv, x + 4, y + 4, w - 8, h - 8, COLOR_CARD_BACK_GOLD);
    ChDrawRectUnfilled(canv, x + 6, y + 6, w - 12, h - 12, 0x80D4AF37);

    // Subtle Geometric Diamond Crosshatch Weave
    for (int offset = 14; offset < h - 14; offset += 16) {
        ChDrawHorizontalLine(canv, x + 8, y + offset, w - 16, 0x18D4AF37);
    }

    // Elegant Central Knowivate Medallion
    int cx = x + w / 2;
    int cy = y + h / 2;

    // Radiating starburst ring
    ChDrawCircleUnfilled(canv, cx, cy, 16, 0x60D4AF37);
    ChDrawCircleUnfilled(canv, cx, cy, 14, COLOR_CARD_BACK_GOLD);
    ChDrawFilledCircle(canv, cx, cy, 12, 0xFF0E2244);

    // Bold "K" Emblem (vertically & horizontally centered)
    int kw = (int)ChFontGetWidth(app->baseFont, (char*)"K");
    ChFontDrawText(canv, app->baseFont, (char*)"K", cx - kw / 2, cy + 4, 12, COLOR_CARD_BACK_GOLD);
}

static void DrawCard(ChCanvas* canv, Card card, int x, int y, bool isSelected) {
    if (!card.faceUp) {
        DrawCardBack(canv, x, y, CARD_WIDTH, CARD_HEIGHT);
        return;
    }

    // Card face
    DrawRoundedRect(canv, x, y, CARD_WIDTH, CARD_HEIGHT, 6, COLOR_CARD_WHITE);

    if (isSelected) {
        ChDrawRectUnfilled(canv, x - 1, y - 1, CARD_WIDTH + 2, CARD_HEIGHT + 2, COLOR_CARD_SELECT_GLOW);
        ChDrawRectUnfilled(canv, x, y, CARD_WIDTH, CARD_HEIGHT, COLOR_CARD_SELECT_GLOW);
    } else {
        ChDrawRectUnfilled(canv, x, y, CARD_WIDTH, CARD_HEIGHT, COLOR_CARD_BORDER);
    }

    uint32_t suitColor = CardIsRed(card.suit) ? COLOR_RED_SUIT : COLOR_BLACK_SUIT;
    const char* rankStr = CardRankStr(card.rank);

    // Top-left rank & mini suit (baseline y+17 places glyph neatly inside card)
    ChFontDrawText(canv, app->baseFont, (char*)rankStr, x + 7, y + 17, 12, suitColor);
    DrawSuitGlyph(canv, card.suit, x + 12, y + 27, 6, suitColor);

    // Center Suit / Face Card Icon
    DrawCourtFigure(canv, card, x + CARD_WIDTH / 2, y + CARD_HEIGHT / 2, suitColor);

    // Bottom-right inverted rank & mini suit
    int rw = (int)ChFontGetWidth(app->baseFont, (char*)rankStr);
    ChFontDrawText(canv, app->baseFont, (char*)rankStr, x + CARD_WIDTH - rw - 6, y + CARD_HEIGHT - 8, 12, suitColor);
    DrawSuitGlyph(canv, card.suit, x + CARD_WIDTH - 12, y + CARD_HEIGHT - 22, 6, suitColor);
}

static void DrawSlotOutline(ChCanvas* canv, int x, int y, int hintSuit) {
    DrawRoundedRect(canv, x, y, CARD_WIDTH, CARD_HEIGHT, 6, COLOR_SLOT_OUTLINE);
    ChDrawRectUnfilled(canv, x, y, CARD_WIDTH, CARD_HEIGHT, COLOR_TOPBAR_BORDER);

    if (hintSuit >= 0 && hintSuit < 4) {
        // Draw subtle faint suit hint watermark
        DrawSuitGlyph(canv, hintSuit, x + CARD_WIDTH / 2, y + CARD_HEIGHT / 2, 14, COLOR_TOPBAR_BORDER);
    }
}

static void RenderTopBar(ChCanvas* canv) {
    // Deep emerald dashboard banner
    ChDrawRect(canv, 0, 26, WINDOW_WIDTH, 42, COLOR_TOPBAR_BG);
    ChDrawHorizontalLine(canv, 0, 68, WINDOW_WIDTH, 0xFF165227);

    // Vertical baseline formula for height=26 button at by=34:
    // by + bh/2 + 4 = 34 + 13 + 4 = 51
    int textBaselineY = 51;

    // 1. "New Game" button: x=15, y=34, w=85, h=26
    DrawRoundedRect(canv, 15, 34, 85, 26, 5, 0xFF114F23);
    ChDrawRectUnfilled(canv, 15, 34, 85, 26, 0xFF2ECC71);
    int twNew = (int)ChFontGetWidth(app->baseFont, (char*)"New Game");
    ChFontDrawText(canv, app->baseFont, (char*)"New Game", 15 + (85 - twNew) / 2, textBaselineY, 11, 0xFFFFFFFF);

    // 2. "Restart" button: x=108, y=34, w=75, h=26
    DrawRoundedRect(canv, 108, 34, 75, 26, 5, 0xFF114F23);
    ChDrawRectUnfilled(canv, 108, 34, 75, 26, 0xFF2ECC71);
    int twRes = (int)ChFontGetWidth(app->baseFont, (char*)"Restart");
    ChFontDrawText(canv, app->baseFont, (char*)"Restart", 108 + (75 - twRes) / 2, textBaselineY, 11, 0xFFFFFFFF);

    // 3. "Undo" button: x=191, y=34, w=65, h=26
    bool canUndo = SolitaireCanUndo();
    uint32_t undoFill = canUndo ? 0xFF165E2B : 0xFF0A2B14;
    uint32_t undoBorder = canUndo ? 0xFFF1C40F : 0xFF164423;
    uint32_t undoText = canUndo ? 0xFFFFFFFF : 0xFF50785D;
    DrawRoundedRect(canv, 191, 34, 65, 26, 5, undoFill);
    ChDrawRectUnfilled(canv, 191, 34, 65, 26, undoBorder);
    int twUndo = (int)ChFontGetWidth(app->baseFont, (char*)"Undo");
    ChFontDrawText(canv, app->baseFont, (char*)"Undo", 191 + (65 - twUndo) / 2, textBaselineY, 11, undoText);

    // 4. Metric Capsules (Dark pill capsules)
    // Moves: x=264, w=78
    DrawRoundedRect(canv, 264, 34, 78, 26, 4, 0xFF051B0C);
    ChDrawRectUnfilled(canv, 264, 34, 78, 26, 0xFF164423);
    char buf[64];
    sprintf(buf, "Moves: %d", game.moveCount);
    int twMoves = (int)ChFontGetWidth(app->baseFont, buf);
    ChFontDrawText(canv, app->baseFont, buf, 264 + (78 - twMoves) / 2, textBaselineY, 11, 0xFFE0E0E0);

    // Score: x=348, w=82
    DrawRoundedRect(canv, 348, 34, 82, 26, 4, 0xFF051B0C);
    ChDrawRectUnfilled(canv, 348, 34, 82, 26, 0xFF164423);
    sprintf(buf, "Score: %d", game.score);
    int twScore = (int)ChFontGetWidth(app->baseFont, buf);
    ChFontDrawText(canv, app->baseFont, buf, 348 + (82 - twScore) / 2, textBaselineY, 11, 0xFFE0E0E0);

    // Time: x=436, w=72
    DrawRoundedRect(canv, 436, 34, 72, 26, 4, 0xFF051B0C);
    ChDrawRectUnfilled(canv, 436, 34, 72, 26, 0xFF164423);
    int m = game.timeElapsedSec / 60;
    int s = game.timeElapsedSec % 60;
    sprintf(buf, "%02d:%02d", m, s);
    int twTime = (int)ChFontGetWidth(app->baseFont, buf);
    ChFontDrawText(canv, app->baseFont, buf, 436 + (72 - twTime) / 2, textBaselineY, 11, 0xFFE0E0E0);

    // 5. Radiant Gold Brand Tag
    ChFontDrawText(canv, app->baseFont, (char*)"K N O W I V A T E", 540, textBaselineY, 11, 0xFFFFD700);
}

static void RenderGameBoard(ChCanvas* canv) {
    // Green felt canvas
    ChDrawRect(canv, 0, 69, WINDOW_WIDTH, WINDOW_HEIGHT - 69, COLOR_TABLE_GREEN);

    // Top row (y=LAYOUT_TOP_Y = 78): Stock, Waste, Gap, Foundations (4)
    int yTop = LAYOUT_TOP_Y;

    // 1. Stock pile (col 0: x=LAYOUT_STOCK_X)
    int xStock = LAYOUT_STOCK_X;
    DrawSlotOutline(canv, xStock, yTop, -1);
    if (game.stock.count > 0) {
        DrawCard(canv, game.stock.cards[game.stock.count - 1], xStock, yTop, false);
    } else {
        // Recycle symbol / circle
        ChDrawCircleUnfilled(canv, xStock + CARD_WIDTH / 2, yTop + CARD_HEIGHT / 2, 12, COLOR_TOPBAR_BORDER);
    }

    // 2. Waste pile (col 1: x=LAYOUT_WASTE_X)
    int xWaste = LAYOUT_WASTE_X;
    DrawSlotOutline(canv, xWaste, yTop, -1);
    int animWaste = GetAnimatingCountForPile(PILE_WASTE, 0);
    int visibleWaste = game.waste.count - animWaste;
    if (visibleWaste > 0) {
        bool sel = (game.selectedPileType == PILE_WASTE);
        DrawCard(canv, game.waste.cards[visibleWaste - 1], xWaste, yTop, sel);
    }

    // 3. Foundations (col 3..6: x=LAYOUT_FOUND_BASE_X + f * LAYOUT_FOUND_STEP)
    for (int f = 0; f < FOUNDATION_COUNT; f++) {
        int xF = LAYOUT_FOUND_BASE_X + f * LAYOUT_FOUND_STEP;
        DrawSlotOutline(canv, xF, yTop, f);
        int animF = GetAnimatingCountForPile(PILE_FOUNDATION, f);
        int visibleF = game.foundations[f].count - animF;
        if (visibleF > 0) {
            bool sel = (game.selectedPileType == PILE_FOUNDATION && game.selectedPileIndex == f);
            DrawCard(canv, game.foundations[f].cards[visibleF - 1], xF, yTop, sel);
        }
    }

    // 4. Tableau Columns (7 columns: x = LAYOUT_TABLEAU_BASE_X + col * LAYOUT_TABLEAU_STEP, y = LAYOUT_TABLEAU_BASE_Y)
    int yTableauBase = LAYOUT_TABLEAU_BASE_Y;
    for (int col = 0; col < TABLEAU_COUNT; col++) {
        int xCol = LAYOUT_TABLEAU_BASE_X + col * LAYOUT_TABLEAU_STEP;
        CardStack* pile = &game.tableau[col];
        int animCol = GetAnimatingCountForPile(PILE_TABLEAU, col);
        int visibleCol = pile->count - animCol;

        if (visibleCol <= 0) {
            DrawSlotOutline(canv, xCol, yTableauBase, -1);
        } else {
            int curY = yTableauBase;
            for (int r = 0; r < visibleCol; r++) {
                Card c = pile->cards[r];
                bool isSel = (game.selectedPileType == PILE_TABLEAU &&
                              game.selectedPileIndex == col &&
                              r >= game.selectedCardIndex);

                DrawCard(canv, c, xCol, curY, isSel);
                curY += c.faceUp ? 24 : 14;
            }
        }
    }

    // 5. In-flight animated cards rendered on top of table with drop shadow
    for (int a = 0; a < MAX_CARD_ANIMATIONS; a++) {
        if (!activeAnims[a].active) continue;
        float t = (float)activeAnims[a].currentFrame / (float)activeAnims[a].totalFrames;
        if (t > 1.0f) t = 1.0f;
        float progress = 1.0f - (1.0f - t) * (1.0f - t); // Ease-out quad
        int curX = (int)(activeAnims[a].startX + (activeAnims[a].targetX - activeAnims[a].startX) * progress);
        int curY = (int)(activeAnims[a].startY + (activeAnims[a].targetY - activeAnims[a].startY) * progress);

        // Elevated soft drop-shadow
        int totalH = CARD_HEIGHT + (activeAnims[a].count - 1) * 24;
        DrawRoundedRect(canv, curX + 3, curY + 4, CARD_WIDTH, totalH, 6, 0xFF052B13);

        // Draw cards in flight
        int cardY = curY;
        for (int c = 0; c < activeAnims[a].count; c++) {
            DrawCard(canv, activeAnims[a].cards[c], curX, cardY, false);
            cardY += 24;
        }
    }

    // Render Victory Cascade if celebration active
    if (game.celebrationActive) {
        for (int i = 0; i < TOTAL_CARDS; i++) {
            if (game.cascadeCards[i].active) {
                DrawCard(canv, game.cascadeCards[i].card, (int)game.cascadeCards[i].x, (int)game.cascadeCards[i].y, false);
            }
        }

        // Victory dialog
        int dlgW = 340, dlgH = 140;
        int dlgX = (WINDOW_WIDTH - dlgW) / 2;
        int dlgY = (WINDOW_HEIGHT - dlgH) / 2;
        DrawRoundedRect(canv, dlgX, dlgY, dlgW, dlgH, 10, 0xFF0D3319);
        ChDrawRectUnfilled(canv, dlgX, dlgY, dlgW, dlgH, COLOR_CARD_SELECT_GLOW);

        int vtw = (int)ChFontGetWidth(app->baseFont, (char*)"VICTORY! YOU WON!");
        ChFontDrawText(canv, app->baseFont, (char*)"VICTORY! YOU WON!", dlgX + (dlgW - vtw) / 2, dlgY + 36, 16, COLOR_CARD_SELECT_GLOW);

        char stats[64];
        sprintf(stats, "Total Moves: %d  |  Time: %02d:%02d", game.moveCount, game.timeElapsedSec / 60, game.timeElapsedSec % 60);
        int stw = (int)ChFontGetWidth(app->baseFont, stats);
        ChFontDrawText(canv, app->baseFont, stats, dlgX + (dlgW - stw) / 2, dlgY + 68, 11, 0xFFFFFFFF);

        // "Play Again" button
        int btnW = 130, btnH = 32;
        int btnX = dlgX + (dlgW - btnW) / 2;
        int btnY = dlgY + 86;
        DrawRoundedRect(canv, btnX, btnY, btnW, btnH, 4, 0xFF145A25);
        ChDrawRectUnfilled(canv, btnX, btnY, btnW, btnH, 0xFF28A745);
        int ptw = (int)ChFontGetWidth(app->baseFont, (char*)"Play Again");
        ChFontDrawText(canv, app->baseFont, (char*)"Play Again", btnX + (btnW - ptw) / 2, btnY + btnH / 2 + 4, 12, 0xFFFFFFFF);
    }
}

static void RenderSplashScreen(ChCanvas* canv) {
    // Rich professional casino green splash screen
    ChDrawRect(canv, 0, 26, WINDOW_WIDTH, WINDOW_HEIGHT - 26, 0xFF083D1B);

    int cx = WINDOW_WIDTH / 2;
    int cy = WINDOW_HEIGHT / 2;

    // Outer decorative emblem box
    int bw = 460, bh = 240;
    int bx = cx - bw / 2;
    int by = cy - bh / 2 + 10;
    DrawRoundedRect(canv, bx, by, bw, bh, 12, 0xFF052B13);
    ChDrawRectUnfilled(canv, bx, by, bw, bh, COLOR_CARD_SELECT_GLOW);
    ChDrawRectUnfilled(canv, bx + 4, by + 4, bw - 8, bh - 8, COLOR_TOPBAR_BORDER);

    // Suits row
    DrawSuitGlyph(canv, CARD_SUIT_SPADES, cx - 75, by + 40, 14, 0xFFFFFFFF);
    DrawSuitGlyph(canv, CARD_SUIT_HEARTS, cx - 25, by + 40, 14, COLOR_RED_SUIT);
    DrawSuitGlyph(canv, CARD_SUIT_CLUBS, cx + 25, by + 40, 14, 0xFFFFFFFF);
    DrawSuitGlyph(canv, CARD_SUIT_DIAMONDS, cx + 75, by + 40, 14, COLOR_RED_SUIT);

    // "K N O W I V A T E"
    int tw1 = (int)ChFontGetWidth(app->baseFont, (char*)"K N O W I V A T E");
    ChFontDrawText(canv, app->baseFont, (char*)"K N O W I V A T E", cx - tw1 / 2, by + 85, 14, 0xFFE0E0E0);

    // "S O L I T A I R E"
    int tw2 = (int)ChFontGetWidth(app->baseFont, (char*)"S O L I T A I R E");
    ChFontDrawText(canv, app->baseFont, (char*)"S O L I T A I R E", cx - tw2 / 2, by + 128, 22, COLOR_CARD_SELECT_GLOW);

    // Subtitle
    int tw3 = (int)ChFontGetWidth(app->baseFont, (char*)"Classic Klondike  *  XenevaOS Edition");
    ChFontDrawText(canv, app->baseFont, (char*)"Classic Klondike  *  XenevaOS Edition", cx - tw3 / 2, by + 168, 11, 0xFFB0BEC5);

    // Loading / prompt
    int tw4 = (int)ChFontGetWidth(app->baseFont, (char*)"Click anywhere to start");
    ChFontDrawText(canv, app->baseFont, (char*)"Click anywhere to start", cx - tw4 / 2, by + 208, 10, 0xFF81C784);
}

static void RenderSplashCurtain(ChCanvas* canv, float alpha) {
    if (alpha <= 0.0f) return;
    int totalH = WINDOW_HEIGHT - 26;
    int halfH = totalH / 2;
    int curH = (int)(halfH * alpha);
    int centerY = 26 + halfH;

    // Top curtain
    int topY = centerY - curH;
    ChDrawRect(canv, 0, topY, WINDOW_WIDTH, curH, 0xFF083D1B);
    ChDrawHorizontalLine(canv, 0, centerY - 1, WINDOW_WIDTH, COLOR_CARD_SELECT_GLOW);

    // Bottom curtain
    ChDrawRect(canv, 0, centerY, WINDOW_WIDTH, curH, 0xFF083D1B);
    ChDrawHorizontalLine(canv, 0, centerY, WINDOW_WIDTH, COLOR_CARD_SELECT_GLOW);

    // If curtain is sufficiently open, show the central emblem
    if (alpha > 0.45f) {
        int cx = WINDOW_WIDTH / 2;
        int bw = 460, bh = 240;
        int bx = cx - bw / 2;
        int by = centerY - bh / 2;

        if (by >= topY && (by + bh) <= (centerY + curH)) {
            DrawRoundedRect(canv, bx, by, bw, bh, 12, 0xFF052B13);
            ChDrawRectUnfilled(canv, bx, by, bw, bh, COLOR_CARD_SELECT_GLOW);
            ChDrawRectUnfilled(canv, bx + 4, by + 4, bw - 8, bh - 8, COLOR_TOPBAR_BORDER);

            DrawSuitGlyph(canv, CARD_SUIT_SPADES, cx - 75, by + 40, 14, 0xFFFFFFFF);
            DrawSuitGlyph(canv, CARD_SUIT_HEARTS, cx - 25, by + 40, 14, COLOR_RED_SUIT);
            DrawSuitGlyph(canv, CARD_SUIT_CLUBS, cx + 25, by + 40, 14, 0xFFFFFFFF);
            DrawSuitGlyph(canv, CARD_SUIT_DIAMONDS, cx + 75, by + 40, 14, COLOR_RED_SUIT);

            int tw1 = (int)ChFontGetWidth(app->baseFont, (char*)"K N O W I V A T E");
            ChFontDrawText(canv, app->baseFont, (char*)"K N O W I V A T E", cx - tw1 / 2, by + 85, 14, 0xFFE0E0E0);
            int tw2 = (int)ChFontGetWidth(app->baseFont, (char*)"S O L I T A I R E");
            ChFontDrawText(canv, app->baseFont, (char*)"S O L I T A I R E", cx - tw2 / 2, by + 128, 22, COLOR_CARD_SELECT_GLOW);
            int tw3 = (int)ChFontGetWidth(app->baseFont, (char*)"Classic Klondike  *  XenevaOS Edition");
            ChFontDrawText(canv, app->baseFont, (char*)"Classic Klondike  *  XenevaOS Edition", cx - tw3 / 2, by + 168, 11, 0xFFB0BEC5);
        }
    }
}

static bool DoMoveCards(int srcType, int srcIdx, int srcCardIdx, int dstType, int dstIdx) {
    int srcX = 0, srcY = 0;
    GetPileCardPos(srcType, srcIdx, srcCardIdx, &srcX, &srcY);

    int dstCount = 0;
    if (dstType == PILE_TABLEAU && dstIdx >= 0 && dstIdx < TABLEAU_COUNT) {
        dstCount = game.tableau[dstIdx].count;
    } else if (dstType == PILE_FOUNDATION && dstIdx >= 0 && dstIdx < FOUNDATION_COUNT) {
        dstCount = game.foundations[dstIdx].count;
    }
    int dstX = 0, dstY = 0;
    GetPileCardPos(dstType, dstIdx, dstCount, &dstX, &dstY);

    Card tempCards[13];
    int moveCount = 0;
    if (srcType == PILE_TABLEAU && srcIdx >= 0 && srcIdx < TABLEAU_COUNT) {
        moveCount = game.tableau[srcIdx].count - srcCardIdx;
        if (moveCount > 13) moveCount = 13;
        for (int i = 0; i < moveCount; i++) {
            tempCards[i] = game.tableau[srcIdx].cards[srcCardIdx + i];
        }
    } else if (srcType == PILE_WASTE && game.waste.count > 0) {
        moveCount = 1;
        tempCards[0] = game.waste.cards[game.waste.count - 1];
    } else if (srcType == PILE_FOUNDATION && srcIdx >= 0 && srcIdx < FOUNDATION_COUNT && game.foundations[srcIdx].count > 0) {
        moveCount = 1;
        tempCards[0] = game.foundations[srcIdx].cards[game.foundations[srcIdx].count - 1];
    }

    bool moved = SolitaireMoveCards(srcType, srcIdx, srcCardIdx, dstType, dstIdx);
    if (moved && moveCount > 0) {
        StartCardAnimation(tempCards, moveCount, srcX, srcY, dstX, dstY, dstType, dstIdx);
    }
    return moved;
}

static bool DoAutoMoveToFoundation(int srcType, int srcIdx, int srcCardIdx) {
    int srcX = 0, srcY = 0;
    GetPileCardPos(srcType, srcIdx, srcCardIdx, &srcX, &srcY);

    Card c;
    if (srcType == PILE_WASTE && game.waste.count > 0) {
        c = game.waste.cards[game.waste.count - 1];
    } else if (srcType == PILE_TABLEAU && srcIdx >= 0 && srcIdx < TABLEAU_COUNT && game.tableau[srcIdx].count > 0) {
        c = game.tableau[srcIdx].cards[srcCardIdx];
    } else {
        return false;
    }

    int fIdx = c.suit;
    int dstX = 0, dstY = 0;
    GetPileCardPos(PILE_FOUNDATION, fIdx, 0, &dstX, &dstY);

    bool moved = SolitaireAutoMoveToFoundation(srcType, srcIdx, srcCardIdx);
    if (moved) {
        StartCardAnimation(&c, 1, srcX, srcY, dstX, dstY, PILE_FOUNDATION, fIdx);
    }
    return moved;
}

static void DoStockClick() {
    if (game.stock.count > 0) {
        Card c = game.stock.cards[game.stock.count - 1];
        c.faceUp = true;
        int srcX = LAYOUT_STOCK_X, srcY = LAYOUT_TOP_Y;
        int dstX = LAYOUT_WASTE_X, dstY = LAYOUT_TOP_Y;
        SolitaireStockClick();
        StartCardAnimation(&c, 1, srcX, srcY, dstX, dstY, PILE_WASTE, 0);
    } else {
        SolitaireStockClick();
    }
}

void SolitaireHandleMouse(int x, int y, int btn) {
    static int prevBtn = 0;
    bool isClickDown = (btn == 1 && prevBtn == 0);
    prevBtn = btn;

    if (!isClickDown) return; // Only trigger on leading edge of left click

    // 1. If in splash screen, click immediately completes splash
    if (game.splashState != SPLASH_DONE) {
        game.splashState = SPLASH_DONE;
        game.splashAlpha = 0.0f;
        return;
    }

    // 2. If celebration dialog active, check "Play Again" button
    if (game.celebrationActive) {
        int dlgW = 340, dlgH = 140;
        int dlgX = (WINDOW_WIDTH - dlgW) / 2;
        int dlgY = (WINDOW_HEIGHT - dlgH) / 2;
        int btnW = 130, btnH = 32;
        int btnX = dlgX + (dlgW - btnW) / 2;
        int btnY = dlgY + 86;
        if (x >= btnX && x <= btnX + btnW && y >= btnY && y <= btnY + btnH) {
            SolitaireRestart();
            return;
        }
    }

    // 3. Top bar buttons (y=34..60)
    if (y >= 30 && y <= 64) {
        // "New Game": x=15..100
        if (x >= 15 && x <= 100) {
            uint32_t newSeed = game.initialSeed + 997 + (uint32_t)frameCounter;
            SolitaireInit(newSeed);
            game.splashState = SPLASH_DONE;
            return;
        }
        // "Restart": x=108..183
        if (x >= 108 && x <= 183) {
            SolitaireRestart();
            return;
        }
        // "Undo": x=191..256
        if (x >= 191 && x <= 256) {
            if (SolitaireCanUndo()) {
                SolitaireUndo();
            }
            return;
        }
    }

    // Double-click detection (within 24 frames ≈ 400ms and 12px distance)
    bool isDoubleClick = false;
    if ((frameCounter - (int)lastClickFrame) <= 24 &&
        abs(x - lastClickX) <= 12 && abs(y - lastClickY) <= 12) {
        isDoubleClick = true;
    }
    lastClickFrame = frameCounter;
    lastClickX = x;
    lastClickY = y;

    // 4. Stock Pile (x=LAYOUT_STOCK_X..+CARD_WIDTH, y=LAYOUT_TOP_Y..+CARD_HEIGHT)
    if (x >= LAYOUT_STOCK_X && x <= (LAYOUT_STOCK_X + CARD_WIDTH) &&
        y >= LAYOUT_TOP_Y && y <= (LAYOUT_TOP_Y + CARD_HEIGHT)) {
        DoStockClick();
        return;
    }

    // 5. Waste Pile (x=LAYOUT_WASTE_X..+CARD_WIDTH, y=LAYOUT_TOP_Y..+CARD_HEIGHT)
    if (x >= LAYOUT_WASTE_X && x <= (LAYOUT_WASTE_X + CARD_WIDTH) &&
        y >= LAYOUT_TOP_Y && y <= (LAYOUT_TOP_Y + CARD_HEIGHT)) {
        if (game.waste.count > 0) {
            if (isDoubleClick) {
                // Auto move to foundation
                if (DoAutoMoveToFoundation(PILE_WASTE, 0, game.waste.count - 1)) {
                    return;
                }
            }
            // Toggle selection
            if (game.selectedPileType == PILE_WASTE) {
                game.selectedPileType = PILE_NONE;
                game.selectedPileIndex = -1;
                game.selectedCardIndex = -1;
            } else {
                game.selectedPileType = PILE_WASTE;
                game.selectedPileIndex = 0;
                game.selectedCardIndex = game.waste.count - 1;
            }
        }
        return;
    }

    // 6. Foundations (f=0..3: x=LAYOUT_FOUND_BASE_X + f*LAYOUT_FOUND_STEP)
    for (int f = 0; f < FOUNDATION_COUNT; f++) {
        int xF = LAYOUT_FOUND_BASE_X + f * LAYOUT_FOUND_STEP;
        if (x >= xF && x <= (xF + CARD_WIDTH) &&
            y >= LAYOUT_TOP_Y && y <= (LAYOUT_TOP_Y + CARD_HEIGHT)) {
            if (game.selectedPileType != PILE_NONE) {
                // Attempt to move selected card to foundation
                if (DoMoveCards(game.selectedPileType, game.selectedPileIndex, game.selectedCardIndex, PILE_FOUNDATION, f)) {
                    return;
                }
            }
            // If clicking directly on foundation with cards
            if (game.foundations[f].count > 0) {
                if (game.selectedPileType == PILE_FOUNDATION && game.selectedPileIndex == f) {
                    game.selectedPileType = PILE_NONE;
                    game.selectedPileIndex = -1;
                    game.selectedCardIndex = -1;
                } else {
                    game.selectedPileType = PILE_FOUNDATION;
                    game.selectedPileIndex = f;
                    game.selectedCardIndex = game.foundations[f].count - 1;
                }
            }
            return;
        }
    }

    // 7. Tableau Columns (col=0..6: x=LAYOUT_TABLEAU_BASE_X + col*LAYOUT_TABLEAU_STEP)
    for (int col = 0; col < TABLEAU_COUNT; col++) {
        int xCol = LAYOUT_TABLEAU_BASE_X + col * LAYOUT_TABLEAU_STEP;
        if (x >= xCol && x <= (xCol + CARD_WIDTH)) {
            CardStack* pile = &game.tableau[col];

            // Click on empty tableau slot
            if (pile->count == 0 && y >= LAYOUT_TABLEAU_BASE_Y && y <= (LAYOUT_TABLEAU_BASE_Y + CARD_HEIGHT)) {
                if (game.selectedPileType != PILE_NONE) {
                    DoMoveCards(game.selectedPileType, game.selectedPileIndex, game.selectedCardIndex, PILE_TABLEAU, col);
                }
                return;
            }

            // Click within a non-empty tableau column
            if (pile->count > 0 && y >= LAYOUT_TABLEAU_BASE_Y) {
                // Determine which card was clicked from top to bottom
                int hitCardIdx = -1;
                int curY = LAYOUT_TABLEAU_BASE_Y;

                for (int r = 0; r < pile->count; r++) {
                    int nextY = curY + (pile->cards[r].faceUp ? 24 : 14);
                    bool isLast = (r == pile->count - 1);
                    int cardBottom = isLast ? (curY + CARD_HEIGHT) : nextY;

                    if (y >= curY && y < cardBottom) {
                        hitCardIdx = r;
                        break;
                    }
                    curY = nextY;
                }

                if (hitCardIdx >= 0) {
                    Card hitCard = pile->cards[hitCardIdx];

                    // If double-clicked on top card, attempt auto-foundation move
                    if (isDoubleClick && hitCardIdx == pile->count - 1 && hitCard.faceUp) {
                        if (DoAutoMoveToFoundation(PILE_TABLEAU, col, hitCardIdx)) {
                            return;
                        }
                    }

                    // If we already have a selection from another pile, attempt move onto this column
                    if (game.selectedPileType != PILE_NONE &&
                        !(game.selectedPileType == PILE_TABLEAU && game.selectedPileIndex == col)) {
                        if (DoMoveCards(game.selectedPileType, game.selectedPileIndex, game.selectedCardIndex, PILE_TABLEAU, col)) {
                            return;
                        }
                    }

                    // Otherwise, toggle select this card (if face up)
                    if (hitCard.faceUp) {
                        if (game.selectedPileType == PILE_TABLEAU &&
                            game.selectedPileIndex == col &&
                            game.selectedCardIndex == hitCardIdx) {
                            game.selectedPileType = PILE_NONE;
                            game.selectedPileIndex = -1;
                            game.selectedCardIndex = -1;
                        } else {
                            game.selectedPileType = PILE_TABLEAU;
                            game.selectedPileIndex = col;
                            game.selectedCardIndex = hitCardIdx;
                        }
                    }
                    return;
                } else if (game.selectedPileType != PILE_NONE &&
                           !(game.selectedPileType == PILE_TABLEAU && game.selectedPileIndex == col)) {
                    // Clicked in column below cards: move selected card(s) onto this column
                    DoMoveCards(game.selectedPileType, game.selectedPileIndex, game.selectedCardIndex, PILE_TABLEAU, col);
                    return;
                }
            }
        }
    }

    // Clicked on empty table area: clear selection
    game.selectedPileType = PILE_NONE;
    game.selectedPileIndex = -1;
    game.selectedCardIndex = -1;
}

void WindowHandleMessage(PostEvent* e) {
    switch (e->type) {
    case DEODHAI_REPLY_MOUSE_EVENT: {
        int handle = e->dword4;
        if (mainWin && (int)mainWin->handle == handle) {
            int screenX = e->dword;
            int screenY = e->dword2;
            int button = e->dword3;

            ChWindowHandleMouse(mainWin, screenX, screenY, button);

            if (mainWin->info) {
                int localX = screenX - mainWin->info->x;
                int localY = screenY - mainWin->info->y;
                SolitaireHandleMouse(localX, localY, button);
            }
        }
        memset(e, 0, sizeof(PostEvent));
        break;
    }
    case DEODHAI_REPLY_KEY_EVENT: {
        int code = e->dword;
        if (code == 27) { // ESC: restart
            SolitaireRestart();
        } else if (code == 'n' || code == 'N') {
            SolitaireInit(game.initialSeed + 123);
            game.splashState = SPLASH_DONE;
        } else if (code == 'z' || code == 'Z') {
            if (SolitaireCanUndo()) {
                SolitaireUndo();
            }
        }
        memset(e, 0, sizeof(PostEvent));
        break;
    }
    default:
        memset(e, 0, sizeof(PostEvent));
        break;
    }
}

int main(int argc, char* argv[]) {
    app = ChitralekhaStartApp(argc, argv);

    _KePrint("===================================================\r\n");
    _KePrint("   [+] SOLITAIRE BY KNOWIVATE FOR XENEVAOS [+]     \r\n");
    _KePrint("===================================================\r\n");

    ChWindowBroadcastIcon(app, (char*)"/icons/solitr.bmp");

    mainWin = ChCreateWindow(app, WINDOW_FLAG_MOVABLE, (char*)"Solitaire by Knowivate", 40, 20, WINDOW_WIDTH, WINDOW_HEIGHT);
    mainWin->color = COLOR_TABLE_GREEN;

    ChWindowPaint(mainWin);

    // Initialize game with seed
    SolitaireInit(12345678);

    PostEvent e;
    memset(&e, 0, sizeof(PostEvent));

    while (1) {
        // Drain pending UI events
        while (1) {
            int err = _KeFileIoControl(app->postboxfd, POSTBOX_GET_EVENT, &e);
            if (err == POSTBOX_NO_EVENT) break;
            WindowHandleMessage(&e);
        }

        // Frame updates
        frameCounter++;
        if (frameCounter % 60 == 0) {
            SolitaireUpdateTimerTick();
        }

        SolitaireUpdateSplash();
        SolitaireUpdateCascade();
        UpdateCardAnimations();

        // Render Frame
        if (game.splashState == SPLASH_HOLD) {
            RenderSplashScreen(mainWin->canv);
        } else if (game.splashState == SPLASH_FADE_OUT) {
            RenderTopBar(mainWin->canv);
            RenderGameBoard(mainWin->canv);
            RenderSplashCurtain(mainWin->canv, game.splashAlpha);
        } else {
            RenderTopBar(mainWin->canv);
            RenderGameBoard(mainWin->canv);
        }

        ChWindowUpdate(mainWin, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 1, 0);

        _KeProcessSleep(16); // ~60 FPS
    }

    return 0;
}
