# BlockDigger

![image](https://github.com/user-attachments/assets/df3cb64e-0f3a-44c1-9b2b-929f4c615bda)
![image](https://github.com/user-attachments/assets/db406e1b-62f6-4d49-9701-98f7cc91edef)



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

![image](https://github.com/user-attachments/assets/b0bb4573-78a6-4d07-badf-e21a72dd05ec)


