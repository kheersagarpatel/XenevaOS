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

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 560

#define COLOR_TABLE_GREEN      0xFF0B6623
#define COLOR_TOPBAR_BG        0xFF074718
#define COLOR_TOPBAR_BORDER    0xFF0E7A2C
#define COLOR_SLOT_OUTLINE     0xFF147A33
#define COLOR_CARD_WHITE       0xFFFCFCFC
#define COLOR_CARD_BORDER      0xFF666666
#define COLOR_CARD_SELECT_GLOW 0xFFFFD700
#define COLOR_RED_SUIT         0xFFD32F2F
#define COLOR_BLACK_SUIT       0xFF1A1A1A
#define COLOR_CARD_BACK_BASE   0xFF1A365D
#define COLOR_CARD_BACK_GOLD   0xFFD4AF37

ChitralekhaApp* app = NULL;
ChWindow* mainWin = NULL;

static int frameCounter = 0;
static uint32_t lastClickFrame = 0;
static int lastClickX = -1;
static int lastClickY = -1;

// Procedural vector suit rendering to avoid ASCII/Unicode missing font glyphs
static void DrawSuitGlyph(ChCanvas* canv, int suit, int cx, int cy, int size, uint32_t color) {
    if (suit == CARD_SUIT_HEARTS) {
        // Two lobes + downward triangle
        int r = size / 3;
        ChDrawFilledCircle(canv, cx - r, cy - r / 2, r, color);
        ChDrawFilledCircle(canv, cx + r, cy - r / 2, r, color);
        for (int y = 0; y <= size; y++) {
            int span = (size - y) * (size) / size;
            if (span > 0) {
                ChDrawHorizontalLine(canv, cx - span / 2, cy - r / 2 + y, span, color);
            }
        }
    } else if (suit == CARD_SUIT_DIAMONDS) {
        // Rhombus / diamond
        int halfH = size;
        int halfW = (size * 7) / 10;
        for (int dy = -halfH; dy <= halfH; dy++) {
            int w = halfW * (halfH - (dy < 0 ? -dy : dy)) / halfH;
            if (w > 0) {
                ChDrawHorizontalLine(canv, cx - w, cy + dy, w * 2, color);
            }
        }
    } else if (suit == CARD_SUIT_CLUBS) {
        // 3 circles + stem
        int r = size / 3;
        ChDrawFilledCircle(canv, cx, cy - r - 1, r, color);
        ChDrawFilledCircle(canv, cx - r, cy, r, color);
        ChDrawFilledCircle(canv, cx + r, cy, r, color);
        // stem base
        for (int dy = 0; dy <= size / 2; dy++) {
            int sw = 2 + dy / 2;
            ChDrawHorizontalLine(canv, cx - sw, cy + r / 2 + dy, sw * 2, color);
        }
    } else if (suit == CARD_SUIT_SPADES) {
        // Inverted heart + stem
        int r = size / 3;
        for (int y = 0; y <= size; y++) {
            int span = y * (size) / size;
            if (span > 0) {
                ChDrawHorizontalLine(canv, cx - span / 2, cy - size / 2 + (size - y), span, color);
            }
        }
        ChDrawFilledCircle(canv, cx - r, cy + r / 2, r, color);
        ChDrawFilledCircle(canv, cx + r, cy + r / 2, r, color);
        // stem base
        for (int dy = 0; dy <= size / 2; dy++) {
            int sw = 2 + dy / 2;
            ChDrawHorizontalLine(canv, cx - sw, cy + r + dy, sw * 2, color);
        }
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

static void DrawCardBack(ChCanvas* canv, int x, int y, int w, int h) {
    DrawRoundedRect(canv, x, y, w, h, 6, COLOR_CARD_BACK_BASE);
    ChDrawRectUnfilled(canv, x, y, w, h, COLOR_CARD_BORDER);
    ChDrawRectUnfilled(canv, x + 4, y + 4, w - 8, h - 8, COLOR_CARD_BACK_GOLD);

    // Elegant Knowivate center emblem
    int cx = x + w / 2;
    int cy = y + h / 2;
    ChDrawFilledCircle(canv, cx, cy, 14, COLOR_CARD_BACK_GOLD);
    ChDrawFilledCircle(canv, cx, cy, 11, COLOR_CARD_BACK_BASE);
    ChFontDrawText(canv, app->baseFont, (char*)"K", cx - 4, cy - 6, 12, COLOR_CARD_BACK_GOLD);
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

    // Top-left rank & mini suit
    ChFontDrawText(canv, app->baseFont, (char*)rankStr, x + 6, y + 5, 12, suitColor);
    DrawSuitGlyph(canv, card.suit, x + 11, y + 24, 7, suitColor);

    // Center Suit / Face Card Icon
    DrawSuitGlyph(canv, card.suit, x + CARD_WIDTH / 2, y + CARD_HEIGHT / 2, 14, suitColor);

    // Bottom-right rank
    int rw = (card.rank == 10) ? 14 : 9;
    ChFontDrawText(canv, app->baseFont, (char*)rankStr, x + CARD_WIDTH - rw - 5, y + CARD_HEIGHT - 18, 12, suitColor);
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
    // Top bar background
    ChDrawRect(canv, 0, 26, WINDOW_WIDTH, 42, COLOR_TOPBAR_BG);
    ChDrawHorizontalLine(canv, 0, 68, WINDOW_WIDTH, COLOR_TOPBAR_BORDER);

    // "New Game" button: x=15, y=32, w=85, h=28
    DrawRoundedRect(canv, 15, 32, 85, 28, 4, 0xFF145A25);
    ChDrawRectUnfilled(canv, 15, 32, 85, 28, 0xFF28A745);
    ChFontDrawText(canv, app->baseFont, (char*)"New Game", 25, 38, 11, 0xFFFFFFFF);

    // "Restart" button: x=110, y=32, w=75, h=28
    DrawRoundedRect(canv, 110, 32, 75, 28, 4, 0xFF145A25);
    ChDrawRectUnfilled(canv, 110, 32, 75, 28, 0xFF28A745);
    ChFontDrawText(canv, app->baseFont, (char*)"Restart", 125, 38, 11, 0xFFFFFFFF);

    // Stats
    char buf[64];
    sprintf(buf, "Moves: %d", game.moveCount);
    ChFontDrawText(canv, app->baseFont, buf, 215, 38, 12, 0xFFE0E0E0);

    sprintf(buf, "Score: %d", game.score);
    ChFontDrawText(canv, app->baseFont, buf, 320, 38, 12, 0xFFE0E0E0);

    int m = game.timeElapsedSec / 60;
    int s = game.timeElapsedSec % 60;
    sprintf(buf, "Time: %02d:%02d", m, s);
    ChFontDrawText(canv, app->baseFont, buf, 425, 38, 12, 0xFFE0E0E0);

    // Brand tag
    ChFontDrawText(canv, app->baseFont, (char*)"Solitaire by Knowivate", 615, 38, 12, COLOR_CARD_SELECT_GLOW);
}

static void RenderGameBoard(ChCanvas* canv) {
    // Green felt canvas
    ChDrawRect(canv, 0, 69, WINDOW_WIDTH, WINDOW_HEIGHT - 69, COLOR_TABLE_GREEN);

    // Top row (y=78): Stock, Waste, Gap, Foundations (4)
    int yTop = 78;

    // 1. Stock pile (col 0: x=25)
    int xStock = 25;
    DrawSlotOutline(canv, xStock, yTop, -1);
    if (game.stock.count > 0) {
        DrawCard(canv, game.stock.cards[game.stock.count - 1], xStock, yTop, false);
    } else {
        // Recycle symbol / circle
        ChDrawCircleUnfilled(canv, xStock + CARD_WIDTH / 2, yTop + CARD_HEIGHT / 2, 12, COLOR_TOPBAR_BORDER);
    }

    // 2. Waste pile (col 1: x=130)
    int xWaste = 130;
    DrawSlotOutline(canv, xWaste, yTop, -1);
    if (game.waste.count > 0) {
        bool sel = (game.selectedPileType == PILE_WASTE);
        DrawCard(canv, game.waste.cards[game.waste.count - 1], xWaste, yTop, sel);
    }

    // 3. Foundations (col 3..6: x=340, 445, 550, 655)
    for (int f = 0; f < FOUNDATION_COUNT; f++) {
        int xF = 340 + f * 105;
        DrawSlotOutline(canv, xF, yTop, f);
        if (game.foundations[f].count > 0) {
            bool sel = (game.selectedPileType == PILE_FOUNDATION && game.selectedPileIndex == f);
            DrawCard(canv, game.foundations[f].cards[game.foundations[f].count - 1], xF, yTop, sel);
        }
    }

    // Tableau Columns (7 columns: x = 25 + col * 105, y = 200)
    int yTableauBase = 200;
    for (int col = 0; col < TABLEAU_COUNT; col++) {
        int xCol = 25 + col * 105;
        CardStack* pile = &game.tableau[col];

        if (pile->count == 0) {
            DrawSlotOutline(canv, xCol, yTableauBase, -1);
        } else {
            int curY = yTableauBase;
            for (int r = 0; r < pile->count; r++) {
                Card c = pile->cards[r];
                bool isSel = (game.selectedPileType == PILE_TABLEAU &&
                              game.selectedPileIndex == col &&
                              r >= game.selectedCardIndex);

                DrawCard(canv, c, xCol, curY, isSel);
                curY += c.faceUp ? 24 : 14;
            }
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

        ChFontDrawText(canv, app->baseFont, (char*)"VICTORY! YOU WON!", dlgX + 65, dlgY + 22, 16, COLOR_CARD_SELECT_GLOW);

        char stats[64];
        sprintf(stats, "Total Moves: %d  |  Time: %02d:%02d", game.moveCount, game.timeElapsedSec / 60, game.timeElapsedSec % 60);
        ChFontDrawText(canv, app->baseFont, stats, dlgX + 45, dlgY + 55, 11, 0xFFFFFFFF);

        // "Play Again" button
        DrawRoundedRect(canv, dlgX + 105, dlgY + 85, 130, 32, 4, 0xFF145A25);
        ChDrawRectUnfilled(canv, dlgX + 105, dlgY + 85, 130, 32, 0xFF28A745);
        ChFontDrawText(canv, app->baseFont, (char*)"Play Again", dlgX + 135, dlgY + 93, 12, 0xFFFFFFFF);
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
    ChFontDrawText(canv, app->baseFont, (char*)"K N O W I V A T E", cx - 70, by + 80, 14, 0xFFE0E0E0);

    // "S O L I T A I R E"
    ChFontDrawText(canv, app->baseFont, (char*)"S O L I T A I R E", cx - 90, by + 120, 22, COLOR_CARD_SELECT_GLOW);

    // Subtitle
    ChFontDrawText(canv, app->baseFont, (char*)"Classic Klondike  *  XenevaOS Edition", cx - 110, by + 165, 11, 0xFFB0BEC5);

    // Loading / prompt
    ChFontDrawText(canv, app->baseFont, (char*)"Click anywhere to start", cx - 65, by + 200, 10, 0xFF81C784);
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

            ChFontDrawText(canv, app->baseFont, (char*)"K N O W I V A T E", cx - 70, by + 80, 14, 0xFFE0E0E0);
            ChFontDrawText(canv, app->baseFont, (char*)"S O L I T A I R E", cx - 90, by + 120, 22, COLOR_CARD_SELECT_GLOW);
            ChFontDrawText(canv, app->baseFont, (char*)"Classic Klondike  *  XenevaOS Edition", cx - 110, by + 165, 11, 0xFFB0BEC5);
        }
    }
}

void SolitaireHandleMouse(int btn, int x, int y) {
    if (btn != 1) return; // Only process left-click down

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
        int btnX = dlgX + 105, btnY = dlgY + 85, btnW = 130, btnH = 32;
        if (x >= btnX && x <= btnX + btnW && y >= btnY && y <= btnY + btnH) {
            SolitaireRestart();
            return;
        }
    }

    // 3. Top bar buttons
    if (y >= 32 && y <= 60) {
        // "New Game": x=15..100
        if (x >= 15 && x <= 100) {
            uint32_t newSeed = game.initialSeed + 997 + (uint32_t)frameCounter;
            SolitaireInit(newSeed);
            game.splashState = SPLASH_DONE;
            return;
        }
        // "Restart": x=110..185
        if (x >= 110 && x <= 185) {
            SolitaireRestart();
            return;
        }
    }

    // Double-click detection (within 24 frames ≈ 400ms and 10px distance)
    bool isDoubleClick = false;
    if ((frameCounter - (int)lastClickFrame) <= 24 &&
        abs(x - lastClickX) <= 10 && abs(y - lastClickY) <= 10) {
        isDoubleClick = true;
    }
    lastClickFrame = frameCounter;
    lastClickX = x;
    lastClickY = y;

    // 4. Stock Pile (x=25..100, y=78..183)
    if (x >= 25 && x <= (25 + CARD_WIDTH) && y >= 78 && y <= (78 + CARD_HEIGHT)) {
        SolitaireStockClick();
        return;
    }

    // 5. Waste Pile (x=130..205, y=78..183)
    if (x >= 130 && x <= (130 + CARD_WIDTH) && y >= 78 && y <= (78 + CARD_HEIGHT)) {
        if (game.waste.count > 0) {
            if (isDoubleClick) {
                // Auto move to foundation
                if (SolitaireAutoMoveToFoundation(PILE_WASTE, 0, game.waste.count - 1)) {
                    return;
                }
            }
            // Select waste card
            game.selectedPileType = PILE_WASTE;
            game.selectedPileIndex = 0;
            game.selectedCardIndex = game.waste.count - 1;
        }
        return;
    }

    // 6. Foundations (f=0..3: x=340 + f*105)
    for (int f = 0; f < FOUNDATION_COUNT; f++) {
        int xF = 340 + f * 105;
        if (x >= xF && x <= (xF + CARD_WIDTH) && y >= 78 && y <= (78 + CARD_HEIGHT)) {
            if (game.selectedPileType != PILE_NONE) {
                // Attempt to move selected card to foundation
                if (SolitaireMoveCards(game.selectedPileType, game.selectedPileIndex, game.selectedCardIndex, PILE_FOUNDATION, f)) {
                    return;
                }
            }
            // If clicking directly on foundation with cards
            if (game.foundations[f].count > 0) {
                game.selectedPileType = PILE_FOUNDATION;
                game.selectedPileIndex = f;
                game.selectedCardIndex = game.foundations[f].count - 1;
            }
            return;
        }
    }

    // 7. Tableau Columns (col=0..6: x=25 + col*105)
    for (int col = 0; col < TABLEAU_COUNT; col++) {
        int xCol = 25 + col * 105;
        if (x >= xCol && x <= (xCol + CARD_WIDTH)) {
            CardStack* pile = &game.tableau[col];

            // Click on empty tableau slot
            if (pile->count == 0 && y >= 200 && y <= (200 + CARD_HEIGHT)) {
                if (game.selectedPileType != PILE_NONE) {
                    SolitaireMoveCards(game.selectedPileType, game.selectedPileIndex, game.selectedCardIndex, PILE_TABLEAU, col);
                }
                return;
            }

            // Click within a non-empty tableau column
            if (pile->count > 0 && y >= 200) {
                // Determine which card was clicked from top to bottom
                int hitCardIdx = -1;
                int curY = 200;

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
                        if (SolitaireAutoMoveToFoundation(PILE_TABLEAU, col, hitCardIdx)) {
                            return;
                        }
                    }

                    // If we already have a selection from another pile, attempt move onto this column
                    if (game.selectedPileType != PILE_NONE &&
                        !(game.selectedPileType == PILE_TABLEAU && game.selectedPileIndex == col)) {
                        if (SolitaireMoveCards(game.selectedPileType, game.selectedPileIndex, game.selectedCardIndex, PILE_TABLEAU, col)) {
                            return;
                        }
                    }

                    // Otherwise, select this card (if face up)
                    if (hitCard.faceUp) {
                        game.selectedPileType = PILE_TABLEAU;
                        game.selectedPileIndex = col;
                        game.selectedCardIndex = hitCardIdx;
                    }
                    return;
                }
            }
        }
    }

    // Clicked on empty space: clear selection
    game.selectedPileType = PILE_NONE;
    game.selectedPileIndex = -1;
    game.selectedCardIndex = -1;
}

void WindowHandleMessage(PostEvent* e) {
    switch (e->type) {
    case DEODHAI_REPLY_MOUSE_EVENT: {
        int handle = e->dword4;
        if (mainWin && (int)mainWin->handle == handle) {
            ChWindowHandleMouse(mainWin, e->dword, e->dword2, e->dword3);
            SolitaireHandleMouse(e->dword, e->dword2, e->dword3);
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

    mainWin = ChCreateWindow(app, WINDOW_FLAG_MOVABLE, (char*)"Solitaire by Knowivate", 120, 80, WINDOW_WIDTH, WINDOW_HEIGHT);
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
