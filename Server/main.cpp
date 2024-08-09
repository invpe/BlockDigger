// MPDigger (c) 2k24 invpe
#include <random>
#include <vector>
#include <algorithm>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <ctime>
#include <sstream>
#include <cstdlib>
#include <algorithm>

const int PORT = 12345;
const int MAX_PLAYERS = 16;
const int TIMEOUT = 5; 
const int SPAWN_INTERVAL = 1; 
const int MAP_MIN = -16;
const int MAP_MAX = 16;


struct Player {
    int socket;
    std::string nick;
    sockaddr_in address;
    time_t last_active;    
    std::string buffer;
    int score;
    
};

// Function to generate a random color
std::tuple<int, int, int> generate_random_color() {
    int r = rand() % 255;
    int g = rand() % 255;
    int b = rand() % 255;
    return std::make_tuple(r, g, b);
}

// Assumed definition for Box
struct Box {
    int x, y, z;
    int r, g, b; // Color components
    int dig_count;

    Box(int x, int y, int z, int r = 0, int g = 0, int b = 0, int digs = 3) 
        : x(x), y(y), z(z), r(r), g(g), b(b), dig_count(digs) {}

    bool operator==(const Box& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
 
};
 
std::unordered_map<int, Player> players; // Key: socket
std::set<std::string> nicks; // Set of player nicks
fd_set master_set, read_set;
int server_socket, max_fd;
std::vector<Box> map; // Stores positions of boxes

void set_nonblocking(int socket) {
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}

void send_to_player(int player_socket, const std::string& message) {
    send(player_socket, message.c_str(), message.size(), 0);    
}

void broadcast(const std::string& message, int except_socket = -1) {
    for (const auto& pair : players) {
        if (pair.first != except_socket && !pair.second.nick.empty()) {
            send_to_player(pair.first, message);
        }
    } 
    printf("%s\n", message.data());
}

bool is_nick_taken(const std::string& nick) {
    return nicks.find(nick) != nicks.end();
}

void remove_player(int player_socket) {
    if (players.find(player_socket) != players.end()) {
        std::string nick = players[player_socket].nick;
        if (!nick.empty()) {
            nicks.erase(nick);
            broadcast("LEAVE " + nick + "\n");
            std::cout << "Player left: " << nick << std::endl;
        }
        players.erase(player_socket);
        close(player_socket);
        FD_CLR(player_socket, &master_set);
    }
} 

void handle_new_connection() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);

    if (client_socket < 0) {
        std::cerr << "Error accepting new connection" << std::endl;
        return;
    }

    set_nonblocking(client_socket);
    FD_SET(client_socket, &master_set);
    if (client_socket > max_fd) {
        max_fd = client_socket;
    }

    // Add the new player without a nickname
    players[client_socket] = {client_socket, "", client_addr, time(0), "", 0};

    // Print player joining message
    std::cout << "New player joined with socket: " << client_socket << std::endl;
}

void process_message(int client_socket, const std::string& message) {
    std::istringstream iss(message);
    std::string command;
    iss >> command;

    if (command == "NICK") {
        std::string nick;
        iss >> nick;

        if (is_nick_taken(nick) || nick.empty()) {
            send_to_player(client_socket, "Nick already taken or invalid!\n");
            close(client_socket);
            FD_CLR(client_socket, &master_set);
            players.erase(client_socket);
            return;
        }
        players[client_socket].nick = nick;
        nicks.insert(nick);
        broadcast("JOIN " + nick + "\n");

        // Send the current state of the map to the new player
        for (const auto& box : map) {
            std::string msg = "SPAWN " + std::to_string(box.x) + " " + std::to_string(box.y) + " " + std::to_string(box.z) + " " + std::to_string(box.r) + " " + std::to_string(box.g) + " " + std::to_string(box.b) + "\n";
            send_to_player(client_socket, msg);
        }

        // Send a list of players
        for (const auto& pair : players) {
            std::string msg = "SCORE "+pair.second.nick+" "+std::to_string(pair.second.score)+"\n";
            send_to_player(client_socket, msg);
        }
 
        // Print new player joined message with nickname
        std::cout << "Player joined: " << nick << std::endl;
    } else if (command == "PONG") {
        players[client_socket].last_active = time(0);
    } else {
        // If the player has not set a nickname, disconnect them
        if (players[client_socket].nick.empty()) {
            send_to_player(client_socket, "You must set a nickname first using the NICK command.\n");
            close(client_socket);
            FD_CLR(client_socket, &master_set);
            players.erase(client_socket);
            return;
        }

        if (command == "DIG") {
            int x, y, z;
            if (iss >> x >> y >> z) {
                auto it = std::find(map.begin(), map.end(), Box(x, y, z));
                if (it != map.end()) { 
                    if(it->dig_count>0)
                    {
                        it->dig_count--;
                        broadcast(message+"\n");
                    }
                    else
                    {
                        map.erase(it);
                        std::string msg = "DESTROY " + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z) + "\n";
                        broadcast(msg);

                        // add score
                        players[client_socket].score++;
                        msg = "SCORE "+players[client_socket].nick+" "+std::to_string(players[client_socket].score)+"\n";
                        broadcast(msg);
                    }
                } else {
                    send_to_player(client_socket, "No box at this position!\n");
                }
            } else {
                send_to_player(client_socket, "Invalid DIG command.\n");
            }
        } else if (command == "MSG") {
            std::string chat_message = message.substr(4);
            if (!chat_message.empty()) {
                std::string nick = players[client_socket].nick;
                std::string msg = "MSG " + nick + ": " + chat_message + "\n";
                broadcast(msg, client_socket);
            } else {
                send_to_player(client_socket, "Invalid MSG command.\n");
            }
        } else if (command == "QUIT") {           

            remove_player(client_socket);

        } else {
            send_to_player(client_socket, "Unknown command.\n");
        }
    }

    players[client_socket].last_active = time(0);
}

void handle_client_data(int client_socket) {
    char buffer[1024];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0) {
        // Client disconnected
        remove_player(client_socket);
        return;
    }

    buffer[bytes_received] = '\0';
    players[client_socket].buffer += buffer;

    size_t pos;
    while ((pos = players[client_socket].buffer.find('\n')) != std::string::npos) {
        std::string message = players[client_socket].buffer.substr(0, pos);
        players[client_socket].buffer.erase(0, pos + 1);
        process_message(client_socket, message);
    }
}

void check_timeouts() {
    time_t now = time(0);
    std::vector<int> to_remove;

    for (const auto& pair : players) {
        if (now - pair.second.last_active > TIMEOUT) {
            to_remove.push_back(pair.first);
        }
    }

    for (int socket : to_remove) {
        std::string nick = players[socket].nick;
        remove_player(socket);
        if (!nick.empty()) {
            broadcast("TIMEOUT " + nick + "\n");
            // Print player timeout message with nickname
            std::cout << "Player timed out: " << nick << std::endl;
        }
    }
}


Box get_random_box(const std::vector<Box>& boxes) {
    static std::mt19937 rng(time(nullptr)); // Random number generator
    std::uniform_int_distribution<size_t> dist(0, boxes.size() - 1);
    return boxes[dist(rng)];
}

bool is_position_free(int x, int y, int z, const std::vector<Box>& boxes) {
    // Check if the position is already occupied by a box
    auto it = std::find_if(boxes.begin(), boxes.end(), [x, y, z](const Box& b) {
        return b.x == x && b.y == y && b.z == z;
    });
    return it == boxes.end();
}

bool are_adjacent_positions_free(int x, int y, int z, const std::vector<Box>& boxes) {
    // Check adjacent positions (6 directions: +x, -x, +y, -y, +z, -z)
    const int directions[6][3] = {
        {1, 0, 0}, {-1, 0, 0},
        {0, 1, 0}, {0, -1, 0},
        {0, 0, 1}, {0, 0, -1}
    };

    for (const auto& dir : directions) {
        int adj_x = x + dir[0];
        int adj_y = y + dir[1];
        int adj_z = z + dir[2];

        if (is_position_free(adj_x, adj_y, adj_z, boxes)) {
            return true; // At least one adjacent position is free
        }
    }

    return false; // No adjacent positions are free
}

void spawn_boxes() {
    if (map.empty()) {
        std::cout << "No existing boxes to base new box placement on." << std::endl;
        return;
    }

    bool box_spawned = false;
    const int max_attempts = 100; // Number of attempts to find a valid position

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        // Choose a random box from the existing boxes
        Box random_box = get_random_box(map);

        // Check all adjacent positions for a free spot
        const int directions[6][3] = {
            {1, 0, 0}, {-1, 0, 0},
            {0, 1, 0}, {0, -1, 0},
            {0, 0, 1}, {0, 0, -1}
        };

        for (const auto& dir : directions) {
            int new_x = random_box.x + dir[0];
            int new_y = random_box.y + dir[1];
            int new_z = random_box.z + dir[2];

            if (is_position_free(new_x, new_y, new_z, map)) {
                // Generate a random color for the new box
                int r, g, b;
                std::tie(r, g, b) = generate_random_color();
                Box new_box(new_x, new_y, new_z, r, g, b);

                // Add the new box to the map
                map.push_back(new_box);

                // Prepare and send broadcast message
                std::string msg = "SPAWN " + std::to_string(new_x) + " " + std::to_string(new_y) + " " + std::to_string(new_z) + " " + std::to_string(r) + " " + std::to_string(g) + " " + std::to_string(b) + "\n";
                broadcast(msg);

                // Log the spawn
                std::cout << "Spawned new box at (" << new_x << ", " << new_y << ", " << new_z << ")" << std::endl;

                box_spawned = true;
                break; // Exit after spawning one box
            }
        }

        if (box_spawned) {
            break; // Exit the attempt loop if a box has been spawned
        }
    }

    if (!box_spawned) {
        std::cout << "No valid position found to spawn a new box adjacent to any existing box." << std::endl;
    }
}
int main() {
    srand(static_cast<unsigned>(time(0))); // Seed the random number generator

    sockaddr_in server_addr;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    set_nonblocking(server_socket);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error binding socket" << std::endl;
        return 1;
    }

    if (listen(server_socket, MAX_PLAYERS) < 0) {
        std::cerr << "Error listening on socket" << std::endl;
        return 1;
    }

    FD_ZERO(&master_set);
    FD_SET(server_socket, &master_set);
    max_fd = server_socket;

    time_t last_spawn = time(0);
    time_t last_status_report = time(0); // Timestamp for status reporting

    if (map.empty()) {
        // Initialize with a starting box if the map is empty
        int r, g, b;
        std::tie(r, g, b) = generate_random_color();
        int STARTING_BOX_X = 0;
        int STARTING_BOX_Y = 0;
        int STARTING_BOX_Z = 0;
        Box starting_box(STARTING_BOX_X, STARTING_BOX_Y, STARTING_BOX_Z, r, g, b);
        map.push_back(starting_box);
        std::cout << "Spawned initial box at (" << STARTING_BOX_X << ", " << STARTING_BOX_Y << ", " << STARTING_BOX_Z << ")" << std::endl;        
    }

    while (true) {
        read_set = master_set;

        struct timeval timeout;
        timeout.tv_sec = 1; // Check every second for timeouts
        timeout.tv_usec = 0;

        if (select(max_fd + 1, &read_set, nullptr, nullptr, &timeout) < 0) {
            std::cerr << "Error with select()" << std::endl;
            return 1;
        }

        for (int i = 0; i <= max_fd; ++i) {
            if (FD_ISSET(i, &read_set)) {
                if (i == server_socket) {
                    handle_new_connection();
                } else {
                    handle_client_data(i);
                }
            }
        }

        check_timeouts();

        // Spawn boxes at intervals
        if (difftime(time(0), last_spawn) >= SPAWN_INTERVAL) {
            spawn_boxes();
            last_spawn = time(0);
        }

        // Print the number of boxes and players every second
        if (difftime(time(0), last_status_report) >= 1) {
            printf("Players %i, Boxes %i\n", players.size(), map.size());
            last_status_report = time(0);
        }
    }

    close(server_socket);
    return 0;
}
