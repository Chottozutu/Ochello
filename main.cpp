#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include <map>
#include <sstream>
#include <cctype>

const int CELL = 64;
const int ROWS = 8;
const int COLS = 8;
const int WINDOW_W = COLS * CELL;
const int WINDOW_H = ROWS * CELL;

enum PieceType { KING, QUEEN, ROOK, BISHOP, KNIGHT, PAWN };

struct Piece {
    bool isWhite;
    PieceType type;
    int row, col;
    SDL_Texture* texture;
    bool hasMoved = false;          
    bool enPassantVulnerable = false; 
};

// --- プロトタイプ ---
std::vector<std::pair<int,int>> getLegalMoves(Piece* board[ROWS][COLS], Piece* piece, std::pair<int,int> lastMove = {-1,-1});
SDL_Texture* loadTexture(SDL_Renderer* ren, const std::string& path);
std::string serializeBoard(Piece* board[ROWS][COLS], bool whiteTurn);
void flipOthello(Piece* board[ROWS][COLS], int row, int col,
                    std::map<std::string,SDL_Texture*>& textures,
                    bool& gameOver, bool& winnerIsWhite);

Mix_Chunk* moveSound = Mix_LoadWAV("./sound/move.mp3");
Mix_Chunk* captureSound = Mix_LoadWAV("./sound/capture.mp3");
Mix_Chunk* gameOverSound = Mix_LoadWAV("./sound/gameover.mp3");
Mix_Chunk* flipSound = Mix_LoadWAV("./sound/flip.mp3");

int main(int argc, char* argv[]) {
    bool gameOver = false;
    bool winnerIsWhite = false;
    int halfMoveClock = 0; 
    std::vector<std::string> boardHistory;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

    // --- BGM読み込み ---
    Mix_Music* bgm = Mix_LoadMUS("./sound/bgm.mp3");
    if(!bgm){
        std::cerr << "Failed to load BGM: " << Mix_GetError() << std::endl;
    }
    Mix_PlayMusic(bgm, -1); // -1 でループ再生

    SDL_Window* win = SDL_CreateWindow("Chess+Othello=Ochello",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    TTF_Font* font = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", 32);

    Piece* board[ROWS][COLS] = {};

    // --- 画像読み込み ---
    std::map<std::string, SDL_Texture*> textures;
    std::string base = "./img/";
    std::string colors[2] = { "WHITE", "BLACK" };
    std::string names[6] = { "KING","QUEEN","ROOK","BISHOP","KNIGHT","PAWN" };
    for(int c=0;c<2;++c)
        for(int t=0;t<6;++t){
            std::string path = base + colors[c] + "_" + names[t] + ".png";
            textures[colors[c]+"_"+names[t]] = loadTexture(ren,path);
        }

    // --- 効果音読み込み ---
    moveSound     = Mix_LoadWAV("./sound/move.mp3");
    captureSound  = Mix_LoadWAV("./sound/capture.mp3");
    gameOverSound = Mix_LoadWAV("./sound/gameover.mp3");
    flipSound     = Mix_LoadWAV("./sound/flip.mp3");
    if(!moveSound || !captureSound || !gameOverSound){
        std::cerr << "Failed to load sound: " << Mix_GetError() << std::endl;
    }

    // --- 駒の配置と対応付け ---
    auto place = [&](int r,int c,bool white,PieceType t){
        std::string color = white ? "WHITE" : "BLACK";
        std::string name;
        switch(t){ case KING:name="KING";break;
                    case QUEEN:name="QUEEN";break;
                    case ROOK:name="ROOK";break;
                    case BISHOP:name="BISHOP";break;
                    case KNIGHT:name="KNIGHT";break;
                    case PAWN:name="PAWN";break;
        }
        board[r][c] = new Piece{white,t,r,c,textures[color+"_"+name]};
    };

    // --- 初期配置 ---
    place(7,0,true,ROOK); place(7,7,true,ROOK);
    place(7,1,true,KNIGHT); place(7,6,true,KNIGHT);
    place(7,2,true,BISHOP); place(7,5,true,BISHOP);
    place(7,3,true,QUEEN); place(7,4,true,KING);
    for(int i=0;i<8;i++) place(6,i,true,PAWN);

    place(0,0,false,ROOK); place(0,7,false,ROOK);
    place(0,1,false,KNIGHT); place(0,6,false,KNIGHT);
    place(0,2,false,BISHOP); place(0,5,false,BISHOP);
    place(0,3,false,QUEEN); place(0,4,false,KING);
    for(int i=0;i<8;i++) place(1,i,false,PAWN);

    int selectedRow=-1,selectedCol=-1;
    std::vector<std::pair<int,int>> legalMoves;
    bool isWhiteTurn=true;
    auto turnStartTime = std::chrono::steady_clock::now();
    std::pair<int,int> lastMove = {-1,-1};

    bool title = true;
    bool tutorial = true;
    bool running = true;
    SDL_Event e;

    while(title){
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT){
                Mix_PlayChannel(-1, moveSound, 0);
                title = false;
            }
            SDL_RenderClear(ren);
            SDL_Surface* s = IMG_Load("./img/title.png");
            SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
            SDL_FreeSurface(s);
            SDL_RenderCopy(ren, t, nullptr, nullptr);
            SDL_RenderPresent(ren);
            SDL_DestroyTexture(t);
        }
    }

    while(tutorial){
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT){
                Mix_PlayChannel(-1, moveSound, 0);
                tutorial = false;
            }
            SDL_RenderClear(ren);
            SDL_Surface* s = IMG_Load("./img/tutorial.png");
            SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
            SDL_FreeSurface(s);
            SDL_RenderCopy(ren, t, nullptr, nullptr);
            SDL_RenderPresent(ren);
            SDL_DestroyTexture(t);
        }
    }

    while(running){
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT) running=false;
            else if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT){
                int col = e.button.x / CELL;
                int row = e.button.y / CELL;

                if(selectedRow==-1){
                    if(board[row][col] && board[row][col]->isWhite==isWhiteTurn){
                        selectedRow=row; selectedCol=col;
                        legalMoves=getLegalMoves(board,board[row][col],lastMove);
                    }
                } else {
                    bool moved=false;
                    Piece* backup = nullptr;
                    for(auto& mv:legalMoves){
                        if(mv.first==row && mv.second==col){
                            Piece* p = board[selectedRow][selectedCol];
                            backup = board[row][col];
                            
                            // --- アンパッサン処理 ---
                            if(p->type==PAWN && board[row][col]==nullptr && selectedCol!=col){
                                int capRow = p->isWhite ? row+1 : row-1;
                                delete board[capRow][col]; board[capRow][col]=nullptr;
                            }

                            // --- 移動実行 ---
                            board[row][col]=p; board[selectedRow][selectedCol]=nullptr;
                            int oldRow = p->row, oldCol = p->col;
                            p->row=row; p->col=col;

                            // --- 捕獲対象がキングならゲーム終了 ---
                            if(backup && backup->type == KING){
                                gameOver = true;
                                winnerIsWhite = p->isWhite;
                            }

                            // --- キャスリング ---
                            if(p->type==KING && abs(col-oldCol)==2){
                                if(col>oldCol){
                                    Piece* rook = board[row][7];
                                    board[row][5]=rook; board[row][7]=nullptr;
                                    rook->col=5; rook->hasMoved=true;
                                } else {
                                    Piece* rook = board[row][0];
                                    board[row][3]=rook; board[row][0]=nullptr;
                                    rook->col=3; rook->hasMoved=true;
                                }
                            }

                            // --- ポーンプロモーション ---
                            if(p->type==PAWN && (row==0 || row==7)){
                                delete p;
                                board[row][col]=new Piece{isWhiteTurn,QUEEN,row,col,textures[std::string(isWhiteTurn?"WHITE":"BLACK")+"_QUEEN"],true};
                                p = board[row][col];
                            }

                            if(backup || p->type==PAWN) halfMoveClock=0;
                            else halfMoveClock++;

                            delete backup;
                            moved=true;
                            lastMove={oldRow,oldCol};
                            p->hasMoved=true;

                            // --- オセロ反転 ---
                            flipOthello(board,row,col,textures, gameOver, winnerIsWhite);

                            break;
                        }
                    }

                    if(moved){
                        // --- 効果音再生 ---
                        if (gameOver) {
                            Mix_HaltMusic();
                            Mix_PlayChannel(-1, gameOverSound, 0);
                        } else if (backup) {
                            Mix_PlayChannel(-1, captureSound, 0);
                        } else {
                            Mix_PlayChannel(-1, moveSound, 0);
                        }

                        isWhiteTurn=!isWhiteTurn;
                        turnStartTime = std::chrono::steady_clock::now();
                        std::string serialized = serializeBoard(board,isWhiteTurn);
                        boardHistory.push_back(serialized);
                    }

                    selectedRow=selectedCol=-1;
                    legalMoves.clear();
                    if(!moved && board[row][col] && board[row][col]->isWhite==isWhiteTurn)
                        legalMoves=getLegalMoves(board,board[row][col],lastMove),
                        selectedRow=row,selectedCol=col;
                }
            }
        }

        // --- 背景描画 ---
        SDL_Color lightCell,darkCell;
        if(isWhiteTurn){ lightCell={200,255,200}; darkCell={100,200,100}; }
        else{ lightCell={80,120,80}; darkCell={40,80,40}; }

        for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++){
            bool light=(r+c)%2==0;
            SDL_SetRenderDrawColor(ren,
                light?lightCell.r:darkCell.r,
                light?lightCell.g:darkCell.g,
                light?lightCell.b:darkCell.b,
                255);
            SDL_Rect cell={c*CELL,r*CELL,CELL,CELL};
            SDL_RenderFillRect(ren,&cell);
        }

        // --- 移動可能マス ---
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
        for(auto& mv:legalMoves){
            SDL_SetRenderDrawColor(ren,0,200,255,120);
            SDL_Rect rect={mv.second*CELL,mv.first*CELL,CELL,CELL};
            SDL_RenderFillRect(ren,&rect);
        }
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);

        // --- 駒描画 ---
        for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++)
            if(board[r][c] && board[r][c]->texture){
                SDL_Rect rect={c*CELL,r*CELL,CELL,CELL};
                SDL_RenderCopy(ren,board[r][c]->texture,nullptr,&rect);
            }

        // --- 選択中マス ---
        if(selectedRow>=0 && selectedCol>=0){
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren,255,255,0,120);
            SDL_Rect rect={selectedCol*CELL,selectedRow*CELL,CELL,CELL};
            SDL_RenderFillRect(ren,&rect);
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        }

        // --- 勝者・ターン表示 ---
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now-turnStartTime).count();
        if(gameOver){
            SDL_Color winColor={255,50,50,255};
            const char* text = "Game Over!";
            SDL_Surface* surf=TTF_RenderText_Solid(font,text,winColor);
            SDL_Texture* tex=SDL_CreateTextureFromSurface(ren,surf);
            SDL_Rect rect={WINDOW_W/2-150,WINDOW_H/2-20,surf->w,surf->h};
            SDL_RenderCopy(ren,tex,nullptr,&rect);
            SDL_FreeSurface(surf); SDL_DestroyTexture(tex);
        } else if(elapsed<3){
            SDL_Color textColor=isWhiteTurn?SDL_Color{0,0,0,255}:SDL_Color{255,255,255,255};
            const char* turnText=isWhiteTurn?"White's Turn":"Black's Turn";
            SDL_Surface* surf=TTF_RenderText_Solid(font,turnText,textColor);
            SDL_Texture* tex=SDL_CreateTextureFromSurface(ren,surf);
            SDL_Rect rect={WINDOW_W/2-80,WINDOW_H/2-20,surf->w,surf->h};
            SDL_RenderCopy(ren,tex,nullptr,&rect);
            SDL_FreeSurface(surf); SDL_DestroyTexture(tex);
        }

        SDL_RenderPresent(ren);
    }

    // --- 後処理 ---
    for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++) delete board[r][c];
    for(auto& kv:textures) SDL_DestroyTexture(kv.second);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    IMG_Quit();
    TTF_Quit();
    Mix_FreeChunk(moveSound);
    Mix_FreeChunk(captureSound);
    Mix_FreeChunk(gameOverSound);
    Mix_FreeChunk(flipSound);
    Mix_FreeMusic(bgm);
    Mix_CloseAudio();
    SDL_Quit();
    return 0;
}

// --- 画像ロード ---
SDL_Texture* loadTexture(SDL_Renderer* ren,const std::string& path){
    SDL_Surface* surf = IMG_Load(path.c_str());
    if(!surf){ std::cerr<<"Failed to load "<<path<<": "<<IMG_GetError()<<"\n"; return nullptr;}
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren,surf);
    SDL_FreeSurface(surf);
    return tex;
}

// --- オセロ反転処理（キング挟みで勝利判定） ---
void flipOthello(Piece* board[ROWS][COLS], int row, int col,
                    std::map<std::string,SDL_Texture*>& textures,
                    bool& gameOver, bool& winnerIsWhite) {
    bool color = board[row][col]->isWhite;
    int dir[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
    bool flippedAny = false;  // ← 反転が起きたかどうか

    for(auto& d:dir){
        std::vector<Piece*> toFlip;
        int r = row + d[0], c = col + d[1];

        // 間の駒を一時的に格納
        while(r>=0 && r<ROWS && c>=0 && c<COLS && board[r][c]){
            if(board[r][c]->isWhite == color){
                // 同色の駒にぶつかった場合、間の駒をすべて反転
                if(!toFlip.empty()){
                    flippedAny = true;  // ← 反転あり
                    for(auto p: toFlip){
                        // KINGが反転した場合はゲーム終了
                        if(p->type == KING){
                            gameOver = true;
                            winnerIsWhite = color; // 移動した側の勝利
                            Mix_HaltMusic();
                            Mix_PlayChannel(-1, gameOverSound, 0);
                        }

                        p->isWhite = color;

                        // テクスチャ更新
                        std::string tname = (color ? "WHITE" : "BLACK");
                        switch(p->type){
                            case KING:   p->texture = textures[tname+"_KING"]; break;
                            case QUEEN:  p->texture = textures[tname+"_QUEEN"]; break;
                            case ROOK:   p->texture = textures[tname+"_ROOK"]; break;
                            case BISHOP: p->texture = textures[tname+"_BISHOP"]; break;
                            case KNIGHT: p->texture = textures[tname+"_KNIGHT"]; break;
                            case PAWN:   p->texture = textures[tname+"_PAWN"]; break;
                        }
                    }
                }
                break;
            } else {
                toFlip.push_back(board[r][c]);
            }
            r += d[0]; c += d[1];
        }
    }
    
    // 反転が発生した場合のみ効果音を鳴らす
    if (flippedAny && !gameOver) {
        Mix_PlayChannel(-1, flipSound, 0);
    }
}


// --- 駒の合法手 ---
std::vector<std::pair<int,int>> getLegalMoves(Piece* board[ROWS][COLS], Piece* p,std::pair<int,int> lastMove){
    std::vector<std::pair<int,int>> moves;
    int r=p->row, c=p->col;
    int dir[8][2]={{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
    auto add=[&](int rr,int cc){
        if(rr>=0&&rr<ROWS&&cc>=0&&cc<COLS){
            if(!board[rr][cc]||board[rr][cc]->isWhite!=p->isWhite)
                moves.push_back({rr,cc});
        }
    };
    switch(p->type){
        case KING:
            for(auto& d:dir) add(r+d[0],c+d[1]);
            if(!p->hasMoved){
                if(board[r][7] && board[r][7]->type==ROOK && !board[r][7]->hasMoved &&
                    !board[r][5] && !board[r][6]) moves.push_back({r,6});
                if(board[r][0] && board[r][0]->type==ROOK && !board[r][0]->hasMoved &&
                    !board[r][1] && !board[r][2] && !board[r][3]) moves.push_back({r,2});
            }
            break;
        case QUEEN:
            for (int i = 0; i < 8; ++i) {
                int dr = dir[i][0], dc = dir[i][1];
                int nr = r + dr, nc = c + dc;
                while (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS) {
                    if (!board[nr][nc]) moves.push_back({nr, nc});
                    else {
                        if (board[nr][nc]->isWhite != p->isWhite)
                            moves.push_back({nr, nc});
                        break;
                    }
                    nr += dr; nc += dc;
                }
            }
            break;

        case ROOK: {
            int rookDirs[4] = {1, 3, 4, 6};
            for (int i : rookDirs) {
                int dr = dir[i][0], dc = dir[i][1];
                int nr = r + dr, nc = c + dc;
                while (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS) {
                    if (!board[nr][nc]) moves.push_back({nr, nc});
                    else {
                        if (board[nr][nc]->isWhite != p->isWhite)
                            moves.push_back({nr, nc});
                        break;
                    }
                    nr += dr; nc += dc;
                }
            }
            break;
        }

        case BISHOP: {
            int bishopDirs[4] = {0, 2, 5, 7};
            for (int i : bishopDirs) {
                int dr = dir[i][0], dc = dir[i][1];
                int nr = r + dr, nc = c + dc;
                while (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS) {
                    if (!board[nr][nc]) moves.push_back({nr, nc});
                    else {
                        if (board[nr][nc]->isWhite != p->isWhite)
                            moves.push_back({nr, nc});
                        break;
                    }
                    nr += dr; nc += dc;
                }
            }
            break;
        }

        case KNIGHT:{
            int jmp[8][2]={{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
            for(auto& jj:jmp) add(r+jj[0],c+jj[1]);
            break;
        }
        case PAWN:{
            int dr=p->isWhite?-1:1;
            int nr=r+dr;
            if(nr>=0&&nr<ROWS && !board[nr][c]) moves.push_back({nr,c});
            if((p->isWhite&&r==6)||( !p->isWhite&&r==1)){
                if(!board[r+2*dr][c] && !board[r+dr][c]) moves.push_back({r+2*dr,c});
            }
            for(int dc=-1;dc<=1;dc+=2){
                int nc=c+dc;
                if(nc>=0&&nc<COLS){
                    if(board[nr][nc] && board[nr][nc]->isWhite!=p->isWhite)
                        moves.push_back({nr,nc});
                    // アンパッサン
                    int capRow = p->isWhite ? r : r;
                    if(lastMove.first==capRow+dr && lastMove.second==nc){
                        if(board[lastMove.first][lastMove.second] &&
                            board[lastMove.first][lastMove.second]->type==PAWN &&
                            board[lastMove.first][lastMove.second]->isWhite!=p->isWhite)
                            moves.push_back({nr,nc});
                    }
                }
            }
            break;
        }
    }
    return moves;
}

// --- 局面文字列化 ---
std::string serializeBoard(Piece* board[ROWS][COLS], bool whiteTurn){
    std::stringstream ss;
    for(int r=0;r<ROWS;r++){
        for(int c=0;c<COLS;c++){
            if(!board[r][c]) ss<<"0";
            else {
                char ch='0';
                switch(board[r][c]->type){
                    case KING: ch='K'; break;
                    case QUEEN: ch='Q'; break;
                    case ROOK: ch='R'; break;
                    case BISHOP: ch='B'; break;
                    case KNIGHT: ch='N'; break;
                    case PAWN: ch='P'; break;
                }
                if(!board[r][c]->isWhite) ch=std::tolower(ch);
                ss<<ch;
            }
        }
    }
    ss<<whiteTurn;
    return ss.str();
}
