// Written by: Andrew Carling
// Date: 2025-05-24 
// Course: C Programming
// Purpose: This program implements a simple Battleship game in C.

//-----------------------------------------------------------------------------
// I. INCLUDES AND DEFINITIONS
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>   // For srand, rand, time, localtime, strftime
#include <ctype.h>  // For toupper, isalpha, isdigit, islower, isupper
#include <stdbool.h> // For bool type

#define GRID_SIZE 10
#define MAX_SHIPS 5
#define MAX_SHIP_NAME_LEN 50
#define MAX_PLAYER_NAME_LEN 4 // 3 chars + null terminator
#define DATETIME_STR_LEN 20   // For "YYYY-MM-DD HH:MM"
#define SAVE_FILE_NAME "battleship_save_game.dat"
#define SCORE_FILE_NAME "topTenScores.txt"

// Cell States for Grids
#define EMPTY_CELL '~'
#define MISS_CELL 'M'
#define HIT_CELL 'H'
// Ship letters (S, A, V, E, D) will also be used.

// Shot Processing Results
typedef enum {
    SHOT_MISS,
    SHOT_HIT,
    SHOT_SUNK,
    SHOT_ALREADY_PROCESSED, // Hit a spot that was already a hit or part of a sunk ship
    SHOT_ERROR
} ShotProcessResult;

// Coordinate Parsing Results
typedef enum {
    PARSE_OK,
    PARSE_ERROR_FORMAT,      // General format issue
    PARSE_ERROR_COL_RANGE,   // Column char out of A-J range
    PARSE_ERROR_ROW_NAN,     // Row part is not a number
    PARSE_ERROR_ROW_RANGE    // Row number out of 1-10 range
} ShotParseError;


//-----------------------------------------------------------------------------
// II. DATA STRUCTURES
//-----------------------------------------------------------------------------

typedef struct {
    int row;
    int col;
} Coordinate;

typedef struct {
    char name_long[MAX_SHIP_NAME_LEN];
    char letter;
    int size;
} ShipTypeInfo;

const ShipTypeInfo SHIP_TYPES[MAX_SHIPS] = {
    {"Seminole State Ship", 'S', 3},
    {"Air Force Academy",   'A', 5},
    {"Valencia Destroyer",  'V', 4},
    {"Eskimo University",   'E', 3},
    {"Deland High School",  'D', 2}
};

typedef struct {
    char name_long[MAX_SHIP_NAME_LEN];
    char letter;
    int size;
    int hits_taken;
    bool is_sunk;
    Coordinate segments[5]; // Max ship size is 5
} Ship;

typedef struct {
    char computer_ocean_grid[GRID_SIZE][GRID_SIZE]; // Stores ship letters (uppercase if intact, lowercase if hit)
    char player_target_grid[GRID_SIZE][GRID_SIZE];  // Stores EMPTY_CELL, MISS_CELL, HIT_CELL, or sunk ship letter
    Ship computer_fleet[MAX_SHIPS];
    int missiles_fired_count;
    int ships_remaining_count;
    bool game_in_progress;
    Coordinate last_shot_coord;
    bool last_shot_valid; // To know if last_shot_coord is meaningful for highlighting
} GameState;

typedef struct {
    char player_name[MAX_PLAYER_NAME_LEN];
    int score_value;
    char date_time_achieved[DATETIME_STR_LEN];
} ScoreEntry;

//-----------------------------------------------------------------------------
// III. FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Menu Functions
void displayMainMenu();
int getMenuChoice();
void displayHelpScreen();

// Game Setup Functions
void initializeNewGame(GameState *game);
void setupComputerShips(GameState *game);
bool isValidShipPlacement(const char grid[GRID_SIZE][GRID_SIZE], const ShipTypeInfo* ship_type, int r, int c, int orientation);

// Gameplay Loop Function
void playGame(GameState *game);

// Gameplay Helper Functions
void displayPlayerTargetGrid(const char grid[GRID_SIZE][GRID_SIZE], Coordinate last_shot, bool highlight_last_shot);
void displayShipStatusAndStats(const GameState *game);
void displayComputerOceanGrid_Revealed(const char grid[GRID_SIZE][GRID_SIZE]);
void getPlayerShotInput(char* buffer, int buffer_size, const char* prompt);
ShotParseError parseShotCoordinates(const char* shot_str, int* r, int* c);
ShotProcessResult processPlayerShot(GameState *game, int r_shot, int c_shot);
void updateTargetGridForSunkShip(GameState *game, const Ship* sunk_ship);
char numberToLetter(int num);
int letterToNumber(char val);

// Game State Persistence Functions
bool saveGameState(const GameState *game);
bool loadGameState(GameState *game);

// Scoring Functions
void viewTopScores();
void updateTopScores(int new_score_value);
int readScoresFromFile(ScoreEntry scores_array[], int max_scores);
void writeScoresToFile(const ScoreEntry scores_array[], int count);
void sortScores(ScoreEntry scores_array[], int count);
void getCurrentDateTimeString(char* buffer, int buffer_size);

// Utility Functions
void clearScreen();
void pauseForKey(const char* message);
void safeGets(char *buffer, int size);

//-----------------------------------------------------------------------------
// IV. MAIN FUNCTION
//-----------------------------------------------------------------------------
int main() {
    GameState current_game;
    current_game.game_in_progress = false;
    current_game.last_shot_valid = false;
    bool running = true;
    int choice;

    srand(time(NULL)); // Seed random number generator once

    printf("Welcome to Battleship!\n");
    pauseForKey("Press Enter to continue to the Main Menu...");

    while (running) {
        displayMainMenu();
        choice = getMenuChoice();

        switch (choice) {
            case 1: // Start New Game
                initializeNewGame(&current_game);
                playGame(&current_game);
                break;
            case 2: // Resume Game
                if (loadGameState(&current_game)) {
                    printf("Game resumed.\n");
                    pauseForKey("Press Enter to start playing...");
                    playGame(&current_game);
                } else {
                    printf("No saved game found or error loading.\n");
                    pauseForKey("Press Enter to start a new game instead...");
                    initializeNewGame(&current_game);
                    playGame(&current_game);
                }
                break;
            case 3: // View Top 10 Scores
                viewTopScores();
                break;
            case 4: // How to Play
                displayHelpScreen();
                break;
            case 5: // Quit
                if (current_game.game_in_progress) {
                    char save_prompt[10];
                    printf("A game is currently in progress.\n");
                    printf("Save current game before quitting? (Y/N): ");
                    safeGets(save_prompt, sizeof(save_prompt));
                    if (toupper(save_prompt[0]) == 'Y') {
                        if (saveGameState(&current_game)) {
                            printf("Game saved.\n");
                        } else {
                            printf("Error saving game.\n");
                        }
                    }
                }
                running = false;
                printf("Exiting game. Goodbye!\n");
                break;
            default:
                printf("Invalid choice. Please try again.\n");
                pauseForKey(NULL); 
                break;
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------
// V. MENU FUNCTIONS
//-----------------------------------------------------------------------------
void displayMainMenu() {
    clearScreen();
    printf("=======================================\n");
    printf("    B A T T L E S H I P    \n");
    printf("=======================================\n\n");
    printf("MAIN MENU\n");
    printf("---------------------------------------\n");
    printf("1. Start New Game\n");
    printf("2. Resume Game\n");
    printf("3. View Top 10 Scores\n");
    printf("4. How to Play\n");
    printf("5. Quit Game\n");
    printf("---------------------------------------\n");
}

int getMenuChoice() {
    char input[10];
    int choice = 0;
    printf("Enter your choice (1-5): ");
    safeGets(input, sizeof(input));
    if (sscanf(input, "%d", &choice) == 1 && choice >= 1 && choice <= 5) {
        return choice;
    }
    return 0; // Invalid choice
}

void displayHelpScreen() {
    clearScreen();
    printf("-----------------------------------------------------------------\n");
    printf("                       HOW TO PLAY BATTLESHIP                    \n");
    printf("-----------------------------------------------------------------\n");
    printf("OBJECTIVE:\n");
    printf("  Be the first to sink all 5 of the computer's hidden ships.\n\n");
    printf("THE FLEET (Name, Letter on Grid when Sunk, Size):\n");
    for (int i = 0; i < MAX_SHIPS; ++i) {
        printf("  - %-20s (%c) - %d holes\n", SHIP_TYPES[i].name_long, SHIP_TYPES[i].letter, SHIP_TYPES[i].size);
    }
    printf("\nGAMEPLAY:\n");
    printf("  1. On your turn, call out a shot by entering coordinates (e.g., A5, J10).\n");
    printf("  2. The grid will update with the result of your shot:\n");
    printf("     '%c' : Empty water (already shot or initial state)\n", EMPTY_CELL);
    printf("     '%c' : Miss\n", MISS_CELL);
    printf("     '%c' : Hit on a ship (that is not yet sunk)\n", HIT_CELL);
    printf("     'S,A,V,E,D': Indicates a segment of that specific sunk ship.\n");
    printf("  3. A ship is sunk when all its segments have been hit.\n");
    printf("  4. The game ends when all 5 ships are sunk.\n\n");
    printf("SCORING:\n");
    printf("  Try to use the fewest missiles possible. A perfect game uses 17 missiles.\n");
    printf("  Your score (missiles fired) might make the Top 10 list!\n\n");
    printf("SAVING/LOADING:\n");
    printf("  You can save your game progress if you need to quit and resume later.\n");
    printf("-----------------------------------------------------------------\n");
    pauseForKey(NULL);
}


//-----------------------------------------------------------------------------
// VI. GAME SETUP FUNCTIONS
//-----------------------------------------------------------------------------
void initializeNewGame(GameState *game) {
    for (int r = 0; r < GRID_SIZE; ++r) {
        for (int c = 0; c < GRID_SIZE; ++c) {
            game->player_target_grid[r][c] = EMPTY_CELL; 
            game->computer_ocean_grid[r][c] = EMPTY_CELL; 
        }
    }

    for (int i = 0; i < MAX_SHIPS; ++i) {
        strcpy(game->computer_fleet[i].name_long, SHIP_TYPES[i].name_long);
        game->computer_fleet[i].letter = SHIP_TYPES[i].letter;
        game->computer_fleet[i].size = SHIP_TYPES[i].size;
        game->computer_fleet[i].hits_taken = 0;
        game->computer_fleet[i].is_sunk = false;
    }

    game->missiles_fired_count = 0;
    game->ships_remaining_count = MAX_SHIPS;
    game->game_in_progress = true;
    game->last_shot_valid = false;

    setupComputerShips(game);
    printf("New game initialized. The computer has secretly placed its ships.\n");
    pauseForKey("Press Enter to begin...");
}

void setupComputerShips(GameState *game) {
    for (int i = 0; i < MAX_SHIPS; ++i) {
        const ShipTypeInfo* current_ship_type = &SHIP_TYPES[i];
        bool placed_successfully = false;
        int attempts = 0;

        while (!placed_successfully && attempts < 1000) { 
            int start_row = rand() % GRID_SIZE;
            int start_col = rand() % GRID_SIZE;
            int orientation = rand() % 2; 

            if (isValidShipPlacement(game->computer_ocean_grid, current_ship_type, start_row, start_col, orientation)) {
                for (int j = 0; j < current_ship_type->size; ++j) {
                    int r = start_row;
                    int c = start_col;
                    if (orientation == 0) { 
                        c += j;
                        game->computer_ocean_grid[r][c] = current_ship_type->letter; 
                        game->computer_fleet[i].segments[j].row = r;
                        game->computer_fleet[i].segments[j].col = c;
                    } else { 
                        r += j;
                        game->computer_ocean_grid[r][c] = current_ship_type->letter; 
                        game->computer_fleet[i].segments[j].row = r;
                        game->computer_fleet[i].segments[j].col = c;
                    }
                }
                placed_successfully = true;
            }
            attempts++;
        }
        if (!placed_successfully) {
            fprintf(stderr, "Warning: Could not place ship %s optimally after %d attempts. Game might be unplayable.\n", current_ship_type->name_long, attempts);
        }
    }
}

bool isValidShipPlacement(const char grid[GRID_SIZE][GRID_SIZE], const ShipTypeInfo* ship_type, int r_start, int c_start, int orientation) {
    if (orientation == 0) { 
        if (c_start < 0 || r_start < 0 || c_start + ship_type->size > GRID_SIZE || r_start >= GRID_SIZE) return false;
    } else { 
        if (r_start < 0 || c_start < 0 || r_start + ship_type->size > GRID_SIZE || c_start >= GRID_SIZE) return false;
    }

    for (int i = 0; i < ship_type->size; ++i) {
        int r = r_start;
        int c = c_start;
        if (orientation == 0) c += i; else r += i;
        if (grid[r][c] != EMPTY_CELL) return false; 
    }
    return true;
}

//-----------------------------------------------------------------------------
// VII. GAMEPLAY LOOP FUNCTION
//-----------------------------------------------------------------------------
void playGame(GameState *game) {
    char shot_input_str[10];
    int shot_row, shot_col;
    ShotParseError parse_err;

    while (game->ships_remaining_count > 0 && game->game_in_progress) {
        clearScreen();
        displayPlayerTargetGrid(game->player_target_grid, game->last_shot_coord, game->last_shot_valid);
        displayShipStatusAndStats(game);

        printf("Enter 'quit' to return to main menu.\n");
        getPlayerShotInput(shot_input_str, sizeof(shot_input_str), "Your command (e.g., A5 or quit): ");

        if (strcmp(shot_input_str, "quit") == 0 || strcmp(shot_input_str, "QUIT") == 0) {
            char save_prompt[10];
            printf("Are you sure you want to quit this game session?\n");
            printf("Save current game before returning to menu? (Y/N): ");
            safeGets(save_prompt, sizeof(save_prompt));
            if (toupper(save_prompt[0]) == 'Y') {
                 if(saveGameState(game)) printf("Game saved.\n"); else printf("Error saving game.\n");
            }
            game->game_in_progress = false; 
            pauseForKey("Returning to Main Menu...");
            return;
        }

        parse_err = parseShotCoordinates(shot_input_str, &shot_row, &shot_col);
        game->last_shot_valid = false; 

        if (parse_err != PARSE_OK) {
            switch (parse_err) {
                case PARSE_ERROR_FORMAT: printf("Error: Invalid coordinate format. Use LetterNumber (e.g., A5, J10).\n"); break;
                case PARSE_ERROR_COL_RANGE: printf("Error: Column out of range. Must be A-J.\n"); break;
                case PARSE_ERROR_ROW_NAN: printf("Error: Row must be a number.\n"); break;
                case PARSE_ERROR_ROW_RANGE: printf("Error: Row out of range. Must be 1-10.\n"); break;
                default: printf("Error: Unknown coordinate parsing error.\n"); break;
            }
            pauseForKey(NULL);
            continue;
        }
        
        game->last_shot_coord.row = shot_row; 
        game->last_shot_coord.col = shot_col;
        game->last_shot_valid = true;

        if (game->player_target_grid[shot_row][shot_col] != EMPTY_CELL &&
            game->player_target_grid[shot_row][shot_col] != HIT_CELL) { 
            printf("You've already conclusively fired at %s (%c). Try a different spot.\n", shot_input_str, game->player_target_grid[shot_row][shot_col]);
            pauseForKey(NULL);
            continue;
        }

        game->missiles_fired_count++;
        ShotProcessResult result = processPlayerShot(game, shot_row, shot_col);
        char result_message[100] = "";

        switch (result) {
            case SHOT_MISS:
                sprintf(result_message, "***** M I S S *****");
                game->player_target_grid[shot_row][shot_col] = MISS_CELL;
                break;
            case SHOT_HIT:
                sprintf(result_message, "***** H I T ! *****");
                game->player_target_grid[shot_row][shot_col] = HIT_CELL;
                break;
            case SHOT_SUNK:
                for (int i = 0; i < MAX_SHIPS; ++i) { 
                    if (game->computer_fleet[i].is_sunk) {
                        bool this_ship_hit = false;
                        for(int k=0; k < game->computer_fleet[i].size; ++k) {
                            if(game->computer_fleet[i].segments[k].row == shot_row && game->computer_fleet[i].segments[k].col == shot_col) {
                                this_ship_hit = true; break;
                            }
                        }
                        if(this_ship_hit) {
                            sprintf(result_message, "***** YOU SUNK THE %s! (%c) *****", game->computer_fleet[i].name_long, game->computer_fleet[i].letter);
                            updateTargetGridForSunkShip(game, &game->computer_fleet[i]);
                            break; 
                        }
                    }
                }
                break;
            case SHOT_ALREADY_PROCESSED:
                sprintf(result_message, "You already hit that spot. It's part of a ship (%c).", game->player_target_grid[shot_row][shot_col]);
                break;
            case SHOT_ERROR:
                sprintf(result_message, "Error processing shot. Please report this.");
                break;
        }
        printf("\n%s\n", result_message);
        pauseForKey("Press Enter for next turn or results...");
    } 

    if (game->ships_remaining_count == 0) {
        clearScreen();
        displayPlayerTargetGrid(game->player_target_grid, game->last_shot_coord, false); 
        displayShipStatusAndStats(game); 
        printf("\n====================================================\n");
        printf("    CONGRATULATIONS! You sunk all enemy ships!    \n");
        printf("====================================================\n");
        printf("Total missiles fired: %d\n", game->missiles_fired_count);
        if (game->missiles_fired_count == 17) { 
            printf("A PERFECT GAME! You used the minimum possible missiles!\n");
        }
        updateTopScores(game->missiles_fired_count);
        game->game_in_progress = false;

        char view_prompt[10];
        printf("\nWould you like to see the computer's ship placements? (Y/N): ");
        safeGets(view_prompt, sizeof(view_prompt));
        if (toupper(view_prompt[0]) == 'Y') {
            displayComputerOceanGrid_Revealed(game->computer_ocean_grid);
        }
    }
    pauseForKey("Press Enter to return to the Main Menu...");
}

//-----------------------------------------------------------------------------
// VIII. GAMEPLAY HELPER FUNCTIONS
//-----------------------------------------------------------------------------
void displayPlayerTargetGrid(const char grid[GRID_SIZE][GRID_SIZE], Coordinate last_shot, bool highlight_last_shot) {
    printf("\nYOUR TARGET GRID:\n");
    printf("  |"); 
    for (int c = 0; c < GRID_SIZE; ++c) {
        printf(" %c |", numberToLetter(c));
    }
    printf("\n");
    printf("  +");
    for (int c = 0; c < GRID_SIZE; ++c) printf("---+"); printf("\n");

    for (int r = 0; r < GRID_SIZE; ++r) {
        printf("%2d|", r + 1); 
        for (int c = 0; c < GRID_SIZE; ++c) {
            char display_char = grid[r][c];
            bool is_last_shot = highlight_last_shot && r == last_shot.row && c == last_shot.col;
            
            if (is_last_shot) printf("[%c]", display_char);
            else printf(" %c ", display_char);
            printf("|");
        }
        printf("\n");
        printf("  +");
        for (int c = 0; c < GRID_SIZE; ++c) printf("---+"); printf("\n");
    }
    printf("---------------------------------------\n");
}

void displayShipStatusAndStats(const GameState *game) {
    printf("\nGAME STATUS:\n");
    printf("---------------------------------------\n");
    printf("Missiles Fired: %d\n", game->missiles_fired_count);
    printf("Enemy Fleet Status:\n");
    for (int i = 0; i < MAX_SHIPS; ++i) {
        const Ship* ship = &game->computer_fleet[i];
        char status_str[30];
        if (ship->is_sunk) {
            sprintf(status_str, "SUNK");
        } else if (ship->hits_taken > 0) {
            sprintf(status_str, "HIT (%d/%d)", ship->hits_taken, ship->size);
        } else {
            sprintf(status_str, "Undamaged");
        }
        printf("  (%c) %-20s : %s\n", ship->letter, ship->name_long, status_str);
    }
    printf("---------------------------------------\n");
}

void displayComputerOceanGrid_Revealed(const char grid[GRID_SIZE][GRID_SIZE]) {
    clearScreen();
    printf("\nCOMPUTER'S SECRET OCEAN GRID (Revealed):\n");
    printf("  |"); 
    for (int c = 0; c < GRID_SIZE; ++c) {
        printf(" %c |", numberToLetter(c));
    }
    printf("\n");
    printf("  +");
    for (int c = 0; c < GRID_SIZE; ++c) printf("---+"); printf("\n");

    for (int r = 0; r < GRID_SIZE; ++r) {
        printf("%2d|", r + 1);
        for (int c = 0; c < GRID_SIZE; ++c) {
            char cell_content = grid[r][c];
            printf(" %c ", cell_content); 
            printf("|");
        }
        printf("\n");
        printf("  +");
        for (int c = 0; c < GRID_SIZE; ++c) printf("---+"); printf("\n");
    }
    printf("---------------------------------------\n");
    pauseForKey("This was the computer's setup. Press Enter to continue...");
}


void getPlayerShotInput(char* buffer, int buffer_size, const char* prompt) {
    printf("%s", prompt);
    safeGets(buffer, buffer_size);
    if (strcmp(buffer, "quit") != 0 && strcmp(buffer, "QUIT") != 0) {
        for (int i = 0; buffer[i]; i++) {
            buffer[i] = toupper(buffer[i]);
        }
    }
}

ShotParseError parseShotCoordinates(const char* shot_str, int* r, int* c) {
    if (shot_str == NULL || strlen(shot_str) < 2 || strlen(shot_str) > 3) return PARSE_ERROR_FORMAT;

    char char_col_upper = toupper(shot_str[0]);
    if (!isalpha(char_col_upper)) return PARSE_ERROR_FORMAT; 
    if (char_col_upper < 'A' || char_col_upper > ('A' + GRID_SIZE - 1)) return PARSE_ERROR_COL_RANGE;
    *c = letterToNumber(char_col_upper);

    int parsed_row_val = 0;
    int num_parsed_items = 0;
    
    if (strlen(shot_str) == 2) { 
        if (!isdigit(shot_str[1])) return PARSE_ERROR_ROW_NAN;
        num_parsed_items = sscanf(shot_str + 1, "%1d", &parsed_row_val); 
    } else if (strlen(shot_str) == 3) { 
         if (!isdigit(shot_str[1]) || !isdigit(shot_str[2])) return PARSE_ERROR_ROW_NAN;
        num_parsed_items = sscanf(shot_str + 1, "%2d", &parsed_row_val); 
    } else {
        return PARSE_ERROR_FORMAT;
    }
    
    if (num_parsed_items != 1) return PARSE_ERROR_ROW_NAN;
    if (parsed_row_val < 1 || parsed_row_val > GRID_SIZE) return PARSE_ERROR_ROW_RANGE;
    *r = parsed_row_val - 1; 

    return PARSE_OK;
}

ShotProcessResult processPlayerShot(GameState *game, int r_shot, int c_shot) {
    if (game->player_target_grid[r_shot][c_shot] == MISS_CELL || 
        (isupper(game->player_target_grid[r_shot][c_shot]) && game->player_target_grid[r_shot][c_shot] != EMPTY_CELL && game->player_target_grid[r_shot][c_shot] != HIT_CELL) ) { 
        return SHOT_ALREADY_PROCESSED;
    }

    char target_on_computer_grid = game->computer_ocean_grid[r_shot][c_shot];

    if (target_on_computer_grid == EMPTY_CELL) {
        return SHOT_MISS;
    } else if (islower(target_on_computer_grid)) { 
        return SHOT_ALREADY_PROCESSED; 
    } else if (isupper(target_on_computer_grid)) { 
        char ship_hit_letter = target_on_computer_grid;
        int found_ship_index = -1;
        for (int i = 0; i < MAX_SHIPS; ++i) {
            if (game->computer_fleet[i].letter == ship_hit_letter) {
                found_ship_index = i;
                break;
            }
        }

        if (found_ship_index != -1) {
            game->computer_fleet[found_ship_index].hits_taken++;
            game->computer_ocean_grid[r_shot][c_shot] = tolower(ship_hit_letter); 

            if (game->computer_fleet[found_ship_index].hits_taken >= game->computer_fleet[found_ship_index].size) {
                if (!game->computer_fleet[found_ship_index].is_sunk) {
                    game->computer_fleet[found_ship_index].is_sunk = true;
                    game->ships_remaining_count--;
                    return SHOT_SUNK;
                } else { 
                    return SHOT_ALREADY_PROCESSED; 
                }
            } else {
                return SHOT_HIT;
            }
        } else {
            fprintf(stderr, "Error: Hit a non-empty, non-lowercase cell ('%c') on computer grid that doesn't map to a known ship letter.\n", target_on_computer_grid);
            return SHOT_ERROR; 
        }
    }
    fprintf(stderr, "Error: Unhandled case in processPlayerShot for cell '%c'.\n", target_on_computer_grid);
    return SHOT_ERROR;
}


void updateTargetGridForSunkShip(GameState *game, const Ship* sunk_ship) {
    for (int i = 0; i < sunk_ship->size; ++i) {
        int r = sunk_ship->segments[i].row;
        int c = sunk_ship->segments[i].col;
        game->player_target_grid[r][c] = sunk_ship->letter; 
    }
}

char numberToLetter(int num) {
    if (num >= 0 && num < 26) return 'A' + num;
    return '?'; 
}

int letterToNumber(char val) {
    return toupper(val) - 'A';
}

//-----------------------------------------------------------------------------
// IX. GAME STATE PERSISTENCE FUNCTIONS
//-----------------------------------------------------------------------------
bool saveGameState(const GameState *game) {
    FILE *file = fopen(SAVE_FILE_NAME, "wb");
    if (file == NULL) {
        perror("Error opening save file for writing");
        return false;
    }
    size_t written = fwrite(game, sizeof(GameState), 1, file);
    fclose(file);
    if (written != 1) {
        fprintf(stderr, "Error: Failed to write complete game state to save file.\n");
        return false;
    }
    return true;
}

bool loadGameState(GameState *game) {
    FILE *file = fopen(SAVE_FILE_NAME, "rb");
    if (file == NULL) {
        return false; 
    }
    size_t read_count = fread(game, sizeof(GameState), 1, file);
    fclose(file);
    if (read_count != 1) {
        fprintf(stderr, "Error: Failed to read complete game state from save file or file corrupted.\n");
        return false;
    }
    game->game_in_progress = true; 
    return true;
}

//-----------------------------------------------------------------------------
// X. SCORING FUNCTIONS
//-----------------------------------------------------------------------------
void viewTopScores() {
    clearScreen();
    printf("--- TOP 10 SCORES ---\n");
    ScoreEntry scores[10]; 
    int count = readScoresFromFile(scores, 10);

    if (count == 0) {
        printf("No scores recorded yet. Be the first!\n");
    } else {
        printf("Rank | Name | Score (Missiles) | Date Achieved\n");
        printf("-----|------|------------------|--------------------\n");
        for (int i = 0; i < count; ++i) {
            printf("%-4d | %-4s | %-16d | %s\n", i + 1, scores[i].player_name, scores[i].score_value, scores[i].date_time_achieved);
        }
    }
    printf("------------------------------------------------------\n");
    pauseForKey(NULL);
}

void updateTopScores(int new_score_value) {
    ScoreEntry scores[11]; 
    int count = readScoresFromFile(scores, 10);

    bool qualifies = false;
    if (count < 10) {
        qualifies = true;
    } else if (new_score_value < scores[9].score_value) { 
        qualifies = true;
    }

    if (qualifies) {
        char player_name_input[MAX_PLAYER_NAME_LEN + 10]; 
        printf("\nCongratulations! You've made the Top 10 high scores!\n");
        do {
            printf("Enter your initials (3 characters, e.g., ACE): ");
            safeGets(player_name_input, sizeof(player_name_input));
            if (strlen(player_name_input) != 3) {
                printf("Error: Initials must be exactly 3 characters. Please try again.\n");
            }
        } while (strlen(player_name_input) != 3);
        
        ScoreEntry new_entry;
        strncpy(new_entry.player_name, player_name_input, MAX_PLAYER_NAME_LEN -1);
        new_entry.player_name[MAX_PLAYER_NAME_LEN - 1] = '\0';
        new_entry.score_value = new_score_value;
        getCurrentDateTimeString(new_entry.date_time_achieved, DATETIME_STR_LEN);

        if (count < 10) {
            scores[count] = new_entry;
            count++;
        } else { 
            scores[9] = new_entry; 
        }

        sortScores(scores, count); 
        
        writeScoresToFile(scores, count); 
        printf("Your score has been recorded!\n");
        viewTopScores(); 
    } else {
        printf("Good game! Your score of %d missiles was not quite enough for the Top 10 this time.\n", new_score_value);
    }
}

int readScoresFromFile(ScoreEntry scores_array[], int max_scores_to_read) {
    FILE *file = fopen(SCORE_FILE_NAME, "r");
    if (file == NULL) {
        return 0; 
    }

    int num_scores = 0;
    while (num_scores < max_scores_to_read &&
           fscanf(file, "%3s %d %19[^\n]", 
                  scores_array[num_scores].player_name,
                  &scores_array[num_scores].score_value,
                  scores_array[num_scores].date_time_achieved) == 3) {
        scores_array[num_scores].player_name[MAX_PLAYER_NAME_LEN - 1] = '\0'; 
        scores_array[num_scores].date_time_achieved[DATETIME_STR_LEN - 1] = '\0'; 
        num_scores++;
    }
    fclose(file);

    sortScores(scores_array, num_scores); 
    return num_scores;
}

void writeScoresToFile(const ScoreEntry scores_array[], int count) {
    FILE *file = fopen(SCORE_FILE_NAME, "w"); 
    if (file == NULL) {
        perror("Error: Could not write scores to file");
        return;
    }

    for (int i = 0; i < count; ++i) {
        fprintf(file, "%s %d %s\n", scores_array[i].player_name, scores_array[i].score_value, scores_array[i].date_time_achieved);
    }
    fclose(file);
}

void sortScores(ScoreEntry scores_array[], int count) {
    ScoreEntry temp;
    for (int i = 0; i < count - 1; ++i) {
        for (int j = 0; j < count - i - 1; ++j) {
            if (scores_array[j].score_value > scores_array[j + 1].score_value) {
                temp = scores_array[j];
                scores_array[j] = scores_array[j + 1];
                scores_array[j + 1] = temp;
            }
        }
    }
}

void getCurrentDateTimeString(char* buffer, int buffer_size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M", tm_info);
}


//-----------------------------------------------------------------------------
// XI. UTILITY FUNCTIONS
//-----------------------------------------------------------------------------
void clearScreen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

void pauseForKey(const char* message) {
    if (message != NULL && strlen(message) > 0) {
        printf("%s\n", message);
    } else {
        printf("Press Enter to continue...");
    }
    
    int c;
    // Try to consume any leftover characters from previous input line
    // This is particularly important after a safeGets that might not have read the newline
    // if the input was exactly buffer_size-1 characters long.
    bool newline_found_in_buffer = false;
    while ((c = getchar()) != '\n' && c != EOF) {
        newline_found_in_buffer = true; // Consumed something other than just a newline
    }
    // If the buffer was empty and only a newline was pending from a previous Enter,
    // the above loop might not execute, and 'c' would be '\n'.
    // If getchar() returned EOF, something is wrong, but we proceed.
}


void safeGets(char *buffer, int size) {
    if (fgets(buffer, size, stdin) != NULL) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        } else {
            if (len == size -1 && buffer[len-1] != '\n') { // Buffer was filled without a newline
                int c;
                while ((c = getchar()) != '\n' && c != EOF); // Clear rest of stdin
            }
        }
    } else {
        buffer[0] = '\0'; 
        clearerr(stdin); 
    }
}