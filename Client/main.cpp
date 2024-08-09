#include <algorithm> 
#include <GL/freeglut.h>
#include <GL/gl.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <map>
#include <cstring>
#include <ctime>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#define PICK_BUFFER_SIZE 16 * 16 * 16
#define BLOCK_SCALE 0.5
const int WIDTH = 800;
const int HEIGHT = 600;
const int PONG_INTERVAL = 5;
const int BUFFER_SIZE = 2048;



std::string receive_buffer;
std::map<std::string,int> mPlayers;
GLfloat fogColor[4] = {0.15f, 0.15f, 0.165f, 1.0f}; 
GLfloat lightPos[] = {0.0f, 10.0f, 10.0f, 1.0f};  // Position of light
GLfloat lightAmbient[] = {0.2f, 0.2f, 0.2f, 1.0f}; // Ambient light
GLfloat lightDiffuse[] = {0.8f, 0.8f, 0.8f, 1.0f}; // Diffuse light
GLfloat lightSpecular[] = {1.0f, 1.0f, 1.0f, 1.0f}; // Specular light

int server_socket;
std::string nick;
std::string server_ip;
int server_port;

time_t last_pong_time = 0;

struct Box {
    int x, y, z;
    GLubyte r, g, b; 

    Box() : x(0), y(0), z(0), r(1.0f), g(1.0f), b(1.0f) {}
    Box(int x, int y, int z, GLubyte r, GLubyte g, GLubyte b)
        : x(x), y(y), z(z), r(r), g(g), b(b) {}

    // When digged, drop the color 
    void Dig()
    {
       
       r/=2;
       g/=2;
       b/=2;
       if(r<0)r=0;
       if(g<0)g=0;
       if(b<0)b=0;

    }
    void Tick()
    { 
    }
    bool is_face_visible(const std::unordered_map<std::string, Box>& boxes, int dx, int dy, int dz) const {
        std::string key = std::to_string(x + dx) + "_" + std::to_string(y + dy) + "_" + std::to_string(z + dz);
        return boxes.find(key) == boxes.end();
    }
};

std::unordered_map<std::string, Box> boxes; // Key: "x_y_z", Value: Box


struct Camera {
    float posX, posY, posZ;
    float rotX, rotY;
    Camera() : posX(0.0f), posY(0.0f), posZ(50.0f), rotX(0.0f), rotY(0.0f) {}
    void apply() {
        glTranslatef(-posX, -posY, -posZ);
        glRotatef(rotX, 1.0f, 0.0f, 0.0f);
        glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    }
    void rotate(float x, float y) { rotX += x; rotY += y; }
    void zoom(float amount) {
        posZ += amount;
        if (posZ < 5.0f) posZ = 5.0f;
        if (posZ > 100.0f) posZ = 100.0f;
    }
    void pan(float x, float y) { posX += x; posY += y; }
};

Camera camera;
int lastMouseX, lastMouseY;
bool rotating = false;

void draw_face(float x, float y, float z, GLubyte r, GLubyte g, GLubyte b, const std::string& face) {
    
    glBegin(GL_QUADS);

    if (face == "front") {
        glNormal3f(0.0f, 0.0f, 1.0f);  // Normal pointing out of the screen
        glVertex3f(x - 0.25f, y - 0.25f, z + 0.25f);  // Bottom-left
        glVertex3f(x + 0.25f, y - 0.25f, z + 0.25f);  // Bottom-right
        glVertex3f(x + 0.25f, y + 0.25f, z + 0.25f);  // Top-right
        glVertex3f(x - 0.25f, y + 0.25f, z + 0.25f);  // Top-left

    } else if (face == "back") {
        glNormal3f(0.0f, 0.0f, -1.0f);  // Normal pointing into the screen
        glVertex3f(x - 0.25f, y - 0.25f, z - 0.25f);  // Bottom-left
        glVertex3f(x - 0.25f, y + 0.25f, z - 0.25f);  // Top-left
        glVertex3f(x + 0.25f, y + 0.25f, z - 0.25f);  // Top-right
        glVertex3f(x + 0.25f, y - 0.25f, z - 0.25f);  // Bottom-right

    } else if (face == "left") {
        glNormal3f(-1.0f, 0.0f, 0.0f);  // Normal pointing to the left
        glVertex3f(x - 0.25f, y - 0.25f, z + 0.25f);  // Bottom-left
        glVertex3f(x - 0.25f, y + 0.25f, z + 0.25f);  // Top-left
        glVertex3f(x - 0.25f, y + 0.25f, z - 0.25f);  // Top-right
        glVertex3f(x - 0.25f, y - 0.25f, z - 0.25f);  // Bottom-right

    } else if (face == "right") {
        glNormal3f(1.0f, 0.0f, 0.0f);  // Normal pointing to the right
        glVertex3f(x + 0.25f, y - 0.25f, z - 0.25f);  // Bottom-left
        glVertex3f(x + 0.25f, y + 0.25f, z - 0.25f);  // Top-left
        glVertex3f(x + 0.25f, y + 0.25f, z + 0.25f);  // Top-right
        glVertex3f(x + 0.25f, y - 0.25f, z + 0.25f);  // Bottom-right

    } else if (face == "top") {
        glNormal3f(0.0f, 1.0f, 0.0f);  // Normal pointing up
        glVertex3f(x - 0.25f, y + 0.25f, z - 0.25f);  // Bottom-left
        glVertex3f(x - 0.25f, y + 0.25f, z + 0.25f);  // Top-left
        glVertex3f(x + 0.25f, y + 0.25f, z + 0.25f);  // Top-right
        glVertex3f(x + 0.25f, y + 0.25f, z - 0.25f);  // Bottom-right

    } else if (face == "bottom") {
        glNormal3f(0.0f, -1.0f, 0.0f);  // Normal pointing down
        glVertex3f(x - 0.25f, y - 0.25f, z - 0.25f);  // Bottom-left
        glVertex3f(x + 0.25f, y - 0.25f, z - 0.25f);  // Bottom-right
        glVertex3f(x + 0.25f, y - 0.25f, z + 0.25f);  // Top-right
        glVertex3f(x - 0.25f, y - 0.25f, z + 0.25f);  // Top-left
    }

    glEnd();
}

void draw_box(const Box& box) {
    float x = box.x * BLOCK_SCALE;
    float y = box.y * BLOCK_SCALE;
    float z = box.z * BLOCK_SCALE;

    // Used for picking too
    glColor3ub(box.r, box.g, box.b);
   

    if (box.is_face_visible(boxes, 0, 0, 1)) draw_face(x, y, z, box.r, box.g, box.b, "front");
    if (box.is_face_visible(boxes, 0, 0, -1)) draw_face(x, y, z, box.r, box.g, box.b, "back");
    if (box.is_face_visible(boxes, -1, 0, 0)) draw_face(x, y, z, box.r, box.g, box.b, "left");
    if (box.is_face_visible(boxes, 1, 0, 0)) draw_face(x, y, z, box.r, box.g, box.b, "right");
    if (box.is_face_visible(boxes, 0, 1, 0)) draw_face(x, y, z, box.r, box.g, box.b, "top");
    if (box.is_face_visible(boxes, 0, -1, 0)) draw_face(x, y, z, box.r, box.g, box.b, "bottom");
 
}

void set_nonblocking(int socket) {
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}

bool connect_to_server() {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return false;
    }
    set_nonblocking(server_socket);

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

    if (connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        if (errno != EINPROGRESS) {
            std::cerr << "Error connecting to server" << std::endl;
            return false;
        }
    }

    std::string nick_command = "NICK " + nick + "\n";
    send(server_socket, nick_command.c_str(), nick_command.size(), 0);
    return true;
}

void send_pong() {
    std::string pong_command = "PONG\n";
    send(server_socket, pong_command.c_str(), pong_command.size(), 0);
}


void parse_message(const std::string& message) {
    std::istringstream iss(message);
    std::string command;
    iss >> command;

    if (command == "SPAWN") {
        int x, y, z;
        float r, g, b;
        iss >> x >> y >> z >> r >> g >> b;
        std::string key = std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z);
        boxes[key] = Box(x, y, z, r, g, b);             
    } else if (command == "DESTROY") {
        int x, y, z;
        iss >> x >> y >> z;
        std::string key = std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z);
        boxes.erase(key);
    }else if (command == "JOIN") {
        std::string strPlayer;
        iss >> strPlayer;
        mPlayers[strPlayer]=0;        
    }else if (command == "SCORE") {
        std::string strPlayer;
        int score;
        iss >> strPlayer >> score;
        mPlayers[strPlayer]=score;
    }
    else if (command == "LEAVE")
    {
        std::string strNick;
        iss >> strNick;
        mPlayers.erase(strNick);
    }
    else if (command == "DIG") {
        int x, y, z;
        iss >> x >> y >> z;
        std::string key = std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z);
        boxes[key].Dig();
    }
}

void receive_messages() {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(server_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';  // Null-terminate the buffer
        receive_buffer += std::string(buffer);  // Append new data to buffer

        // Process messages in the buffer
        size_t pos;
        while ((pos = receive_buffer.find('\n')) != std::string::npos) {
            std::string message = receive_buffer.substr(0, pos);
            receive_buffer.erase(0, pos + 1);  // Remove the processed message from buffer
            if (!message.empty()) {
                parse_message(message);  // Process the complete message
            }
        }
    } else if (bytes_received == 0) {
        // Connection closed by server
        std::cerr << "Connection lost" << std::endl;
        close(server_socket);
        exit(1);
    } else if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available at the moment, continue
            return;
        }
        std::cerr << "Receive error: " << strerror(errno) << std::endl;
        close(server_socket);
        exit(1);
    }
}

void RenderText(float x, float y, const std::string& text) {
    glRasterPos2f(x, y);
    for (char c : text) {
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, c);
    }
}

void pick(int x, int y) {
    GLuint selectBuf[PICK_BUFFER_SIZE];
    GLint hits;
    GLint viewport[4];

    // Setup selection buffer and enter selection mode
    glGetIntegerv(GL_VIEWPORT, viewport);
    glSelectBuffer(PICK_BUFFER_SIZE, selectBuf);
    glRenderMode(GL_SELECT);

    // Initialize the name stack
    glInitNames();
    glPushName(0);

    // Set up the projection matrix for picking
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Create a small picking region near the cursor
    gluPickMatrix((GLdouble)x, (GLdouble)(viewport[3] - y), 5.0, 5.0, viewport);
    gluPerspective(45.0, (double)WIDTH / HEIGHT, 1.0, 100.0);

    // Set up the modelview matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    camera.apply();
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    
    // Render each box with a unique name
    for (const auto& pair : boxes) {
        const std::string& key = pair.first;
        const Box& box = pair.second;

        // Convert the key (e.g., "1_2_3") into a name (unique integer)
        GLuint name = std::hash<std::string>{}(key);
        glLoadName(name);  // Assign the name to the current object

        draw_box(box);  // Render the box
    }
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
 
    // Get the hits and return to render mode
    hits = glRenderMode(GL_RENDER);

    // Process hits
    if (hits > 0) {
        GLuint* ptr = selectBuf;
        GLuint closestHit = ptr[3]; // The name of the closest hit object

        // Convert closestHit back to the box key
        for (const auto& pair : boxes) {
            const std::string& key = pair.first;
            GLuint name = std::hash<std::string>{}(key);
            if (name == closestHit) {
                // We found the box that was clicked
                const Box& box = pair.second;
                std::string dig_command = "DIG " + std::to_string(box.x) + " " + std::to_string(box.y) + " " + std::to_string(box.z) + "\n";
                send(server_socket, dig_command.c_str(), dig_command.size(), 0);
                break;
            }
        }
    }
}




 
void display() { 

     
    glClearColor(fogColor[0],fogColor[1],fogColor[2],fogColor[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();    
    gluPerspective(45.0, (double)WIDTH / HEIGHT, 1.0, 200.0);


    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    camera.apply();

    
    glEnable(GL_FOG);   
    glEnable(GL_COLOR_MATERIAL);    
    glEnable(GL_LIGHTING); 
    glEnable(GL_LIGHT0);  
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
 

    for (const auto& pair : boxes) {
        const Box& box = pair.second;
        draw_box(box);
    }

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHT0);
    glDisable(GL_LIGHTING);    
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_FOG); 


    // Switch to orthographic projection

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WIDTH, 0, HEIGHT); 

    // Switch to modelview matrix for rendering text
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

 // Draw the black bar at the top of the screen
    glColor3f(0.0f, 0.0f, 0.0f);  // Set color to black
    glBegin(GL_QUADS);
    glVertex2f(0, HEIGHT - 32);  // Bottom-left corner of the bar
    glVertex2f(WIDTH, HEIGHT - 32);  // Bottom-right corner of the bar
    glVertex2f(WIDTH, HEIGHT);  // Top-right corner of the bar
    glVertex2f(0, HEIGHT);  // Top-left corner of the bar
    glEnd();

 // Draw the black bar at the bottom of the screen
    glColor3f(0.0f, 0.0f, 0.0f);  // Set color to black
    glBegin(GL_QUADS);
    glVertex2f(0,  32);  // Bottom-left corner of the bar
    glVertex2f(WIDTH,  32);  // Bottom-right corner of the bar
    glVertex2f(WIDTH, 0);  // Top-right corner of the bar
    glVertex2f(0, 0);  // Top-left corner of the bar
    glEnd();
    

    glColor3f(1.0, 1.0, 1.0);
    RenderText(10, HEIGHT - 20, "Blocks: " + std::to_string(boxes.size()));
    std::string strScores = "";
    for (const auto& pair : mPlayers) { 
        strScores += pair.first+":"+std::to_string(pair.second)+" ";
    }
    RenderText(10, 16, strScores);


    glutSwapBuffers();

}
 

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / h, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_RIGHT_BUTTON) {
        if (state == GLUT_DOWN) {
            rotating = true;
            lastMouseX = x;
            lastMouseY = y;
        } else {
            rotating = false;
        }
    } else if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {               
        pick(x, y);
        glutPostRedisplay();  
    } else if (button == 3) {
        camera.zoom(-1.0f);
        glutPostRedisplay();
    } else if (button == 4) {
        camera.zoom(1.0f);
        glutPostRedisplay();
    }
}

void motion(int x, int y) {
    if (rotating) {
        float deltaX = (y - lastMouseY) * 0.2f;
        float deltaY = (x - lastMouseX) * 0.2f;
        camera.rotate(deltaX, deltaY);
        lastMouseX = x;
        lastMouseY = y;
        glutPostRedisplay();
    }
}

void idle() {
    receive_messages();

    time_t current_time = time(0);
    if (difftime(current_time, last_pong_time) >= PONG_INTERVAL) {
        send_pong();
        last_pong_time = current_time;
    }


    // Tick the boxes
    for (auto& pair : boxes) {
        Box& box = pair.second;
        box.Tick(); 
    }
 
    glutPostRedisplay();
}
int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <NICK> <Server>" << std::endl;
        return 1;
    }

    nick = argv[1];
    std::string server_info = argv[2];
    size_t colon_pos = server_info.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "Invalid server format. Use <IP>:<PORT>" << std::endl;
        return 1;
    }
    server_ip = server_info.substr(0, colon_pos);
    server_port = std::stoi(server_info.substr(colon_pos + 1));

    if (!connect_to_server()) {
        return 1;
    }

    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Multiplayer Digger");

    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.0f);
    glFogf(GL_FOG_END, 50.0f);
    glHint(GL_FOG_HINT, GL_NICEST);
    
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);

    glDepthFunc(GL_LESS);    
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);  


    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutIdleFunc(idle);

    glutMainLoop();

    close(server_socket);
    return 0;
}
