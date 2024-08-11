#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "vec.h"
#include "fastrand.h"

void printb(unsigned long long v) {
    unsigned long long i, s = 1ll<<((sizeof(v)<<3)-1); // s = only most significant bit at 1
    for (i = s; i; i>>=1) printf("%d", v & i || 0 );
    printf("\n");
}

typedef struct {
    uint8_t from;
    uint8_t to;
} Move;

static const Move NULL_MOVE = {49, 50};
static const Move PASS_MOVE = {50, 51};

void Move_print(Move* m) {
    if (m->from == PASS_MOVE.from && m->to == PASS_MOVE.to) {
        printf("0000");
        return;
    }
    printf("%c%c%c%c", 'a' + m->from % 7, '1' + m->from / 7, 'a' + m->to % 7, '1' + m->to / 7);
}

typedef uint64_t BitBoard;

const BitBoard FULL = 0x1ffffffffffffll;
const BitBoard FILE_A = 0x40810204081ll;
const BitBoard FILE_B = 0x81020408102ll;
const BitBoard FILE_C = 0x102040810204ll;
const BitBoard FILE_D = 0x204081020408ll;
const BitBoard FILE_E = 0x408102040810ll;
const BitBoard FILE_F = 0x810204081020ll;
const BitBoard FILE_G = 0x1020408102040ll;

const BitBoard RANK_1 = 0x7fll;
const BitBoard RANK_2 = 0x3f80ll;
const BitBoard RANK_3 = 0x1fc000ll;
const BitBoard RANK_4 = 0xfe00000ll;
const BitBoard RANK_5 = 0x7f0000000ll;
const BitBoard RANK_6 = 0x3f800000000ll;
const BitBoard RANK_7 = 0x1fc0000000000ll;

const BitBoard FILES[7] = {FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G};
const BitBoard RANKS[7] = {RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7};

BitBoard BB_from_idx(int idx) {
    return 1ull << idx;
}

BitBoard BB_from_square(int x, int y) {
    return FILES[x] & RANKS[y];
}

BitBoard BB_singles(BitBoard b) {
    return
        ((b << 7) | (b >> 7) |
        // R              // RU           // RD
        (((b << 1) | (b << 8) | (b >> 6)) & ~FILE_A) |
        // L              // LU           // LD
        (((b >> 1) | (b << 6) | (b >> 8)) & ~FILE_G))
        & FULL;
}

BitBoard BB_doubles(BitBoard b) {
    return
        (
        // UU            // DD
        (b << 14) | (b >> 14) |
        // RUU             // RDD
        (((b << 15) | (b >> 13)) & ~FILE_A) |
        // LUU             // LDD
        (((b << 13) | (b >> 15)) & ~FILE_G) |
        // RR              // RRUU         // RRDD          // RRU          // RRD
        (((b << 2) | (b << 16) | (b >> 12) | (b << 9) | (b >> 5)) & ~(FILE_A | FILE_B)) |
        // LL              // LLUU         // LLDD          // LLU          // LLD
        (((b >> 2) | (b << 12) | (b >> 16) | (b << 5) | (b >> 9)) & ~(FILE_F | FILE_G))
        ) & FULL;
}

BitBoard BB_not(BitBoard b) {
    return ~b & FULL;
}

BitBoard BB_reach(BitBoard b) {
    return BB_singles(b) | BB_doubles(b);
}

typedef uint64_t BitBoardIter;

BitBoardIter BB_iter(BitBoard b) {
    return b;
}

int BB_next(BitBoardIter* iter) {
    if (*iter == 0) {
        return -1;
    }
    int idx = __builtin_ctzll(*iter);
    *iter &= *iter - 1;
    return idx;
}

typedef struct {
    BitBoard white;
    BitBoard black;
    BitBoard blocked;
    int turn;
    int half_turns;
} Pos;


Pos Pos_from_fen(char* fen) {
    BitBoard white = 0;
    BitBoard black = 0;
    BitBoard blocked = 0;
    // parse pos
    int x = 0;
    int y = 6;
    int i = 0;
    while (1) {
        char c = fen[i];
        if (c == ' ') {
            i++;
            break;
        }
        switch (c) {
            case 'x': {
                black |= BB_from_square(x, y);
                x += 1;
                break;
            }
            case 'o': {
                white |= BB_from_square(x, y);
                x += 1;
                break;
            }
            case '-': {
                blocked |= BB_from_square(x, y);
                x += 1;
                break;
            }
            case '/': {
                x = 0;
                y -= 1;
                break;
            }
            default: {
                if (c >= '0' && c <= '9') {
                    x += c - '0';
                    break;
                } else {
                    printf("Invalid character in FEN: %c\n", c);
                    exit(1);
                }
            }
        }
        i++;
    }
    // parse turn
    int turn;
    if (fen[i] == 'x') {
        turn = 1;
    } else if (fen[i] == 'o') {
        turn = 0;
    } else {
        printf("Invalid turn in FEN: %c\n", fen[i]);
        exit(1);
    }
    i += 2;
    // parse half turns
    int half_turns = 0;
    half_turns = atoi(fen + i);
    return (Pos) {
        .white = white,
        .black = black,
        .blocked = blocked,
        .turn = turn,
        .half_turns = half_turns
    };
}

BitBoard Pos_empty(Pos* p) {
    return BB_not(p->white | p->black | p->blocked);
}

BitBoard Pos_colored(Pos* p, int color) {
    if (color == 0) {
        return p->white;
    } else {
        return p->black;
    }
}

BitBoard Pos_both_sides(Pos* p) {
    return p->white | p->black;
}

bool Pos_game_over(Pos* p) {
    bool black_dead = p->black == 0;
    bool white_dead = p->white == 0;
    bool too_many_half_turns = p->half_turns >= 100;
    BitBoard both_sides = Pos_both_sides(p);
    bool no_moves = (BB_reach(both_sides) & Pos_empty(p)) == 0;
    return black_dead || white_dead || too_many_half_turns || no_moves;
}

bool Pos_must_pass(Pos* p) {
    if (Pos_game_over(p)) {
        return false;
    }

    BitBoard own_sq = Pos_colored(p, p->turn);
    BitBoard empty = Pos_empty(p);
    return (BB_reach(own_sq) & empty) == 0;
}

int Pos_winner(Pos *p) {
    if (!Pos_game_over(p)) {
        // game not over
        return -1;
    }
    if (p->white == 0) {
        // black wins
        return 1;
    } else if (p->black == 0) {
        // white wins
        return 0;
    }
    int white_count = __builtin_popcountll(p->white);
    int black_count = __builtin_popcountll(p->black);
    if (white_count > black_count) {
        // white wins
        return 0;
    } else if (black_count > white_count) {
        // black wins
        return 1;
    } else {
        // draw
        return 2;
    }
}

Vec* Pos_gen_moves(Pos* p) {
    Vec moves = Vec_with_capacity(sizeof(Move), 50);
    if (Pos_must_pass(p)) {
        Move m = PASS_MOVE;
        Vec_push(&moves, &m);
        Vec* ret = malloc(sizeof(Vec));
        *ret = moves;
        return ret;
    }
    BitBoard own_sq = Pos_colored(p, p->turn);
    BitBoard empty = Pos_empty(p);

    BitBoard singles = BB_singles(own_sq) & empty;
    BitBoardIter singles_iter = BB_iter(singles);
    while (1) {
        int idx = BB_next(&singles_iter);
        if (idx == -1) {
            break;
        }
        Move m = {idx, idx};
        Vec_push(&moves, &m);
    }
    BitBoardIter own_iter = BB_iter(own_sq);
    while (1) {
        int from = BB_next(&own_iter);
        if (from == -1) {
            break;
        }
        BitBoard doubles = BB_doubles(BB_from_idx(from)) & empty;
        BitBoardIter doubles_iter = BB_iter(doubles);
        while (1) {
            int to = BB_next(&doubles_iter);
            if (to == -1) {
                break;
            }
            Move m = {from, to};
            Vec_push(&moves, &m);
        }
    }
    Vec* ret = malloc(sizeof(Vec));
    *ret = moves;
    return ret;
}

void Pos_new_turn(Pos* p) {
    p->turn += 1;
    p->turn %= 2;
    p->half_turns += 1;
}

void Pos_make_move(Pos* p, Move m) {
    if (m.from == PASS_MOVE.from && m.to == PASS_MOVE.to) {
        Pos_new_turn(p);
        return;
    }
    BitBoard from = BB_from_idx(m.from);
    BitBoard to = BB_from_idx(m.to);

    BitBoard* own_sq;
    BitBoard* opp_sq;
    if (p->turn == 0) {
        own_sq = &p->white;
        opp_sq = &p->black;
    } else {
        own_sq = &p->black;
        opp_sq = &p->white;
    }

    *own_sq ^= from | to;

    BitBoard captured = BB_singles(to) & *opp_sq;
    *opp_sq ^= captured;
    *own_sq |= captured;

    Pos_new_turn(p);
}

void Pos_print(Pos* p) {
    int i = 42;
    while (1) {
        BitBoard sq = BB_from_idx(i);

        if ((p->black & sq) != 0) {
            printf("x");
        } else if ((p->white & sq) != 0) {
            printf("o");
        } else if ((p->blocked & sq) != 0) {
            printf("#");
        } else {
            printf(".");
        }

        if (i == 6) {
            break;
        } else if (i % 7 == 6) {
            printf("\n");
            i -= 13;
            continue;
        }
        i++;

    }
}


typedef struct {
    int idx;
    int parent_node;
    Vec children;
    unsigned int visits;
    float value;
    Pos pos;
    Move move;
} Node;

Node Node_new(int idx, int parent_node, Pos* pos, Move move) {
    Node n = {
        .idx = idx,
        .parent_node = parent_node,
        .children = Vec_new(sizeof(int)),
        .visits = 0,
        .value = 0,
        .pos = *pos,
        .move = move
    };
    return n;
}

bool Node_is_expanded(Node* n) {
    return n->children.length > 0;
}

bool Node_is_expandable(Node* n) {
    Vec* moves = Pos_gen_moves(&n->pos);
    int move_len = moves->length;
    Vec_free(moves);
    return n->children.length < move_len;
}

bool Node_is_terminal(Node* n) {
    return !Node_is_expandable(n) && !Node_is_expanded(n);
}

void Node_print(Node* n) {
    printf("Node %d\n", n->idx);
    printf("Visits: %d\n", n->visits);
    printf("Value: %f\n", n->value);
    printf("Move: ");
    Move_print(&n->move);
    printf("\n");
    printf("Children: %d\n", n->children.length);
    printf("\n");
}

bool is_debug_node(Node* n) {
    return n->move.from == 0 && n->move.to == 2 && n->pos.black == 0x8 && n->pos.white == 0x1;
}

float Node_rollout(Node* n) {
    Pos pos = n->pos;
    while (!Pos_game_over(&pos)) {
        Vec* moves = Pos_gen_moves(&pos);
        int move_idx = fastrand() % moves->length;
        Move m = *(Move*)Vec_get(moves, move_idx);
        Vec_free(moves);
        Pos_make_move(&pos, m);
    }

    int winner = Pos_winner(&pos);
    if (winner == 0) {
        return 1.0;
    } else if (winner == 2) {
        return 0.2;
    } else {
        return 0.0;
    }
}

float Node_mean_rollout(Node* n, int n_rollouts) {
    float total = 0.0;
    for (int i = 0; i < n_rollouts; i++) {
        total += Node_rollout(n);
    }
    if (n->pos.turn == 0) {
        return total / (float)n_rollouts;
    }
    return 1.0 - (total / (float)n_rollouts);
}

float Node_default_policy(Node* n) {
    return 1.0 - Node_mean_rollout(n, 10);
}

typedef struct {
    Vec nodes;
} Tree;

Tree Tree_new() {
    Tree t;
    t.nodes = Vec_new(sizeof(Node));
    return t;
}

void Tree_free(Tree* t) {
    for (int i = 0; i < t->nodes.length; i++) {
        Node* n = Vec_get(&t->nodes, i);
        Vec_free_data(&n->children);
    }
    Vec_free_data(&t->nodes);
}

int expand(Node* node, Tree* tree) {
    int idx = tree->nodes.length;
    Vec* moves = Pos_gen_moves(&node->pos);
    Move m = *(Move*)Vec_get(moves, node->children.length);
    Vec_free(moves);
    Pos pos = node->pos;
    Pos_make_move(&pos, m);
    Node new_node = Node_new(idx, node->idx, &pos, m);
    Vec_push(&node->children, &idx);
    Vec_push(&tree->nodes, &new_node);
    return idx;
}

const float INF = 10000000.0;

float ucb1(Node* node, Tree* tree) {
    if (node->visits == 0) {
        return INF;
    }
    const float C = sqrt(2.0);
    float exploitation = node->value / (float)node->visits;
    Node* parent = Vec_get(&tree->nodes, node->parent_node);
    float exploration = C * sqrt((2.0 * log((float)parent->visits)) / (float)node->visits);
    return exploitation + exploration;
}

int best_child(Node* node, Tree* tree) {
    float best = -INF;
    int best_idx = -1;
    for (int i = 0; i < node->children.length; i++) {
        int idx = *(int*)Vec_get(&node->children, i);
        Node* child = Vec_get(&tree->nodes, idx);
        float child_value = ucb1(child, tree);

        if (child_value > best) {
            best = child_value;
            best_idx = idx;
        }
    }
    return best_idx;
}

int tree_policy(Tree* tree, Node* node) {
    int idx = node->idx;
    while (!Node_is_terminal(node)) {
        if (is_debug_node(node)) {
            printf("debug node\n");
        }
        if (Node_is_expandable(node)) {
            idx = expand(node, tree);
            break;
        } else {
            idx = best_child(node, tree);
            node = Vec_get(&tree->nodes, idx);
        }
    }
    return idx;
}

void backpropagate(Tree* tree, int node_idx, float value) {
    Node* node = Vec_get(&tree->nodes, node_idx);
    while (1) {
        node->visits += 1;
        node->value += value;
        value = 1.0 - value;
        node_idx = node->parent_node;
        node = Vec_get(&tree->nodes, node_idx);
        if (node_idx == 0) {
            node->visits += 1;
            node->value += value;
            break;
        }
    }
}

Move best_move(Tree* tree) {
    Node* root = Vec_get(&tree->nodes, 0);
    float best = -INF;
    Move best_move = NULL_MOVE;

    for (int i = 0; i < root->children.length; i++) {
        int idx = *(int*)Vec_get(&root->children, i);
        Node* child = Vec_get(&tree->nodes, idx);
        float child_value = child->value / (float)child->visits;
        if (child_value > best) {
            best = child_value;
            best_move = child->move;
        }
    }
    return best_move;
}

Move uct(Tree* tree, Pos* pos) {
    Vec* moves = Pos_gen_moves(pos);
    if (moves->length == 0) {
        Move m = *(Move*)Vec_get(moves, 0);
        Vec_free(moves);
        return m;
    }

    Node root = Node_new(0, -1, pos, NULL_MOVE);

    Vec_push(&tree->nodes, &root);
    clock_t start = clock();
    while ((double)(clock() - start) / CLOCKS_PER_SEC < 5.0) {
        Node* r = Vec_get(&tree->nodes, 0);
        int selection = tree_policy(tree, r);
        Node* selected = Vec_get(&tree->nodes, selection);
        float value = Node_default_policy(selected);
        backpropagate(tree, selection, value);
    }
    Vec_free(moves);
    return best_move(tree);
}


void uai_handler() {
    Pos p;
    while (1) {
        char line[512];
        fgets(line, 512, stdin);
        line[strcspn(line, "\n")] = 0; 
        char* tok = strtok(line, " ");
        if (strcmp(tok, "uai") == 0) {
            printf("id name Rag\n");
            printf("id author BlueKossa\n");
            printf("uaiok\n");
            fflush(stdout);
        } else if (strcmp(tok, "isready") == 0) {
            printf("readyok\n");
            fflush(stdout);
        } else if (strcmp(tok, "position") == 0) {
            tok = strtok(NULL, " ");
            if (strcmp(tok, "fen") == 0) {
                char* fen = line + strlen("position fen ");
                p = Pos_from_fen(fen);
            } else {
                printf("Unknown position command: %s\n", tok);
                exit(1);
            }
            fflush(stdout);
        } else if (strcmp(tok, "go") == 0) {
            Tree t = Tree_new();
            Move m = uct(&t, &p);
            Tree_free(&t);
            printf("bestmove ");
            Move_print(&m);
            printf("\n");
            fflush(stdout);
        } else {
            printf("Unknown command: %s\n", tok);
            exit(1);
        }
    }
}

int main() {
    uai_handler();
    return 0;
}