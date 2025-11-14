/*
 *  MORIS GUI Phase assignment
 */
#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <bitset>
#include "CSCIx229.h"
#ifdef USEGLEW
#include <GL/glew.h>
#endif
#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include "exprtk.hpp"
//  Default resolution
//  For Retina displays compile with -DRES=2
#ifndef RES
#define RES 1
#endif

#define NUM_POINTS 100    // number of points in each direction for the grid
#define MAX_GEOMETRIES 10 // Maximum number of geometries

using LS = exprtk::expression<float>; // Level set function type - returns phi(x,y,z)

namespace moris::GUI
{
   /**
    * For phase plotting
    */
   enum class PHASE
   {
      NONE,
      POSITIVE,
      NEGATIVE,
      ALL
   };

   //-----------------------------------------------------------
   // Global projection variables
   //-----------------------------------------------------------

   // FIXME: change names to global convention
   float tAsp = 16.0 / 9.0; // Aspect ratio
   int tFOV = 110;          // Field of view for perspective
   int tMode = 0;           // perspective mode switcher
   float tDim = 3.5;        // Size of the world
   int tPhi = 20;           // Elevation of view angle
   int tTheta = 0;          // Azimuth of view angle

   //-----------------------------------------------------------
   // Global lighting variables
   //-----------------------------------------------------------
   int tLight = 1;     // Lighting on or off
   int tSmooth = 1;    // Smooth/Flat shading
   int tMoveLight = 1; // Move light in idle or not

   int tDistance = 5;  // Light distance
   int tInc = 10;      // Ball increment
   int tLocal = 0;     // Local Viewer Model
   int gEmission = 0;  // Emission intensity (%)
   int gAmbient = 5;   // Ambient intensity (%)
   int gDiffuse = 50;  // Diffuse intensity (%)
   int gSpecular = 5;  // Specular intensity (%)
   int gShininess = 0; // Shininess (power of two)
   float tShiny = 1;   // Shininess (value)
   int tZeta = 90;     // Light azimuth
   float tYLight = 5;  // Elevation of light

   //-----------------------------------------------------------
   // Global texture variables
   //-----------------------------------------------------------
   int tTextures = 0;        // Texture on or off
   unsigned int tTexture[3]; //  Texture names

   //-----------------------------------------------------------
   // Global mouse variables
   //-----------------------------------------------------------

   int tMouseX = 0, tMouseY = 0; // Current mouse position
   int tMouseCaptured = 0;       // Whether mouse is captured for camera control

   //-----------------------------------------------------------
   // Global LS variables
   //-----------------------------------------------------------
   uint gSpatialDim = 2;                       // Spatial dimension (2D/3D)
   int tAxes = 0;                              // Display axes or not
   float tXLB = -1.0;                          // x lower bound
   float tXUB = 1.0;                           // x upper bound
   float tZLB = -1.0;                          // z lower bound
   float tZUB = 1.0;                           // z upper bound
   float gX, gY, gZ;                           // Global coordinates for LS evaluation
   std::vector<LS> gLevelSets(MAX_GEOMETRIES); // Vector of level-set functions
   uint gActiveGeometry = 0;                   // Currently active geometry for user input
   uint gNumGeoms = 0;                         // Number of geometries defined

   //-----------------------------------------------------------
   // Global phase variables
   //-----------------------------------------------------------

   std::vector<uint> gPhaseTable;                                     // Phase table
   std::vector<PHASE> gGeomsPhaseToPlot(MAX_GEOMETRIES, PHASE::NONE); // 0 = don't plot, 1 = plot positive, -1 = plot negative, 2 = plot both

   // Colors for each geometry (Paraview KAAMS color scheme)
   std::vector<std::vector<float>> gColors = {
       {1.0, 0.0, 0.0},
       {0.0, 1.0, 0.0},
       {0.0, 0.0, 1.0},
       {1.0, 1.0, 0.0},
       {1.0, 0.0, 0.0},
       {0.0, 1.0, 1.0},
       {1.0, 0.0, 1.0},
       {0.5, 0.5, 0.5},
       {1.0, 0.5, 0.0},
       {0.5, 0.0, 0.5}};

   // For generating sampling points
   std::vector<float> linspace(float start, float end, int num)
   {
      std::vector<float> arr(num);
      float step = (end - start) / (num - 1);
      for (int i = 0; i < num; i++)
         arr[i] = start + i * step;
      return arr;
   }

   /*
    *  Convert an integer to binary representation to determine which phase to draw
    */
   std::vector<int> int_to_binary(int aValue)
   {
      // Create bitset from the integer value
      std::bitset<MAX_GEOMETRIES> tBits(aValue);
      std::vector<int> tBinary(MAX_GEOMETRIES, 0);
      for (int i = 0; i < MAX_GEOMETRIES; i++)
         tBinary[i] = tBits[MAX_GEOMETRIES - 1 - i];
      return tBinary;
   }

   /**
    * Convert binary vector to phase table
    */
   void set_phase_table_from_binary(const std::vector<int> &aBinary)
   {
      for (uint iG = MAX_GEOMETRIES; iG > 0; iG--)
      {
         if (aBinary[iG])
         {
            gGeomsPhaseToPlot[iG] = PHASE::POSITIVE;
         }
         else
         {
            gGeomsPhaseToPlot[iG] = PHASE::NEGATIVE;
         }
      }
   }

   /*
    *  Draw vertex in polar coordinates with normal
    */
   static void Vertex(float th, float ph)
   {
      float x = Sin(th) * Cos(ph);
      float y = Cos(th) * Cos(ph);
      float z = Sin(ph);
      //  For a sphere at the origin, the position
      //  and normal vectors are the same
      glNormal3d(x, y, z);
      glVertex3d(x, y, z);
   }

   /*
    *  Draw a ball
    *     at (x,y,z)
    *     radius (r)
    */
   static void ball(float x, float y, float z, float r)
   {
      //  Save transformation
      glPushMatrix();
      //  Offset, scale and rotate
      glTranslated(x, y, z);
      glScaled(r, r, r);
      //  White ball with yellow specular
      float yellow[] = {1.0, 1.0, 0.0, 1.0};
      float Emission[] = {0.0, 0.0, 0.01 * gEmission, 1.0};
      glColor3f(1, 1, 1);
      glMaterialf(GL_FRONT, GL_SHININESS, tShiny);
      glMaterialfv(GL_FRONT, GL_SPECULAR, yellow);
      glMaterialfv(GL_FRONT, GL_EMISSION, Emission);
      //  Bands of latitude
      for (int ph = -90; ph < 90; ph += tInc)
      {
         glBegin(GL_QUAD_STRIP);
         for (int th = 0; th <= 360; th += 2 * tInc)
         {
            Vertex(th, ph);
            Vertex(th, ph + tInc);
         }
         glEnd();
      }
      //  Undo transofrmations
      glPopMatrix();
   }

   // Helper to trim whitespace from start and end of a string
   std::string trim(const std::string &s)
   {
      size_t start = s.find_first_not_of(" \t\n\r");
      if (start == std::string::npos)
         return "";
      size_t end = s.find_last_not_of(" \t\n\r");
      return s.substr(start, end - start + 1);
   }

   std::vector<uint> get_phase_table_user_input()
   {
      // Print that we are asking for user input
      glWindowPos2i(5, 5);
      Print("Enter phase numbers for each geometry separated by spaces in the console.");

      // Get user input from console
      std::string tInput;
      std::cout << "Enter phase numbers for each geometry separated by spaces:";
      std::getline(std::cin, tInput);

      std::vector<uint> tPhaseTable;
      std::stringstream ss(tInput);
      std::string token;

      while (std::getline(ss, token, ' '))
      {
         token = trim(token); // remove whitespace
         if (!token.empty())
         {
            tPhaseTable.push_back(static_cast<uint>(std::stoul(token)));
         }
      }
      return tPhaseTable;
   }

   LS get_LS_user_input()
   {
      // Print that we are asking for user input
      glWindowPos2i(5, 5);
      Print("Enter a function of %s for the level-set function in the console. To be stored as Geometry %d", gSpatialDim == 2 ? "(x,y)" : "(x,y,z)", gActiveGeometry);

      // Get user input from console
      std::string tInput;
      std::cout << (gSpatialDim == 2 ? "Enter a level-set function of %s(x,y):" : "Enter a level-set function of (x,y,z):");
      std::getline(std::cin, tInput);

      // Define LS variables, add them to exprtk symbol table
      exprtk::symbol_table<float> tSymbolTable;
      tSymbolTable.add_variable("x", gX);
      tSymbolTable.add_variable("y", gY);
      tSymbolTable.add_variable("z", gZ);
      tSymbolTable.add_constants();

      // Create expression
      exprtk::expression<float> tExpression;
      tExpression.register_symbol_table(tSymbolTable);

      // Parse the user input
      exprtk::parser<float> tParser;
      if (!tParser.compile(tInput, tExpression))
      {
         Fatal("Error: Failed to parse expression: %s", tInput.c_str());
      }

      return tExpression;
   }

   float eval_LS(LS aLS, float aX, float aY, float aZ = 0.0f)
   {
      // Set global coordinates
      gX = aX;
      gY = aY;
      gZ = aZ;

      // Evaluate expression
      return aLS.value();
   }

   void drawLS3D(LS aLS, PHASE aSign, uint aColorIndex)
   {
      // TODO

      Fatal("drawLS3D not implemented yet");
   }

   void drawLS2D(LS aLS, PHASE aSign, uint aColorIndex)
   {
      // Check if we need to plot this geometry
      if (aSign == PHASE::NONE)
      {
         return; // don't plot
      }

      glPushMatrix();

      std::vector<float> tXVals = linspace(tXLB, tXUB, NUM_POINTS);
      std::vector<float> tYVals = linspace(tZLB, tZUB, NUM_POINTS);

      // Draw the surface
      for (int i = 0; i < NUM_POINTS - 1; i++)
      {
         glBegin(GL_TRIANGLE_STRIP);
         for (int j = 0; j < NUM_POINTS; j++)
         {
            // Compute coordinates from LS function
            float x = tXVals[i];
            float z = tYVals[j]; // Since OpenGL Y is up, we use Z here. LS function is still (x,y)
            float y = eval_LS(aLS, x, z);

            // Compute normal
            // float nx = -gradX(x, z);
            // float ny = 1.0;
            // float nz = -gradZ(x, z);
            // float length = sqrt(nx * nx + ny * ny + nz * nz);
            // nx /= length;
            // ny /= length;
            // nz /= length;

            // Check sign condition
            if ((aSign == PHASE::POSITIVE && y < 0) || (aSign == PHASE::NEGATIVE && y > 0))
            {
               y = 0.0f;                 // Clamp to zero level-set
               glColor3d(0.0, 0.0, 0.0); // Black color for clamped points (to hide plot)
            }
            else
            {
               // Set color, normal, texture coord, and vertex
               glColor3d(gColors[aColorIndex][0], gColors[aColorIndex][1], gColors[aColorIndex][2]);
            }
            // glNormal3d(nx, ny, nz);
            glVertex3d(x, y, z);

            // Repeat for the next point
            x = tXVals[i + 1];
            z = tYVals[j];
            y = eval_LS(aLS, x, z);

            // nx = -gradX(x, z);
            // ny = 1.0;
            // nz = -gradZ(x, z);
            // length = sqrt(nx * nx + ny * ny + nz * nz);
            // nx /= length;
            // ny /= length;
            // nz /= length;

            // Check sign condition
            if ((aSign == PHASE::POSITIVE && y < 0) || (aSign == PHASE::NEGATIVE && y > 0))
            {
               y = 0.0f;                 // Clamp to zero level-set
               glColor3d(0.0, 0.0, 0.0); // Black color for clamped points (to hide plot)
            }
            else
            {
               // Set color, normal, texture coord, and vertex
               glColor3d(gColors[aColorIndex][0], gColors[aColorIndex][1], gColors[aColorIndex][2]);
            }
            // glNormal3d(nx, ny, nz);
            glVertex3d(x, y, z);
         }
         glEnd();
      }

      glPopMatrix();

      ErrCheck("drawLS");
   }

   void display()
   {
      // Clear the image
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      // Reset transformations
      glLoadIdentity();

      // Apply rotations
      glRotated(tPhi, 1.0, 0.0, 0.0);
      glRotated(tTheta, 0.0, 1.0, 0.0);

      //  Flat or smooth shading
      glShadeModel(tSmooth ? GL_SMOOTH : GL_FLAT);

      //  Light switch
      if (tLight)
      {
         //  Translate intensity to color vectors
         float Ambient[] = {0.01 * gAmbient, 0.01 * gAmbient, 0.01 * gAmbient, 1.0};
         float Diffuse[] = {0.01 * gDiffuse, 0.01 * gDiffuse, 0.01 * gDiffuse, 1.0};
         float Specular[] = {0.01 * gSpecular, 0.01 * gSpecular, 0.01 * gSpecular, 1.0};
         //  Light position
         float Position[] = {tDistance * Cos(tZeta), tYLight, tDistance * Sin(tZeta), 1.0};
         //  Draw light position as ball (still no lighting here)
         glColor3f(1, 1, 1);
         ball(Position[0], Position[1], Position[2], 0.1);
         //  OpenGL should normalize normal vectors
         glEnable(GL_NORMALIZE);
         //  Enable lighting
         glEnable(GL_LIGHTING);
         //  Location of viewer for specular calculations
         // glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,local);
         //  glColor sets ambient and diffuse color materials
         glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
         glEnable(GL_COLOR_MATERIAL);
         //  Enable light 0
         glEnable(GL_LIGHT0);
         //  Set ambient, diffuse, specular components and position of light 0
         glLightfv(GL_LIGHT0, GL_AMBIENT, Ambient);
         glLightfv(GL_LIGHT0, GL_DIFFUSE, Diffuse);
         glLightfv(GL_LIGHT0, GL_SPECULAR, Specular);
         glLightfv(GL_LIGHT0, GL_POSITION, Position);
      }
      else
         glDisable(GL_LIGHTING);

      if (tTextures)
      {
         glEnable(GL_TEXTURE_2D);
         glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      }
      else
         glDisable(GL_TEXTURE_2D);

      // Set the plot to be centered at the origin
      glTranslated((-0.5 * (tXLB + tXUB)), 0.0, (-0.5 * (tZLB + tZUB)));

      switch (gSpatialDim)
      {
      case 2:
      {
         for (uint iG = 0; iG < MAX_GEOMETRIES; iG++)
         {

            drawLS2D(gLevelSets[iG], gGeomsPhaseToPlot[iG], iG);
         }
         break;
      }
      case 3:
      {
         for (uint iG = 0; iG < MAX_GEOMETRIES; iG++)
         {
            drawLS3D(gLevelSets[iG], gGeomsPhaseToPlot[iG], iG);
         }
         break;
      }
      default:
      {
         Fatal("Unsupported spatial dimension %d", gSpatialDim);
      }
      }

      glDisable(GL_LIGHTING);   // No lighting for axes and text
      glDisable(GL_TEXTURE_2D); // No textures for axes and text
      glColor3f(1, 1, 1);       // white
      if (tAxes)
      {
         //  Draw axes in white
         glBegin(GL_LINES);
         glVertex3d(0, 0, 0);
         glVertex3d(1, 0, 0);
         glVertex3d(0, 0, 0);
         glVertex3d(0, 1, 0);
         glVertex3d(0, 0, 0);
         glVertex3d(0, 0, 1);
         glEnd();
         //  Label axes
         glRasterPos3d(1, 0, 0);
         Print("X");
         glRasterPos3d(0, 1, 0);
         Print("Y");
         glRasterPos3d(0, 0, 1);
         Print("Z");
      }

      // Display settings
      glColor3f(1.0, 1.0, 1.0);
      glWindowPos2i(5, 100);
      Print("(th,ph)=%d,%d  Dim=%.1f Light=%s Lighting type=%s",
            tTheta, tPhi, tDim, tLight ? "On" : "Off", tSmooth ? "Smooth" : "Flat");

      // Error check
      ErrCheck("display");

      // Flush and swap buffer
      glFlush();
      glutSwapBuffers();
   }

   /*
    * This function is called by GLUT when the window is resized
    */
   void reshape(int width, int height)
   {
      // Avoid divide by zero
      tAsp = (height > 0) ? (float)width / height : 1;

      glViewport(0, 0, width, height);

      // Apply projection based on the current mode
      if (tMode == 0)
         Project(0, tAsp, tDim);
      else
         Project(tFOV, tAsp, tDim);
   }

   void key(unsigned char ch, int x, int y)
   {
      if (ch == 'm' || ch == 'M')
      {
         tMode = 1 - tMode;
      }
      else if (ch == 'l' || ch == 'L')
      {
         tLight = 1 - tLight;
      }
      else if (ch == 's' || ch == 'S')
      {
         tSmooth = 1 - tSmooth;
      }
      else if (ch == 'a' || ch == 'A')
      {
         tAxes = 1 - tAxes;
      }
      else if (ch == 't' || ch == 'T')
      {
         tTextures = 1 - tTextures;
      }
      else if (ch == 'n' || ch == 'N')
      {
         // Add a new geometry
         if (gNumGeoms < MAX_GEOMETRIES)
         {
            gActiveGeometry = gNumGeoms;
            gLevelSets[gNumGeoms] = get_LS_user_input();
            gGeomsPhaseToPlot[gNumGeoms] = PHASE::ALL; // Default to plot all phases
            gNumGeoms++;
         }
         else
         {
            std::cout << "Maximum number of geometries reached." << std::endl;
         }
      }
      else if (ch == 'p' || ch == 'P')
      {
         // Get user input for phase table
         gPhaseTable = get_phase_table_user_input();
      }
      else if (ch == '0')
      {
         gActiveGeometry = 0;
      }
      else if (ch == '1')
      {
         gActiveGeometry = 1;
      }
      else if (ch == '2')
      {
         gActiveGeometry = 2;
      }
      else if (ch == '3')
      {
         gActiveGeometry = 3;
      }
      else if (ch == '4')
      {
         gActiveGeometry = 4;
      }
      else if (ch == '5')
      {
         gActiveGeometry = 5;
      }
      else if (ch == '6')
      {
         gActiveGeometry = 6;
      }
      else if (ch == '7')
      {
         gActiveGeometry = 7;
      }
      else if (ch == '8')
      {
         gActiveGeometry = 8;
      }
      else if (ch == '9')
      {
         gActiveGeometry = 9;
      }
      else if (ch == 129) // F1 key
      {
         // Convert 1 to binary
         std::vector<int> tBinary = int_to_binary(1);

         set_phase_table_from_binary(tBinary);
      }
      else if (ch == 130) // F2 key
      {
         // Convert 2 to binary
         std::vector<int> tBinary = int_to_binary(2);

         set_phase_table_from_binary(tBinary);
      }
      else if (ch == 131) // F3 key
      {
         // Convert 3 to binary
         std::vector<int> tBinary = int_to_binary(3);

         set_phase_table_from_binary(tBinary);
      }
      else if (ch == 132) // F4 key
      {
         // Convert 4 to binary
         std::vector<int> tBinary = int_to_binary(4);

         set_phase_table_from_binary(tBinary);
      }
      else if (ch == 133) // F5 key
      {
         // Convert 5 to binary
         std::vector<int> tBinary = int_to_binary(5);

         set_phase_table_from_binary(tBinary);
      }
      else if (ch == 134) // F6 key
      {
         // Convert 6 to binary
         std::vector<int> tBinary = int_to_binary(6);

         set_phase_table_from_binary(tBinary);
      }
      else if (ch == 135) // F7 key
      {
         // Convert 7 to binary
         std::vector<int> tBinary = int_to_binary(7);

         set_phase_table_from_binary(tBinary);
      }
      else if (ch == 136) // F8 key
      {
         // Convert 8 to binary
         std::vector<int> tBinary = int_to_binary(8);

         set_phase_table_from_binary(tBinary);
      }
      else if (ch == 137) // F9 key
      {
         // Convert 9 to binary
         std::vector<int> tBinary = int_to_binary(9);

         set_phase_table_from_binary(tBinary);
      }
      else if (ch == 138) // F10 key
      {
         // Convert 10 to binary
         std::vector<int> tBinary = int_to_binary(10);

         set_phase_table_from_binary(tBinary);
      }
      else if (ch == 139) // F11 key
      {
         // Convert 11 to binary
         std::vector<int> tBinary = int_to_binary(11);

         set_phase_table_from_binary(tBinary);
      }
      else if (ch == 140) // F12 key
      {
         // Convert 12 to binary
         std::vector<int> tBinary = int_to_binary(12);

         set_phase_table_from_binary(tBinary);
      }
      else if (ch == 27) // Escape key
         exit(0);
      else if (ch == 13) // Enter key
      {
         // Get user input for the current active geometry
         gLevelSets[gActiveGeometry] = get_LS_user_input();

         // Set to plot this geometry
         gGeomsPhaseToPlot[gActiveGeometry] = PHASE::ALL;
      }

      glutPostRedisplay();
   }

   /*
    *  Functionality to move the camera position
    */
   void special(int key, int x, int y)
   {
   }

   // Function for basic animations
   void idle()
   {
      if (tMoveLight)
      {
         tZeta = (tZeta + 1) % 360;

         glutPostRedisplay();
      }
   }

   /*
    *  Mouse motion callback
    */
   void motion(int x, int y)
   {
      if (tMouseCaptured)
      {
         // Calculate mouse movement
         int deltaX = x - tMouseX;
         int deltaY = y - tMouseY;

         // Update angles based on mouse movement
         tTheta += deltaX * 0.1; // Horizontal movement controls azimuth (yaw)
         tPhi += deltaY * 0.1;   // Vertical movement controls elevation (pitch)

         // Keep phi within reasonable bounds
         if (tPhi > 89)
            tPhi = 89;
         if (tPhi < -89)
            tPhi = -89;

         // Wrap theta around 360 degrees
         if (tTheta > 360)
            tTheta -= 360;
         if (tTheta < 0)
            tTheta += 360;

         // Store current mouse position
         tMouseX = x;
         tMouseY = y;

         // Apply projection based on the current mode
         if (tMode == 0)
            Project(0, tAsp, tDim);
         else
            Project(tFOV, tAsp, tDim);
         glutPostRedisplay();
      }
   }

   /*
    *  Mouse button callback
    */
   void mouse(int button, int state, int x, int y)
   {
      // Handle mouse wheel events
      if (button == 3 || button == 4) // Mouse wheel up/down
      {
         if (state == GLUT_UP)
            return; // Only handle wheel on button down

         if (button == 3) // Scroll up - zoom in
         {
            tDim *= 0.9;
         }
         else if (button == 4) // Scroll down - zoom out
         {
            tDim *= 1.1;
         }

         // Keep dim within reasonable bounds
         if (tDim < 0.1)
            tDim = 0.1;
         if (tDim > 50.0)
            tDim = 50.0;

         // Update the projection
         if (tMode == 0)
            Project(0, tAsp, tDim);
         else
            Project(tFOV, tAsp, tDim);

         glutPostRedisplay();
         return;
      }

      if (button == GLUT_LEFT_BUTTON)
      {
         if (state == GLUT_DOWN)
         {
            // Capture mouse for camera control
            tMouseCaptured = 1;
            tMouseX = x;
            tMouseY = y;
            glutSetCursor(GLUT_CURSOR_NONE); // Hide cursor
         }
         else
         {
            // Release mouse capture
            tMouseCaptured = 0;
            glutSetCursor(GLUT_CURSOR_INHERIT); // Show cursor
         }
      }
   }

} // namespace moris::GUI

// Main
int main(int argc, char *argv[])
{
   //  Initialize GLUT
   glutInit(&argc, argv);
   //  Request float buffered true color window without Z-buffer
   glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
   //  Create window
   glutCreateWindow("Brendan Chong - Sloshing box");
#ifdef USEGLEW
   //  Initialize GLEW
   if (glewInit() != GLEW_OK)
      Fatal("Error initializing GLEW\n");
#endif
   //  Register display, reshape, idle and key callbacks
   glutDisplayFunc(moris::GUI::display);
   glutReshapeFunc(moris::GUI::reshape);
   glutKeyboardFunc(moris::GUI::key);
   glutSpecialFunc(moris::GUI::special);
   glutIdleFunc(moris::GUI::idle);
   glutMouseFunc(moris::GUI::mouse);
   glutMotionFunc(moris::GUI::motion);
   //  Enable Z-buffer depth test
   glEnable(GL_DEPTH_TEST);

   ErrCheck("init");

   //  Pass control to GLUT for events
   glutMainLoop();
   return 0;
}
