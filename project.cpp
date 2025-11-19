/*
 *  MORIS GUI Phase assignment
 */
#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <bitset>
#include <numeric>
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

#define NUM_POINTS 100   // number of points in each direction for the grid
#define MAX_GEOMETRIES 5 // Maximum number of geometries

using LS = exprtk::expression<double>; // Level set function type - returns phi(x,y,z)

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
   double tAsp = 16.0 / 9.0; // Aspect ratio
   int tFOV = 110;           // Field of view for perspective
   int tMode = 0;            // perspective mode switcher
   double tDim = 1.7;        // Size of the world
   int tPhi = 20;            // Elevation of view angle
   int tTheta = 0;           // Azimuth of view angle

   //-----------------------------------------------------------
   // Global lighting variables
   //-----------------------------------------------------------
   int tLight = 0;     // Lighting on or off
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
   double tShiny = 1;  // Shininess (value)
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
   int gSpatialDim = 2;                        // Spatial dimension (2D/3D)
   int tAxes = 1;                              // Display axes or not
   double tXLB = -1.0;                         // x lower bound
   double tXUB = 1.0;                          // x upper bound
   double tZLB = -1.0;                         // z lower bound
   double tZUB = 1.0;                          // z upper bound
   double gX, gY, gZ;                          // Global coordinates for LS evaluation
   std::vector<LS> gLevelSets(MAX_GEOMETRIES); // Vector of level-set functions
   int gActiveGeometry = 0;                    // Currently active geometry for user input
   int gNumGeoms = 0;                          // Number of geometries defined

   //-----------------------------------------------------------
   // Global phase variables
   //-----------------------------------------------------------

   // Phase table: initialize with values 0,1,2,...,(2^MAX_GEOMETRIES)-1
   std::vector<int> gPhaseTable = []()
   {
      int n = 1 << MAX_GEOMETRIES; // 2^MAX_GEOMETRIES (integer shift avoids <cmath>)
      std::vector<int> v(n);
      std::iota(v.begin(), v.end(), 0);
      return v;
   }();
   std::vector<PHASE> gGeomsPhaseToPlot(MAX_GEOMETRIES, PHASE::NONE); // 0 = don't plot, 1 = plot positive, -1 = plot negative, 2 = plot both

   // Colors for each geometry (Paraview KAAMS color scheme)
   std::vector<std::vector<double>> gColors = {
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
   std::vector<double> linspace(double start, double end, int num)
   {
      std::vector<double> arr(num);
      double step = (end - start) / (num - 1);
      for (int i = 0; i < num; i++)
         arr[i] = start + i * step;
      return arr;
   }

   /*
    *  Convert an integer to binary representation to determine which phase to draw
    */
   std::vector<int> int_to_binary(unsigned int aValue, int bits = MAX_GEOMETRIES)
   {
      if (bits <= 0)
         return {};

      std::vector<int> tBinary(bits, 0);
      // Fill MSB-first: tBinary[0] = bit (bits-1)
      for (int i = 0; i < bits; ++i)
      {
         int pos = bits - 1 - i;
         tBinary[i] = static_cast<int>((aValue >> pos) & 1u);
      }
      return tBinary;
   }

   /*
    * Gets the indices of the phase table that correspond to a given phase
    */
   std::vector<int> get_indices_for_phase(int aPhase)
   {
      std::vector<int> indices;
      for (size_t i = 0; i < gPhaseTable.size(); i++)
      {
         if (gPhaseTable[i] == aPhase)
         {
            indices.push_back(i);
         }
      }
      return indices;
   }

   void reset_active_phases()
   {
      for (int iG = 0; iG < MAX_GEOMETRIES; iG++)
      {
         gGeomsPhaseToPlot[iG] = PHASE::NONE;
      }
   }

   /**
    * Adds geometry phases to plot from binary representation
    * If bit is 1, plot positive phase; if bit is 0, plot negative phase
    * Ex. If positive phase is already active but the binary indicates negative phase, it will then plot both pos and neg phases
    */
   void append_active_phases_from_binary(const std::vector<int> &aBinary)
   {
      for (int iG = 0; iG < MAX_GEOMETRIES; iG++)
      {
         if (aBinary[iG] == 1)
         {
            // Set to plot positive phase
            if (gGeomsPhaseToPlot[iG] == PHASE::NONE)
            {
               gGeomsPhaseToPlot[iG] = PHASE::POSITIVE;
            }
            else if (gGeomsPhaseToPlot[iG] == PHASE::NEGATIVE)
            {
               gGeomsPhaseToPlot[iG] = PHASE::ALL;
            }
         }
         else
         {
            // Set to plot negative phase
            if (gGeomsPhaseToPlot[iG] == PHASE::NONE)
            {
               gGeomsPhaseToPlot[iG] = PHASE::NEGATIVE;
            }
            else if (gGeomsPhaseToPlot[iG] == PHASE::POSITIVE)
            {
               gGeomsPhaseToPlot[iG] = PHASE::ALL;
            }
         }
      }
   }

   /**
    * Gets all the bitsets from the phase table that are assigned to a given phase index,
    * then sets the active geometry phases to plot accordingly
    */
   void set_active_phases_from_phase_index(int aIndex)
   {
      // Reset active phases
      reset_active_phases();

      // Get all the bitset indices for the given phase
      std::vector<int> tIndices = get_indices_for_phase(aIndex);

      // Loop through the bitsets and convert to binary to set active regions for each geometry
      for (int iIndex : tIndices)
      {
         std::vector<int> tBinary = int_to_binary(iIndex, gNumGeoms); // Get the geometry signs for this index

         append_active_phases_from_binary(tBinary); // Add them to the active phases
      }
   }

   /**
    * Function to see the negative region for all currently active geometries
    */
   void set_all_active_phases_to_negative()
   {
      for (int iG = 0; iG < MAX_GEOMETRIES; iG++)
      {
         if (gGeomsPhaseToPlot[iG] != PHASE::NONE)
         {
            gGeomsPhaseToPlot[iG] = PHASE::NEGATIVE;
         }
      }
   }

   /**
    * Function to see the positive region for all currently active geometries
    */
   void set_all_active_phases_to_positive()
   {
      for (int iG = 0; iG < MAX_GEOMETRIES; iG++)
      {
         if (gGeomsPhaseToPlot[iG] != PHASE::NONE)
         {
            gGeomsPhaseToPlot[iG] = PHASE::POSITIVE;
         }
      }
   }

   /*
    *  Draw vertex in polar coordinates with normal
    */
   static void Vertex(double th, double ph)
   {
      double x = Sin(th) * Cos(ph);
      double y = Cos(th) * Cos(ph);
      double z = Sin(ph);
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
   static void ball(double x, double y, double z, double r)
   {
      //  Save transformation
      glPushMatrix();
      //  Offset, scale and rotate
      glTranslated(x, y, z);
      glScaled(r, r, r);
      //  White ball with yellow specular
      float yellow[] = {1.0, 1.0, 0.0, 1.0};
      float Emission[] = {0.0, 0.0, (float)0.01 * gEmission, 1.0};
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

   void get_phase_table_user_input()
   {
      // Read a full line from console
      std::string tInput;
      std::cout << "Enter phase numbers for each geometry (comma or space separated): ";
      std::getline(std::cin, tInput);

      // Normalize delimiters: replace commas with spaces so we can use >> extraction
      for (char &c : tInput)
      {
         if (c == ',')
            c = ' ';
      }

      // Initialize phase table to default 0 values, then overwrite with user input
      std::fill(gPhaseTable.begin(), gPhaseTable.end(), -1);

      std::stringstream ss(tInput);
      std::string token;
      size_t tPhase = 0;
      while (ss >> token)
      {
         token = trim(token); // remove surrounding whitespace
         if (token.empty())
            continue;
         if (tPhase >= gPhaseTable.size())
            break; // ignore extra values
         try
         {
            gPhaseTable[tPhase] = static_cast<int>(std::stoul(token));
         }
         catch (const std::exception &)
         {
            // Invalid token: leave as invalid phase and continue
            gPhaseTable[tPhase] = -1;
         }
         ++tPhase;
      }

      if (tPhase != std::pow(2, gNumGeoms))
      {
         std::cout << "Incorrect number of phases entered. Expected " << std::pow(2, gNumGeoms) << " but got " << tPhase << ". Try again\n";
         get_phase_table_user_input();
      }
   }

   LS load_LS_from_string(std::string aInput)
   {
      // Define LS variables, add them to exprtk symbol table
      exprtk::symbol_table<double> tSymbolTable;
      tSymbolTable.add_variable("x", gX);
      tSymbolTable.add_variable("y", gY);
      tSymbolTable.add_variable("z", gZ);
      tSymbolTable.add_constants();

      // Create expression
      exprtk::expression<double> tExpression;
      tExpression.register_symbol_table(tSymbolTable);

      // Parse the user input
      exprtk::parser<double> tParser;
      if (!tParser.compile(aInput, tExpression))
      {
         Fatal("Error: Failed to parse expression: %s", aInput.c_str());
      }

      return tExpression;
   }

   LS get_LS_user_input()
   {
      // Print that we are asking for user input
      glWindowPos2i(5, 5);
      Print("Enter a function of %s for the level-set function in the console. To be stored as Geometry %d", gSpatialDim == 2 ? "(x,y)" : "(x,y,z)", gActiveGeometry);

      // Get user input from console
      std::string tInput;
      std::cout << (gSpatialDim == 2 ? "Enter a level-set function of (x,y):" : "Enter a level-set function of (x,y,z):");
      std::getline(std::cin, tInput);

      return load_LS_from_string(tInput);
   }

   double eval_LS(LS aLS, double aX, double aY, double aZ = 0.0f)
   {
      // Set global coordinates
      gX = aX;
      gY = aY;
      gZ = aZ;

      // Evaluate expression
      return aLS.value();
   }

   void drawLS3D(LS aLS, PHASE aSign, int aColorIndex)
   {
      // TODO

      Fatal("drawLS3D not implemented yet");
   }

   void drawLS2D(LS aLS, PHASE aSign, int aColorIndex)
   {
      // Check if we need to plot this geometry
      if (aSign == PHASE::NONE)
      {
         return; // don't plot
      }

      glPushMatrix();

      std::vector<double> tXVals = linspace(tXLB, tXUB, NUM_POINTS);
      std::vector<double> tYVals = linspace(tZLB, tZUB, NUM_POINTS);

      // Draw the surface, splitting triangle strips when vertices don't match sign condition
      for (int i = 0; i < NUM_POINTS - 1; i++)
      {
         bool stripOpen = false;
         double x0 = tXVals[i];
         double x1 = tXVals[i + 1];

         for (int j = 0; j < NUM_POINTS; j++)
         {
            double z = tYVals[j]; // Since OpenGL Y is up, we use Z here. LS function is still (x,y)
            double y0 = eval_LS(aLS, x0, z);
            double y1 = eval_LS(aLS, x1, z);

            bool valid0 = !((aSign == PHASE::POSITIVE && y0 < 0) || (aSign == PHASE::NEGATIVE && y0 > 0));
            bool valid1 = !((aSign == PHASE::POSITIVE && y1 < 0) || (aSign == PHASE::NEGATIVE && y1 > 0));

            // If both vertices are valid, emit them into the strip. Otherwise close the current strip.
            if (valid0 && valid1)
            {
               if (!stripOpen)
               {
                  glBegin(GL_TRIANGLE_STRIP);
                  stripOpen = true;
               }
               // Set color for this geometry
               glColor3d(gColors[aColorIndex][0], gColors[aColorIndex][1], gColors[aColorIndex][2]);
               // Emit the two vertices for this column
               glVertex3d(x0, y0, z);
               glVertex3d(x1, y1, z);
            }
            else
            {
               if (stripOpen)
               {
                  glEnd();
                  stripOpen = false;
               }
            }
         }
         if (stripOpen)
         {
            glEnd();
            stripOpen = false;
         }
      }

      glPopMatrix();

      ErrCheck("drawLS");
   }

   void print_phase_table()
   {
      if (gNumGeoms == 0)
      {
         return;
      }

      // number of columns (geometries) to display
      int tNumCols = std::max(1, gNumGeoms);

      // helper: measure pixel length of an UTF-8/ASCII string
      auto tPixelLength = [&](const std::string &s) -> int
      {
#ifdef GLUT_BITMAP_LENGTH
         void *tFont = GLUT_BITMAP_HELVETICA_12;

         // some GLUT variations expose glutBitmapLength
         return glutBitmapLength(tFont, reinterpret_cast<const unsigned char *>(s.c_str()));
#else
         // fallback estimate: assume monospace ~8 px per char
         return static_cast<int>(s.size()) * 8;
#endif
      };

      // geometry column labels, build with fixed column spacing so columns align
      const int tColSpacingPixels = 40; // pixels between column centers
      // compute table and placement using a left margin so table sits at top-left
      const int tLeftX = 20; // left margin of the whole table
      int tTableWidth = tNumCols * tColSpacingPixels;

      // estimate width needed for the "Phase N | " label using the largest phase number
      int tnPhases = 1 << tNumCols;
      std::string samplePhaseLabel = "Phase " + std::to_string(std::max(0, tnPhases - 1)) + " | ";
      int phaseLabelW = tPixelLength(samplePhaseLabel);

      // table's X start (leave space for the phase label on the left)
      int tColsX = tLeftX + phaseLabelW + 10; // 10 px gap after phase label

      // draw each column label centered in its column cell
      int tColCenterX;
      for (int j = 0; j < tNumCols; ++j)
      {
         std::string tPhaseKey = std::to_string(j);
         int tKeyWidth = tPixelLength(tPhaseKey);
         tColCenterX = tColsX + j * tColSpacingPixels + (tColSpacingPixels / 2);
         int xpos = std::max(0, tColCenterX - tKeyWidth / 2);
         glWindowPos2i(xpos, 940);
         Print(tPhaseKey.c_str());
      }

      // Title
      std::string tTitle = "PHASE TABLE";
      std::string tDivider = "--------------------------------";
      glWindowPos2i(tPixelLength(tDivider) / 2, 960);
      Print(tTitle.c_str());

      // divider line
      glWindowPos2i(tLeftX, 920);
      Print("--------------------------------");

      // right-side value placement (to the right of the table)
      int tValueX = tColsX + tTableWidth + 10;

      for (int i = 0; i < tnPhases; ++i)
      {
         int y = 900 - i * 20;

         // draw phase label on the left (use baseX left margin)
         std::string tPhaseKey = "Phase " + std::to_string(i) + " | ";
         glWindowPos2i(tLeftX, y);
         Print(tPhaseKey.c_str());

         // draw each symbol at its column center
         for (int j = 0; j < tNumCols; ++j)
         {
            unsigned tBit = (static_cast<unsigned>(i) >> (tNumCols - 1 - j)) & 1u;
            std::string tKey(1, tBit ? '+' : '-');
            tColCenterX = tColsX + j * tColSpacingPixels + (tColSpacingPixels / 2);
            int tKeyWidth = tPixelLength(tKey);
            int tKeyX = std::max(0, tColCenterX - tKeyWidth / 2);
            glWindowPos2i(tKeyX, y);
            Print(tKey.c_str());
         }

         // draw the phase-table value to the right
         std::string tPhaseIndex = std::to_string(gPhaseTable[i]);
         glWindowPos2i(tValueX, y);
         Print(tPhaseIndex.c_str());
      }
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
         float Ambient[] = {(float)0.01 * gAmbient, (float)0.01 * gAmbient, (float)0.01 * gAmbient, 1.0};
         float Diffuse[] = {(float)0.01 * gDiffuse, (float)0.01 * gDiffuse, (float)0.01 * gDiffuse, 1.0};
         float Specular[] = {(float)0.01 * gSpecular, (float)0.01 * gSpecular, (float)0.01 * gSpecular, 1.0};
         //  Light position
         float Position[] = {tDistance * (float)Cos(tZeta), tYLight, tDistance * (float)Sin(tZeta), 1.0};
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

      // Set the plot to be centered at the middle of the domain
      glTranslated((-0.5 * (tXLB + tXUB)), 0.0, (-0.5 * (tZLB + tZUB)));

      switch (gSpatialDim)
      {
      case 2:
      {
         for (int iG = 0; iG < MAX_GEOMETRIES; iG++)
         {
            drawLS2D(gLevelSets[iG], gGeomsPhaseToPlot[iG], iG);
         }
         break;
      }
      case 3:
      {
         for (int iG = 0; iG < MAX_GEOMETRIES; iG++)
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
         Print("Z");
         glRasterPos3d(0, 0, 1);
         Print("Y");
      }

      // Display settings
      glColor3f(1.0, 1.0, 1.0);
      glWindowPos2i(5, 25);
      Print("Domain_x=[%f,%f] Domain_z=[%f,%f] Light=%s Lighting type=%s",
            tXLB, tXUB, tZLB, tZUB, tLight ? "On" : "Off", tSmooth ? "Smooth" : "Flat");

      // Print phase table for user reference
      print_phase_table();

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
      tAsp = (height > 0) ? (double)width / height : 1;

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

            // reset phase table
            std::iota(gPhaseTable.begin(), gPhaseTable.end(), 0);
         }
         else
         {
            std::cout << "Maximum number of geometries reached." << std::endl;
         }
      }
      else if (ch == 'p' || ch == 'P')
      {
         // Get user input for phase table
         get_phase_table_user_input();
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
      else if (ch == '_')
      {
         set_active_phases_from_phase_index(0); // since there's no F0 key
      }
      else if (ch == 'd' || ch == 'D')
      {
         // Delete the current active geometry
         // Shift all geometries after it down by one
         for (int iG = gActiveGeometry; iG < gNumGeoms - 1; iG++)
         {
            gLevelSets[iG] = gLevelSets[iG + 1];
            gGeomsPhaseToPlot[iG] = gGeomsPhaseToPlot[iG + 1];
         }
         gNumGeoms--;
         gLevelSets[gNumGeoms] = LS();               // Reset last geometry
         gGeomsPhaseToPlot[gNumGeoms] = PHASE::NONE; // Reset last geometry's phase to plot

         // reset phase table
         std::iota(gPhaseTable.begin(), gPhaseTable.end(), 0);

         // Plot all phases
         for (int iG = 0; iG < gNumGeoms; iG++)
         {
            gGeomsPhaseToPlot[iG] = PHASE::ALL;
         }
      }
      else if (ch == '+')
      {
         set_all_active_phases_to_positive();
      }
      else if (ch == '-')
      {
         set_all_active_phases_to_negative();
      }
      else if (ch == 32) // space bar
      {
         // Plot all phases
         for (int iG = 0; iG < gNumGeoms; iG++)
         {
            gGeomsPhaseToPlot[iG] = PHASE::ALL;
         }
      }
      else if (ch == 13) // Enter key
      {
         // Get user input for the current active geometry
         gLevelSets[gActiveGeometry] = get_LS_user_input();

         // Set to plot this geometry
         gGeomsPhaseToPlot[gActiveGeometry] = PHASE::ALL;
      }
      else if (ch == '/' || ch == '?')
      {
         // Load demo level-set functions
         gLevelSets[0] = load_LS_from_string("sin(0.43*x)+cos(y)-1");
         gLevelSets[1] = load_LS_from_string("sin(x)-1.2*cos(y)+1");
         gLevelSets[2] = load_LS_from_string("x^2+y^2-1");
         gNumGeoms = 3;

         // Set to plot all geometries
         for (int iG = 0; iG < gNumGeoms; iG++)
         {
            gGeomsPhaseToPlot[iG] = PHASE::ALL;
         }
         // Set demo phase table
         std::fill(gPhaseTable.begin(), gPhaseTable.end(), -1);
         gPhaseTable[0] = 0;
         gPhaseTable[1] = 0;
         gPhaseTable[2] = 0;
         gPhaseTable[3] = 0;
         gPhaseTable[4] = 1;
         gPhaseTable[5] = 0;
         gPhaseTable[6] = 0;
         gPhaseTable[7] = 0;
      }
      else if (ch == 27) // Escape key
         exit(0);

      // If a number key was pressed, update the active phases to plot to only plot that geometry
      if (ch == '0' || ch == '1' || ch == '2' || ch == '3' || ch == '4' ||
          ch == '5' || ch == '6' || ch == '7' || ch == '8' || ch == '9')
      {
         int tGeomIndex = ch - '0';
         reset_active_phases();
         gGeomsPhaseToPlot[tGeomIndex] = PHASE::ALL;
      }

      glutPostRedisplay();
   }

   /*
    *  Functionality to move the camera position
    */
   void special(int key, int x, int y)
   {
      if (key == GLUT_KEY_F1) // F1 key
      {
         set_active_phases_from_phase_index(1);
      }
      else if (key == GLUT_KEY_F2) // F2 key
      {
         set_active_phases_from_phase_index(2);
      }
      else if (key == GLUT_KEY_F3) // F3 key
      {
         set_active_phases_from_phase_index(3);
      }
      else if (key == GLUT_KEY_F4) // F4 key
      {
         set_active_phases_from_phase_index(4);
      }
      else if (key == GLUT_KEY_F5) // F5 key
      {
         set_active_phases_from_phase_index(5);
      }
      else if (key == GLUT_KEY_F6) // F6 key
      {
         set_active_phases_from_phase_index(6);
      }
      else if (key == GLUT_KEY_F7) // F7 key
      {
         set_active_phases_from_phase_index(7);
      }
      else if (key == GLUT_KEY_F8) // F8 key
      {
         set_active_phases_from_phase_index(8);
      }
      else if (key == GLUT_KEY_F9) // F9 key
      {
         set_active_phases_from_phase_index(9);
      }
      else if (key == GLUT_KEY_F10) // F10 key
      {
         set_active_phases_from_phase_index(10);
      }
      else if (key == GLUT_KEY_F11) // F11 key
      {
         set_active_phases_from_phase_index(11);
      }
      else if (key == GLUT_KEY_F12) // F12 key
      {
         set_active_phases_from_phase_index(12);
      }
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

         if (button == 3) // Scroll up - make the plotting domain smaller
         {
            float tLength = tXUB - tXLB;
            float tCenter = 0.5 * (tXUB + tXLB);
            tLength *= 0.98; // Zoom in by reducing length by 2%
            tXLB = tCenter - 0.5 * tLength;
            tXUB = tCenter + 0.5 * tLength;

            tLength = tZUB - tZLB;
            tCenter = 0.5 * (tZUB + tZLB);
            tLength *= 0.98; // Zoom in by reducing length by 2%
            tZLB = tCenter - 0.5 * tLength;
            tZUB = tCenter + 0.5 * tLength;
         }
         else if (button == 4) // Scroll down - zoom out
         {
            float tLength = tXUB - tXLB;
            float tCenter = 0.5 * (tXUB + tXLB);
            tLength *= 1.02; // Zoom out by increasing length by 2%
            tXLB = tCenter - 0.5 * tLength;
            tXUB = tCenter + 0.5 * tLength;

            tLength = tZUB - tZLB;
            tCenter = 0.5 * (tZUB + tZLB);
            tLength *= 1.02; // Zoom out by increasing length by 2%
            tZLB = tCenter - 0.5 * tLength;
            tZUB = tCenter + 0.5 * tLength;
         }

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
   //  Request double buffered true color window without Z-buffer
   glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
   //  Create window
   glutCreateWindow("Brendan Chong - MORIS Phase Assignment GUI");
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
