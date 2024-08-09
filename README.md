# BlockDigger

![image](https://github.com/user-attachments/assets/34939465-4369-4cff-bda0-f9d8983ccceb)

A very simple framework made with raw sockets and OpenGL for a multiplayer blocks mining game.
It's a good starting point for further customization and enhancements. 
Should compile on anything having GLUT/GL

# How does it work
- The max size of the generated block structure is 32x32x32 
- Server generates blocks around a center point that are adjacent to each other.
- Each block has 3 digs (3 clicks) to be destroyed. 
- Each destroyed blocks gives +1 to the player score
- Clients connect to the master server
- Clients rotate around the generated structure holding RMB
- Clients zoom with scroller
- Clients dig the blocks with LMB
- Clients render only visible faces

# Enhancements
Thousands, but this code is kept to minimum to allow customizing it's simple form

![image](https://github.com/user-attachments/assets/7f921dda-1927-4873-b164-dd97d9880e45)

