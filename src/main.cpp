#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include <array>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <map>

struct Move {
    int from = -1;
    int to = -1;
    char promotion = 0;
    bool castle = false;
    bool enPassant = false;
};

enum class Screen {
    Loading,
    Menu,
    Difficulty,
    Game,
    Settings,
    About,
    Credits
};

std::array<char, 64> board;
bool whiteTurn = true;
bool vsAI = false;
bool gameOver = false;
bool soundOn = true;
bool showHints = true;
int difficulty = 2;
int selected = -1;
int enPassantSquare = -1;
int halfmoveClock = 0;
int fullmoveNumber = 1;

bool whiteKingMoved = false;
bool blackKingMoved = false;
bool whiteRookAMoved = false;
bool whiteRookHMoved = false;
bool blackRookAMoved = false;
bool blackRookHMoved = false;

Screen screen = Screen::Loading;
DWORD loadStarted = 0;
int hoverX = -1;
int hoverY = -1;
const int designWidth = 1100;
const int designHeight = 760;
std::string statusText = "White to move";
std::vector<Move> selectedMoves;
std::vector<std::string> moveHistory;
std::map<std::string, int> repetition;
std::mt19937 rng((unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());

const int boardX = 34;
const int boardY = 50;
const int squareSize = 78;

bool isWhite(char p) { return p >= 'A' && p <= 'Z'; }
bool isBlack(char p) { return p >= 'a' && p <= 'z'; }
bool sameSide(char a, char b) { return a != '.' && b != '.' && isWhite(a) == isWhite(b); }
int rowOf(int s) { return s / 8; }
int colOf(int s) { return s % 8; }
bool inside(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }

std::string squareName(int s) {
    std::string out;
    out += char('a' + colOf(s));
    out += char('8' - rowOf(s));
    return out;
}

enum class SoundCue {
    Click,
    Move,
    Capture,
    Check,
    GameStart,
    GameEnd
};

void playSound(SoundCue cue) {
    if (!soundOn) return;

    const wchar_t* file=L"assets\\audio\\ui_click.wav";

    if (cue==SoundCue::Move) file=L"assets\\audio\\move.wav";
    else if (cue==SoundCue::Capture) file=L"assets\\audio\\capture.wav";
    else if (cue==SoundCue::Check) file=L"assets\\audio\\check.wav";
    else if (cue==SoundCue::GameStart) file=L"assets\\audio\\game_start.wav";
    else if (cue==SoundCue::GameEnd) file=L"assets\\audio\\game_end.wav";

    PlaySoundW(file,nullptr,SND_FILENAME|SND_ASYNC|SND_NODEFAULT);
}

void playUiSound(UINT = MB_OK) {
    playSound(SoundCue::Click);
}

std::string positionKey() {
    std::string key(board.begin(), board.end());
    key += whiteTurn ? 'w' : 'b';
    key += whiteKingMoved ? '1' : '0';
    key += blackKingMoved ? '1' : '0';
    key += whiteRookAMoved ? '1' : '0';
    key += whiteRookHMoved ? '1' : '0';
    key += blackRookAMoved ? '1' : '0';
    key += blackRookHMoved ? '1' : '0';
    key += std::to_string(enPassantSquare);
    return key;
}

void resetGame() {
    std::string start =
        "rnbqkbnr"
        "pppppppp"
        "........"
        "........"
        "........"
        "........"
        "PPPPPPPP"
        "RNBQKBNR";

    for (int i = 0; i < 64; i++) board[i] = start[i];

    whiteTurn = true;
    gameOver = false;
    selected = -1;
    enPassantSquare = -1;
    halfmoveClock = 0;
    fullmoveNumber = 1;
    whiteKingMoved = false;
    blackKingMoved = false;
    whiteRookAMoved = false;
    whiteRookHMoved = false;
    blackRookAMoved = false;
    blackRookHMoved = false;
    statusText = "White to move";
    selectedMoves.clear();
    moveHistory.clear();
    repetition.clear();
    repetition[positionKey()] = 1;
}

bool squareAttacked(const std::array<char,64>& state, int square, bool byWhite) {
    int row = rowOf(square), col = colOf(square);
    int pawnRow = row + (byWhite ? 1 : -1);

    for (int dc : {-1,1}) {
        int c = col + dc;
        if (inside(pawnRow,c) && state[pawnRow*8+c] == (byWhite ? 'P' : 'p')) return true;
    }

    const int knight[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
    for (auto& m : knight) {
        int r=row+m[0], c=col+m[1];
        if (inside(r,c) && state[r*8+c] == (byWhite ? 'N':'n')) return true;
    }

    const int diag[4][2]={{-1,-1},{-1,1},{1,-1},{1,1}};
    for (auto& d : diag) {
        int r=row+d[0], c=col+d[1];
        while (inside(r,c)) {
            char p=state[r*8+c];
            if (p!='.') {
                if (p==(byWhite?'B':'b') || p==(byWhite?'Q':'q')) return true;
                break;
            }
            r+=d[0]; c+=d[1];
        }
    }

    const int straight[4][2]={{-1,0},{1,0},{0,-1},{0,1}};
    for (auto& d : straight) {
        int r=row+d[0], c=col+d[1];
        while (inside(r,c)) {
            char p=state[r*8+c];
            if (p!='.') {
                if (p==(byWhite?'R':'r') || p==(byWhite?'Q':'q')) return true;
                break;
            }
            r+=d[0]; c+=d[1];
        }
    }

    for (int dr=-1; dr<=1; dr++) for (int dc=-1; dc<=1; dc++) {
        if (!dr && !dc) continue;
        int r=row+dr,c=col+dc;
        if (inside(r,c) && state[r*8+c] == (byWhite?'K':'k')) return true;
    }

    return false;
}

bool kingInCheck(const std::array<char,64>& state, bool white) {
    char king = white ? 'K' : 'k';
    for (int i=0;i<64;i++) if (state[i]==king) return squareAttacked(state,i,!white);
    return true;
}

void addSliding(std::vector<Move>& moves, int from, const int dirs[][2], int count) {
    int row=rowOf(from), col=colOf(from);
    for (int i=0;i<count;i++) {
        int r=row+dirs[i][0], c=col+dirs[i][1];
        while (inside(r,c)) {
            int to=r*8+c;
            if (board[to]=='.') moves.push_back({from,to});
            else {
                if (!sameSide(board[from],board[to])) moves.push_back({from,to});
                break;
            }
            r+=dirs[i][0]; c+=dirs[i][1];
        }
    }
}

std::vector<Move> pseudoMoves(bool white) {
    std::vector<Move> moves;

    for (int from=0; from<64; from++) {
        char piece=board[from];
        if (piece=='.' || isWhite(piece)!=white) continue;

        int row=rowOf(from), col=colOf(from);
        char lower=(char)std::tolower((unsigned char)piece);

        if (lower=='p') {
            int dir=white?-1:1;
            int start=white?6:1;
            int promo=white?0:7;
            int r=row+dir;

            if (inside(r,col) && board[r*8+col]=='.') {
                int to=r*8+col;
                if (r==promo) {
                    for (char q : {white?'Q':'q',white?'R':'r',white?'B':'b',white?'N':'n'})
                        moves.push_back({from,to,q});
                } else moves.push_back({from,to});

                int rr=row+dir*2;
                if (row==start && board[rr*8+col]=='.') moves.push_back({from,rr*8+col});
            }

            for (int dc : {-1,1}) {
                int c=col+dc;
                if (!inside(r,c)) continue;
                int to=r*8+c;

                if (board[to]!='.' && !sameSide(piece,board[to])) {
                    if (r==promo) {
                        for (char q : {white?'Q':'q',white?'R':'r',white?'B':'b',white?'N':'n'})
                            moves.push_back({from,to,q});
                    } else moves.push_back({from,to});
                } else if (to==enPassantSquare) {
                    Move m{from,to};
                    m.enPassant=true;
                    moves.push_back(m);
                }
            }
        }

        else if (lower=='n') {
            const int km[8][2]={{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
            for (auto& m:km) {
                int r=row+m[0],c=col+m[1];
                if (!inside(r,c)) continue;
                int to=r*8+c;
                if (board[to]=='.' || !sameSide(piece,board[to])) moves.push_back({from,to});
            }
        }

        else if (lower=='b') {
            const int d[4][2]={{-1,-1},{-1,1},{1,-1},{1,1}};
            addSliding(moves,from,d,4);
        }

        else if (lower=='r') {
            const int d[4][2]={{-1,0},{1,0},{0,-1},{0,1}};
            addSliding(moves,from,d,4);
        }

        else if (lower=='q') {
            const int d[8][2]={{-1,-1},{-1,1},{1,-1},{1,1},{-1,0},{1,0},{0,-1},{0,1}};
            addSliding(moves,from,d,8);
        }

        else if (lower=='k') {
            for (int dr=-1;dr<=1;dr++) for (int dc=-1;dc<=1;dc++) {
                if (!dr&&!dc) continue;
                int r=row+dr,c=col+dc;
                if (!inside(r,c)) continue;
                int to=r*8+c;
                if (board[to]=='.' || !sameSide(piece,board[to])) moves.push_back({from,to});
            }

            if (white && from==60 && !whiteKingMoved && !kingInCheck(board,true)) {
                if (!whiteRookHMoved && board[61]=='.' && board[62]=='.' && board[63]=='R' &&
                    !squareAttacked(board,61,false) && !squareAttacked(board,62,false)) {
                    Move m{60,62}; m.castle=true; moves.push_back(m);
                }
                if (!whiteRookAMoved && board[59]=='.' && board[58]=='.' && board[57]=='.' && board[56]=='R' &&
                    !squareAttacked(board,59,false) && !squareAttacked(board,58,false)) {
                    Move m{60,58}; m.castle=true; moves.push_back(m);
                }
            }

            if (!white && from==4 && !blackKingMoved && !kingInCheck(board,false)) {
                if (!blackRookHMoved && board[5]=='.' && board[6]=='.' && board[7]=='r' &&
                    !squareAttacked(board,5,true) && !squareAttacked(board,6,true)) {
                    Move m{4,6}; m.castle=true; moves.push_back(m);
                }
                if (!blackRookAMoved && board[3]=='.' && board[2]=='.' && board[1]=='.' && board[0]=='r' &&
                    !squareAttacked(board,3,true) && !squareAttacked(board,2,true)) {
                    Move m{4,2}; m.castle=true; moves.push_back(m);
                }
            }
        }
    }

    return moves;
}

void applyMoveTo(std::array<char,64>& state, const Move& move) {
    char piece=state[move.from];
    state[move.to]=move.promotion?move.promotion:piece;
    state[move.from]='.';

    if (move.enPassant) state[move.to+(isWhite(piece)?8:-8)]='.';

    if (move.castle) {
        if (move.to==62) { state[61]=state[63]; state[63]='.'; }
        if (move.to==58) { state[59]=state[56]; state[56]='.'; }
        if (move.to==6)  { state[5]=state[7]; state[7]='.'; }
        if (move.to==2)  { state[3]=state[0]; state[0]='.'; }
    }
}

std::vector<Move> legalMoves(bool white) {
    std::vector<Move> out;
    for (const Move& m:pseudoMoves(white)) {
        auto test=board;
        applyMoveTo(test,m);
        if (!kingInCheck(test,white)) out.push_back(m);
    }
    return out;
}

bool insufficientMaterial() {
    int bishops=0, knights=0, others=0;
    for (char p:board) {
        char q=(char)std::tolower((unsigned char)p);
        if (q=='p'||q=='r'||q=='q') others++;
        if (q=='b') bishops++;
        if (q=='n') knights++;
    }
    if (others) return false;
    return bishops+knights<=1;
}

void updateGameState() {
    auto moves=legalMoves(whiteTurn);

    if (moves.empty()) {
        gameOver=true;
        if (kingInCheck(board,whiteTurn)) statusText=whiteTurn?"Checkmate - Black wins":"Checkmate - White wins";
        else statusText="Draw - stalemate";
        playSound(SoundCue::GameEnd);
        return;
    }

    if (halfmoveClock>=100) {
        gameOver=true;
        statusText="Draw - fifty move rule";
        return;
    }

    if (repetition[positionKey()]>=3) {
        gameOver=true;
        statusText="Draw - threefold repetition";
        return;
    }

    if (insufficientMaterial()) {
        gameOver=true;
        statusText="Draw - insufficient material";
        return;
    }

    if (kingInCheck(board,whiteTurn)) {
        statusText=whiteTurn?"White is in check":"Black is in check";
        playSound(SoundCue::GameEnd);
    } else statusText=whiteTurn?"White to move":"Black to move";
}

int pieceValue(char p) {
    switch (std::tolower((unsigned char)p)) {
        case 'p': return 100;
        case 'n': return 320;
        case 'b': return 330;
        case 'r': return 500;
        case 'q': return 900;
        case 'k': return 20000;
    }
    return 0;
}

int evaluate() {
    int score=0;
    for (char p:board) {
        if (p=='.') continue;
        score += isWhite(p) ? pieceValue(p) : -pieceValue(p);
    }
    return score;
}

void doMove(const Move& move, bool record=true) {
    char piece=board[move.from];
    char captured=board[move.to];

    if (record) {
        std::string text=squareName(move.from)+"-"+squareName(move.to);
        if (move.castle) text += " castle";
        if (move.enPassant) text += " e.p.";
        if (move.promotion) { text += "="; text += (char)std::toupper((unsigned char)move.promotion); }
        moveHistory.push_back(text);
    }

    if (std::tolower((unsigned char)piece)=='p' || captured!='.' || move.enPassant) halfmoveClock=0;
    else halfmoveClock++;

    if (piece=='K') whiteKingMoved=true;
    if (piece=='k') blackKingMoved=true;
    if (move.from==56 || move.to==56) whiteRookAMoved=true;
    if (move.from==63 || move.to==63) whiteRookHMoved=true;
    if (move.from==0 || move.to==0) blackRookAMoved=true;
    if (move.from==7 || move.to==7) blackRookHMoved=true;

    enPassantSquare=-1;
    if (std::tolower((unsigned char)piece)=='p' && std::abs(move.to-move.from)==16)
        enPassantSquare=(move.from+move.to)/2;

    applyMoveTo(board,move);
    if (!whiteTurn) fullmoveNumber++;

    selected=-1;
    selectedMoves.clear();
    whiteTurn=!whiteTurn;
    repetition[positionKey()]++;
    if (record) {
        if (wasCapture) playSound(SoundCue::Capture);
        else playSound(SoundCue::Move);
    }
    updateGameState();
}

struct Snapshot {
    std::array<char,64> b;
    bool turn;
    int ep;
    int half;
    int full;
    bool wkm,bkm,wra,wrh,bra,brh;
};

Snapshot snap() {
    return {board,whiteTurn,enPassantSquare,halfmoveClock,fullmoveNumber,
        whiteKingMoved,blackKingMoved,whiteRookAMoved,whiteRookHMoved,blackRookAMoved,blackRookHMoved};
}

void restore(const Snapshot& s) {
    board=s.b; whiteTurn=s.turn; enPassantSquare=s.ep; halfmoveClock=s.half; fullmoveNumber=s.full;
    whiteKingMoved=s.wkm; blackKingMoved=s.bkm; whiteRookAMoved=s.wra; whiteRookHMoved=s.wrh;
    blackRookAMoved=s.bra; blackRookHMoved=s.brh;
}

int minimax(int depth, int alpha, int beta, bool maximizing) {
    auto moves=legalMoves(whiteTurn);
    if (depth==0 || moves.empty()) return evaluate();

    if (maximizing) {
        int best=-1000000;
        for (auto& m:moves) {
            Snapshot s=snap();
            doMove(m,false);
            int value=minimax(depth-1,alpha,beta,false);
            restore(s);
            best=(std::max)(best,value);
            alpha=(std::max)(alpha,value);
            if (beta<=alpha) break;
        }
        return best;
    } else {
        int best=1000000;
        for (auto& m:moves) {
            Snapshot s=snap();
            doMove(m,false);
            int value=minimax(depth-1,alpha,beta,true);
            restore(s);
            best=(std::min)(best,value);
            beta=(std::min)(beta,value);
            if (beta<=alpha) break;
        }
        return best;
    }
}

void aiMove() {
    if (!vsAI || whiteTurn || gameOver || screen!=Screen::Game) return;

    auto moves=legalMoves(false);
    if (moves.empty()) return;

    Move chosen=moves[0];

    if (difficulty==1) {
        std::uniform_int_distribution<int> pick(0,(int)moves.size()-1);
        chosen=moves[pick(rng)];
    } else {
        int depth=difficulty==2?1:(difficulty==3?2:3);
        int best=1000000;
        std::vector<Move> bestMoves;

        for (auto& m:moves) {
            Snapshot s=snap();
            doMove(m,false);
            int value=minimax(depth-1,-1000000,1000000,true);
            restore(s);

            if (value<best) {
                best=value;
                bestMoves.clear();
                bestMoves.push_back(m);
            } else if (value==best) bestMoves.push_back(m);
        }

        std::uniform_int_distribution<int> pick(0,(int)bestMoves.size()-1);
        chosen=bestMoves[pick(rng)];
    }

    doMove(chosen,true);
}

std::wstring pieceSymbol(char p) {
    switch(p) {
        case 'K': return L"\u2654"; case 'Q': return L"\u2655"; case 'R': return L"\u2656";
        case 'B': return L"\u2657"; case 'N': return L"\u2658"; case 'P': return L"\u2659";
        case 'k': return L"\u265A"; case 'q': return L"\u265B"; case 'r': return L"\u265C";
        case 'b': return L"\u265D"; case 'n': return L"\u265E"; case 'p': return L"\u265F";
    }
    return L"";
}

HFONT font(int size, int weight=FW_NORMAL, const wchar_t* face=L"Segoe UI") {
    return CreateFontW(size,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,face);
}

void fill(HDC dc, RECT r, COLORREF c) {
    HBRUSH b=CreateSolidBrush(c);
    FillRect(dc,&r,b);
    DeleteObject(b);
}

void text(HDC dc, const wchar_t* value, RECT r, int size, COLORREF c, int flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE, int weight=FW_NORMAL) {
    HFONT f=font(size,weight);
    HFONT old=(HFONT)SelectObject(dc,f);
    SetTextColor(dc,c);
    SetBkMode(dc,TRANSPARENT);
    DrawTextW(dc,value,-1,&r,flags);
    SelectObject(dc,old);
    DeleteObject(f);
}

void button(HDC dc, RECT r, const wchar_t* label, bool accent=false) {
    bool hovered = hoverX >= r.left && hoverX <= r.right && hoverY >= r.top && hoverY <= r.bottom;

    COLORREF bg = accent ? RGB(72,55,30) : RGB(27,31,39);
    COLORREF border = accent ? RGB(226,190,105) : RGB(70,77,91);

    if (hovered) {
        bg = accent ? RGB(92,70,37) : RGB(39,44,54);
        border = accent ? RGB(244,211,129) : RGB(104,113,130);
    }

    HBRUSH brush = CreateSolidBrush(bg);
    HPEN pen = CreatePen(PS_SOLID, hovered ? 2 : 1, border);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, brush);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);

    RoundRect(dc,r.left,r.top,r.right,r.bottom,14,14);

    SelectObject(dc,oldBrush);
    SelectObject(dc,oldPen);
    DeleteObject(brush);
    DeleteObject(pen);

    text(dc,label,r,18,RGB(244,246,250),DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_SEMIBOLD);
}

bool hit(int x,int y,RECT r) { return x>=r.left&&x<=r.right&&y>=r.top&&y<=r.bottom; }


void panel(HDC dc, RECT r) {
    HBRUSH brush = CreateSolidBrush(RGB(22,25,32));
    HPEN pen = CreatePen(PS_SOLID,1,RGB(53,59,70));
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc,brush);
    HPEN oldPen = (HPEN)SelectObject(dc,pen);
    RoundRect(dc,r.left,r.top,r.right,r.bottom,18,18);
    SelectObject(dc,oldBrush);
    SelectObject(dc,oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void drawLoading(HDC dc) {
    RECT whole={0,0,designWidth,designHeight};
    fill(dc,whole,RGB(11,13,17));

    DWORD elapsed = GetTickCount() - loadStarted;
    float progress = elapsed >= 3300 ? 1.0f : elapsed / 3300.0f;

    text(dc,L"\u265A",{0,120,designWidth,270},104,RGB(226,190,105),
         DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_NORMAL);

    text(dc,L"OPSECCHESS",{0,270,designWidth,330},42,RGB(245,246,249),
         DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_BOLD);

    text(dc,L"INITIALISING OFFLINE CHESS ENGINE",{0,330,designWidth,370},15,RGB(132,140,154),
         DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_SEMIBOLD);

    RECT track={300,445,800,453};
    fill(dc,track,RGB(38,42,50));

    RECT bar={300,445,300 + int(500 * progress),453};
    fill(dc,bar,RGB(226,190,105));

    const wchar_t* stage =
        progress < .22f ? L"STARTING CORE" :
        progress < .48f ? L"LOADING CHESS RULES" :
        progress < .72f ? L"PREPARING AI ENGINE" :
        progress < .92f ? L"LOADING INTERFACE" :
                          L"READY";

    text(dc,stage,{300,470,800,505},14,RGB(165,172,184),
         DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_SEMIBOLD);

    int percent = int(progress * 100.0f);
    std::wstring pct = std::to_wstring(percent) + L"%";
    text(dc,pct.c_str(),{300,510,800,545},14,RGB(105,113,127),
         DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    text(dc,L"ARCHIE B.  •  DEVELOPER",{0,650,designWidth,690},13,RGB(82,89,102),
         DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_SEMIBOLD);
}

void drawMenu(HDC dc) {
    RECT whole={0,0,designWidth,designHeight};
    fill(dc,whole,RGB(11,13,17));

    // subtle top branding
    text(dc,L"\u265A",{58,42,118,102},45,RGB(226,190,105),
         DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    text(dc,L"OPSECCHESS",{125,45,370,92},28,RGB(245,246,249),
         DT_LEFT|DT_VCENTER|DT_SINGLELINE,FW_BOLD);
    text(dc,L"OFFLINE CHESS",{128,82,370,112},12,RGB(105,113,127),
         DT_LEFT|DT_VCENTER|DT_SINGLELINE,FW_SEMIBOLD);

    // hero area
    text(dc,L"PLAY. THINK. IMPROVE.",{70,165,650,225},42,RGB(245,246,249),
         DT_LEFT|DT_VCENTER|DT_SINGLELINE,FW_BOLD);
    text(dc,L"A focused desktop chess experience built for local play.",{73,225,650,270},
         17,RGB(137,145,158),DT_LEFT|DT_VCENTER|DT_SINGLELINE);

    button(dc,{72,315,420,374},L"PLAY VS AI",true);
    button(dc,{72,390,420,449},L"TWO PLAYER");
    button(dc,{72,465,238,519},L"SETTINGS");
    button(dc,{254,465,420,519},L"CREDITS");
    button(dc,{72,535,238,589},L"ABOUT");
    button(dc,{254,535,420,589},L"EXIT");

    // right feature card
    panel(dc,{650,145,1020,610});
    text(dc,L"\u265B",{650,185,1020,315},88,RGB(226,190,105),
         DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    text(dc,L"CLASSIC CHESS",{690,325,980,370},25,RGB(241,243,247),
         DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_BOLD);
    text(dc,L"LOCAL 2 PLAYER  •  AI OPPONENT",{680,370,990,405},13,RGB(128,136,149),
         DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_SEMIBOLD);
    text(dc,L"LEGAL MOVES  •  CASTLING  •  EN PASSANT",{670,420,1000,450},12,RGB(102,110,123),
         DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    text(dc,L"PROMOTION  •  CHECKMATE  •  DRAW RULES",{670,452,1000,482},12,RGB(102,110,123),
         DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    text(dc,L"NO ACCOUNTS  •  NO NETWORKING  •  NO TELEMETRY",{70,690,1030,720},
         12,RGB(72,79,91),DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_SEMIBOLD);
}

void drawDifficulty(HDC dc) {
    RECT whole={0,0,1100,760}; fill(dc,whole,RGB(16,18,23));
    text(dc,L"CHOOSE AI DIFFICULTY",{0,80,1100,140},38,RGB(240,242,247),DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_BOLD);
    button(dc,{170,210,430,280},L"EASY");
    button(dc,{670,210,930,280},L"MEDIUM");
    button(dc,{170,330,430,400},L"HARD",true);
    button(dc,{670,330,930,400},L"EXPERT",true);
    text(dc,L"easy: varied/random moves",{170,285,430,315},15,RGB(145,151,165),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    text(dc,L"medium: looks ahead",{670,285,930,315},15,RGB(145,151,165),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    text(dc,L"hard: deeper search",{170,405,430,435},15,RGB(145,151,165),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    text(dc,L"expert: strongest / slower",{670,405,930,435},15,RGB(145,151,165),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    button(dc,{390,560,710,615},L"BACK");
}

bool isSpecialTarget(int square) {
    for (auto& m:selectedMoves) if (m.to==square && (m.castle||m.enPassant||m.promotion)) return true;
    return false;
}

bool isCaptureTarget(int square) {
    for (auto& m:selectedMoves) if (m.to==square && (board[square]!='.'||m.enPassant)) return true;
    return false;
}

bool isMoveTarget(int square) {
    for (auto& m:selectedMoves) if (m.to==square) return true;
    return false;
}


void ellipseFill(HDC dc,int l,int t,int r,int b,COLORREF color) {
    HBRUSH brush=CreateSolidBrush(color);
    HPEN pen=CreatePen(PS_SOLID,1,color);
    HBRUSH oldBrush=(HBRUSH)SelectObject(dc,brush);
    HPEN oldPen=(HPEN)SelectObject(dc,pen);
    Ellipse(dc,l,t,r,b);
    SelectObject(dc,oldBrush);
    SelectObject(dc,oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void polygonFill(HDC dc,POINT* points,int count,COLORREF color) {
    HBRUSH brush=CreateSolidBrush(color);
    HPEN pen=CreatePen(PS_SOLID,1,color);
    HBRUSH oldBrush=(HBRUSH)SelectObject(dc,brush);
    HPEN oldPen=(HPEN)SelectObject(dc,pen);
    Polygon(dc,points,count);
    SelectObject(dc,oldBrush);
    SelectObject(dc,oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void roundedFill(HDC dc,int l,int t,int r,int b,int radius,COLORREF color) {
    HBRUSH brush=CreateSolidBrush(color);
    HPEN pen=CreatePen(PS_SOLID,1,color);
    HBRUSH oldBrush=(HBRUSH)SelectObject(dc,brush);
    HPEN oldPen=(HPEN)SelectObject(dc,pen);
    RoundRect(dc,l,t,r,b,radius,radius);
    SelectObject(dc,oldBrush);
    SelectObject(dc,oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void drawVectorPiece(HDC dc,char piece,RECT r) {
    bool white=isWhite(piece);
    char p=(char)std::tolower((unsigned char)piece);

    COLORREF main=white?RGB(245,241,229):RGB(27,30,36);
    COLORREF edge=white?RGB(177,169,151):RGB(5,7,10);
    COLORREF shine=white?RGB(255,255,250):RGB(58,63,72);

    int cx=(r.left+r.right)/2;
    int top=r.top+10;
    int bottom=r.bottom-8;
    int w=r.right-r.left;

    // small shadow makes pieces separate from both board colours
    ellipseFill(dc,cx-w/3,bottom-7,cx+w/3,bottom+1,RGB(35,37,42));

    HPEN outline=CreatePen(PS_SOLID,2,edge);
    HPEN oldPen=(HPEN)SelectObject(dc,outline);

    auto base=[&]() {
        roundedFill(dc,cx-24,bottom-13,cx+24,bottom-4,5,main);
        roundedFill(dc,cx-29,bottom-7,cx+29,bottom,5,main);
    };

    if (p=='p') {
        ellipseFill(dc,cx-12,top+4,cx+12,top+28,main);
        POINT body[]={{cx-9,top+27},{cx+9,top+27},{cx+18,bottom-14},{cx-18,bottom-14}};
        polygonFill(dc,body,4,main);
        base();
    }
    else if (p=='r') {
        RECT tower={cx-19,top+20,cx+19,bottom-13};
        fill(dc,tower,main);
        RECT crown={cx-23,top+9,cx+23,top+25};
        fill(dc,crown,main);
        RECT cut1={cx-14,top+8,cx-7,top+17};
        RECT cut2={cx-3,top+8,cx+4,top+17};
        RECT cut3={cx+13,top+8,cx+20,top+17};
        fill(dc,cut1,RGB(91,102,112)); fill(dc,cut2,RGB(91,102,112)); fill(dc,cut3,RGB(91,102,112));
        base();
    }
    else if (p=='n') {
        POINT horse[]={
            {cx-22,bottom-14},{cx-17,top+35},{cx-5,top+22},{cx-10,top+9},
            {cx+6,top+14},{cx+20,top+31},{cx+14,top+45},{cx+2,top+42},
            {cx+14,bottom-14}
        };
        polygonFill(dc,horse,9,main);
        ellipseFill(dc,cx+3,top+24,cx+7,top+28,shine);
        base();
    }
    else if (p=='b') {
        ellipseFill(dc,cx-15,top+7,cx+15,top+39,main);
        POINT body[]={{cx-9,top+34},{cx+9,top+34},{cx+20,bottom-14},{cx-20,bottom-14}};
        polygonFill(dc,body,4,main);
        HPEN slash=CreatePen(PS_SOLID,3,edge);
        HPEN prev=(HPEN)SelectObject(dc,slash);
        MoveToEx(dc,cx+7,top+13,nullptr); LineTo(dc,cx-5,top+30);
        SelectObject(dc,prev); DeleteObject(slash);
        base();
    }
    else if (p=='q') {
        POINT crown[]={
            {cx-24,top+25},{cx-19,top+6},{cx-8,top+20},{cx,top+3},
            {cx+8,top+20},{cx+19,top+6},{cx+24,top+25}
        };
        polygonFill(dc,crown,7,main);
        ellipseFill(dc,cx-23,top+3,cx-17,top+9,main);
        ellipseFill(dc,cx-3,top,cx+3,top+6,main);
        ellipseFill(dc,cx+17,top+3,cx+23,top+9,main);
        POINT body[]={{cx-20,top+24},{cx+20,top+24},{cx+18,bottom-14},{cx-18,bottom-14}};
        polygonFill(dc,body,4,main);
        base();
    }
    else if (p=='k') {
        // crown cross
        RECT vertical={cx-4,top+1,cx+4,top+23};
        RECT horizontal={cx-12,top+8,cx+12,top+15};
        fill(dc,vertical,main); fill(dc,horizontal,main);
        ellipseFill(dc,cx-16,top+20,cx+16,top+45,main);
        POINT body[]={{cx-12,top+39},{cx+12,top+39},{cx+20,bottom-14},{cx-20,bottom-14}};
        polygonFill(dc,body,4,main);
        base();
    }

    SelectObject(dc,oldPen);
    DeleteObject(outline);
}

void drawMoveMarker(HDC dc,RECT r,bool capture,bool special) {
    int cx=(r.left+r.right)/2;
    int cy=(r.top+r.bottom)/2;

    if (capture) {
        HPEN pen=CreatePen(PS_SOLID,5,RGB(199,83,83));
        HPEN old=(HPEN)SelectObject(dc,pen);
        HBRUSH oldBrush=(HBRUSH)SelectObject(dc,GetStockObject(NULL_BRUSH));
        Ellipse(dc,r.left+8,r.top+8,r.right-8,r.bottom-8);
        SelectObject(dc,oldBrush);
        SelectObject(dc,old);
        DeleteObject(pen);
    } else {
        COLORREF c=special?RGB(226,190,105):RGB(61,125,174);
        ellipseFill(dc,cx-8,cy-8,cx+8,cy+8,c);
    }
}

void drawGame(HDC dc) {
    RECT whole={0,0,1100,760}; fill(dc,whole,RGB(16,18,23));

    for (int row=0;row<8;row++) for (int col=0;col<8;col++) {
        int sq=row*8+col;
        RECT r={boardX+col*squareSize,boardY+row*squareSize,boardX+(col+1)*squareSize,boardY+(row+1)*squareSize};
        COLORREF c=((row+col)%2==0)?RGB(226,218,196):RGB(91,102,112);

        if (sq==selected) c=RGB(111,139,166);

        fill(dc,r,c);

        if (showHints && isMoveTarget(sq))
            drawMoveMarker(dc,r,isCaptureTarget(sq),isSpecialTarget(sq));

        if (board[sq]!='.')
            drawVectorPiece(dc,board[sq],r);
    }
    for (int i=0;i<8;i++) {
        wchar_t file[2]={wchar_t(L'a'+i),0};
        RECT fr={boardX+i*squareSize,boardY+8*squareSize+2,boardX+(i+1)*squareSize,boardY+8*squareSize+26};
        text(dc,file,fr,14,RGB(150,157,170),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        wchar_t rank[2]={wchar_t(L'8'-i),0};
        RECT rr={8,boardY+i*squareSize,30,boardY+(i+1)*squareSize};
        text(dc,rank,rr,14,RGB(150,157,170),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }

    std::wstring title=vsAI?L"PLAY VS AI":L"TWO PLAYER";
    text(dc,title.c_str(),{700,50,1050,95},30,RGB(226,190,105),DT_LEFT|DT_VCENTER|DT_SINGLELINE,FW_BOLD);

    std::wstring st(statusText.begin(),statusText.end());
    text(dc,st.c_str(),{700,105,1050,145},19,RGB(235,238,245));

    if (vsAI) {
        const wchar_t* diff=difficulty==1?L"Easy":difficulty==2?L"Medium":difficulty==3?L"Hard":L"Expert";
        std::wstring line=L"AI DIFFICULTY: "; line+=diff;
        text(dc,line.c_str(),{700,145,1050,180},16,RGB(150,157,170));
    }

    text(dc,L"MOVE HISTORY",{700,205,1050,240},18,RGB(235,238,245),DT_LEFT|DT_VCENTER|DT_SINGLELINE,FW_SEMIBOLD);
    int start=(std::max)(0,(int)moveHistory.size()-10);
    int y=242;
    for (int i=start;i<(int)moveHistory.size();i++) {
        std::wstring line=std::to_wstring(i+1)+L". "+std::wstring(moveHistory[i].begin(),moveHistory[i].end());
        text(dc,line.c_str(),{700,y,1050,y+26},15,RGB(155,162,176));
        y+=26;
    }

    fill(dc,{700,525,720,545},RGB(83,151,203));
    text(dc,L"legal move",{730,518,850,550},14,RGB(165,171,184));
    fill(dc,{860,525,880,545},RGB(184,70,70));
    text(dc,L"capture",{890,518,1010,550},14,RGB(165,171,184));
    fill(dc,{700,560,720,580},RGB(221,172,57));
    text(dc,L"special move",{730,553,850,585},14,RGB(165,171,184));

    button(dc,{700,610,855,660},L"NEW GAME");
    button(dc,{875,610,1030,660},L"RESIGN");
    button(dc,{700,675,1030,720},L"MAIN MENU");
}

void drawSettings(HDC dc) {
    RECT whole={0,0,1100,760}; fill(dc,whole,RGB(16,18,23));
    text(dc,L"SETTINGS",{0,80,1100,140},40,RGB(240,242,247),DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_BOLD);

    text(dc,L"AUDIO",{300,220,520,260},22,RGB(235,238,245));
    button(dc,{610,215,800,265},soundOn?L"ON":L"OFF",soundOn);

    text(dc,L"MOVE HINTS",{300,300,520,340},22,RGB(235,238,245));
    button(dc,{610,295,800,345},showHints?L"ON":L"OFF",showHints);

    text(dc,L"local system sounds only - no downloaded audio or online services",{300,385,800,420},15,RGB(135,142,156));
    button(dc,{390,540,710,595},L"BACK");
}

void drawAbout(HDC dc) {
    RECT whole={0,0,1100,760}; fill(dc,whole,RGB(16,18,23));
    text(dc,L"OpsecChess",{0,90,1100,145},42,RGB(226,190,105),DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_BOLD);
    text(dc,L"OFFLINE DESKTOP CHESS",{0,150,1100,190},18,RGB(155,162,176),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    text(dc,L"2 player local + AI  |  legal move validation  |  castling  |  en passant  |  promotion",{100,245,1000,285},17,RGB(220,223,230),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    text(dc,L"checkmate  |  stalemate  |  repetition  |  50 move rule  |  insufficient material",{100,290,1000,330},17,RGB(220,223,230),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    text(dc,L"no server connection, accounts, telemetry or cloud features",{100,365,1000,405},17,RGB(145,151,165),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    button(dc,{390,540,710,595},L"BACK");
}

void drawCredits(HDC dc) {
    RECT whole={0,0,1100,760}; fill(dc,whole,RGB(13,15,20));

    text(dc,L"\u265A",{0,70,1100,180},82,RGB(226,190,105),DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_NORMAL);
    text(dc,L"CREDITS",{0,175,1100,225},34,RGB(240,242,247),DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_BOLD);

    RECT card={300,270,800,440};
    fill(dc,card,RGB(27,30,37));
    FrameRect(dc,&card,(HBRUSH)GetStockObject(GRAY_BRUSH));

    text(dc,L"OpsecChess",{330,295,770,340},25,RGB(226,190,105),DT_CENTER|DT_VCENTER|DT_SINGLELINE,FW_BOLD);
    text(dc,L"Archie B. - Developer",{330,350,770,395},21,RGB(235,238,245),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    text(dc,L"C++17  |  Win32  |  fully offline",{330,395,770,425},15,RGB(135,142,156),DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    button(dc,{390,540,710,595},L"BACK");
}

void draw(HWND window,HDC dc) {
    if (screen==Screen::Loading) drawLoading(dc);
    else if (screen==Screen::Menu) drawMenu(dc);
    else if (screen==Screen::Difficulty) drawDifficulty(dc);
    else if (screen==Screen::Game) drawGame(dc);
    else if (screen==Screen::Settings) drawSettings(dc);
    else if (screen==Screen::About) drawAbout(dc);
    else drawCredits(dc);
}

void startGame(bool ai) {
    vsAI=ai;
    resetGame();
    screen=Screen::Game;
    playSound(SoundCue::GameStart);
}

void gameClick(HWND window,int x,int y) {
    if (hit(x,y,{700,610,855,660})) {
        resetGame();
        InvalidateRect(window,nullptr,FALSE);
        return;
    }

    if (hit(x,y,{875,610,1030,660}) && !gameOver) {
        gameOver=true;
        statusText=whiteTurn?"White resigned - Black wins":"Black resigned - White wins";
        playSound(SoundCue::GameEnd);
        InvalidateRect(window,nullptr,FALSE);
        return;
    }

    if (hit(x,y,{700,675,1030,720})) {
        screen=Screen::Menu;
        InvalidateRect(window,nullptr,FALSE);
        return;
    }

    if (gameOver || (vsAI && !whiteTurn)) return;
    if (x<boardX || y<boardY) return;

    int col=(x-boardX)/squareSize;
    int row=(y-boardY)/squareSize;
    if (!inside(row,col)) return;
    int sq=row*8+col;

    if (selected!=-1) {
        std::vector<Move> targets;
        for (auto& m:selectedMoves) if (m.to==sq) targets.push_back(m);

        if (!targets.empty()) {
            Move chosen=targets[0];

            // if theres multiple promotion options use queen by default for fast play
            for (auto& m:targets) if (std::tolower((unsigned char)m.promotion)=='q') chosen=m;

            doMove(chosen,true);
            InvalidateRect(window,nullptr,FALSE);

            if (vsAI && !gameOver) {
                aiMove();
                InvalidateRect(window,nullptr,FALSE);
            }
            return;
        }
    }

    char p=board[sq];
    if (p!='.' && isWhite(p)==whiteTurn) {
        selected=sq;
        selectedMoves.clear();
        for (auto& m:legalMoves(whiteTurn)) if (m.from==sq) selectedMoves.push_back(m);
        playUiSound(MB_OK);
    } else {
        selected=-1;
        selectedMoves.clear();
    }

    InvalidateRect(window,nullptr,FALSE);
}

void click(HWND window,int x,int y) {
    if (screen==Screen::Loading) return;
    if (screen==Screen::Menu) {
        if (hit(x,y,{72,315,420,374})) screen=Screen::Difficulty;
        else if (hit(x,y,{72,390,420,449})) startGame(false);
        else if (hit(x,y,{72,465,238,519})) screen=Screen::Settings;
        else if (hit(x,y,{254,465,420,519})) screen=Screen::Credits;
        else if (hit(x,y,{72,535,238,589})) screen=Screen::About;
        else if (hit(x,y,{254,535,420,589})) PostMessageW(window,WM_CLOSE,0,0);
    }
    else if (screen==Screen::Difficulty) {
        if (hit(x,y,{170,210,430,280})) { difficulty=1; startGame(true); }
        else if (hit(x,y,{670,210,930,280})) { difficulty=2; startGame(true); }
        else if (hit(x,y,{170,330,430,400})) { difficulty=3; startGame(true); }
        else if (hit(x,y,{670,330,930,400})) { difficulty=4; startGame(true); }
        else if (hit(x,y,{390,560,710,615})) screen=Screen::Menu;
    }
    else if (screen==Screen::Settings) {
        if (hit(x,y,{610,215,800,265})) { soundOn=!soundOn; playUiSound(MB_OK); }
        else if (hit(x,y,{610,295,800,345})) showHints=!showHints;
        else if (hit(x,y,{390,540,710,595})) screen=Screen::Menu;
    }
    else if (screen==Screen::About) {
        if (hit(x,y,{390,540,710,595})) screen=Screen::Menu;
    }
    else if (screen==Screen::Credits) {
        if (hit(x,y,{390,540,710,595})) screen=Screen::Menu;
    }
    else gameClick(window,x,y);

    InvalidateRect(window,nullptr,FALSE);
}

LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    switch(message) {
        case WM_MOUSEMOVE: {
            RECT client;
            GetClientRect(window,&client);
            int rawX=(short)LOWORD(lParam);
            int rawY=(short)HIWORD(lParam);
            hoverX = client.right ? rawX * designWidth / client.right : rawX;
            hoverY = client.bottom ? rawY * designHeight / client.bottom : rawY;
            InvalidateRect(window,nullptr,FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            RECT client;
            GetClientRect(window,&client);
            int rawX=(short)LOWORD(lParam);
            int rawY=(short)HIWORD(lParam);
            int x=client.right ? rawX * designWidth / client.right : rawX;
            int y=client.bottom ? rawY * designHeight / client.bottom : rawY;
            click(window,x,y);
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam==VK_ESCAPE && screen!=Screen::Loading) {
                if (screen==Screen::Menu) PostMessageW(window,WM_CLOSE,0,0);
                else {
                    screen=Screen::Menu;
                    InvalidateRect(window,nullptr,FALSE);
                }
                return 0;
            }
            break;

        case WM_TIMER:
            if (wParam==1) {
                DWORD elapsed=GetTickCount()-loadStarted;
                if (elapsed>=3500 && screen==Screen::Loading) {
                    screen=Screen::Menu;
                    KillTimer(window,1);
                }
                InvalidateRect(window,nullptr,FALSE);
                return 0;
            }
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc=BeginPaint(window,&ps);

            RECT client;
            GetClientRect(window,&client);

            HDC mem=CreateCompatibleDC(dc);
            HBITMAP bmp=CreateCompatibleBitmap(dc,client.right,client.bottom);
            HBITMAP old=(HBITMAP)SelectObject(mem,bmp);

            // Render straight into a full-resolution back buffer.
            // MM_ANISOTROPIC keeps the 1100x760 layout while GDI draws at native display resolution.
            SetMapMode(mem,MM_ANISOTROPIC);
            SetWindowExtEx(mem,designWidth,designHeight,nullptr);
            SetViewportExtEx(mem,client.right,client.bottom,nullptr);
            SetBrushOrgEx(mem,0,0,nullptr);

            draw(window,mem);

            // Reset mapping before copying the already full-resolution frame.
            SetMapMode(mem,MM_TEXT);
            BitBlt(dc,0,0,client.right,client.bottom,mem,0,0,SRCCOPY);

            SelectObject(mem,old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(window,&ps);
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(window,message,wParam,lParam);
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    resetGame();

    const wchar_t className[]=L"OpsecChessWindow";

    WNDCLASSW wc={};
    wc.lpfnWndProc=windowProc;
    wc.hInstance=instance;
    wc.lpszClassName=className;
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);

    if (!RegisterClassW(&wc)) return 0;

    int screenW=GetSystemMetrics(SM_CXSCREEN);
    int screenH=GetSystemMetrics(SM_CYSCREEN);

    HWND window=CreateWindowExW(
        WS_EX_APPWINDOW,className,L"OpsecChess",
        WS_POPUP,
        0,0,screenW,screenH,
        nullptr,nullptr,instance,nullptr
    );

    if (!window) return 0;

    SendMessageW(window,WM_SETICON,ICON_BIG,(LPARAM)LoadIconW(instance,MAKEINTRESOURCEW(101)));
    SendMessageW(window,WM_SETICON,ICON_SMALL,(LPARAM)LoadIconW(instance,MAKEINTRESOURCEW(101)));

    loadStarted=GetTickCount();
    SetTimer(window,1,16,nullptr);

    ShowWindow(window,SW_SHOW);
    UpdateWindow(window);

    MSG msg={};
    while (GetMessageW(&msg,nullptr,0,0)>0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
