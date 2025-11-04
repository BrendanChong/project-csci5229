/*
 *  Lorenz Attractor Visualization
 */
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
//  Default resolution
//  For Retina displays compile with -DRES=2
#ifndef RES
#define RES 1
#endif

#define NUM_POINTS 100 // number of points in each direction for the grid

//-----------------------------------------------------------
// Global projection variables
//-----------------------------------------------------------

double asp = 16.0 / 9.0; // Aspect ratio
int fov = 110;           // Field of view for perspective
int m = 0;               // perspective mode switcher
double dim = 10.0;        // Size of the world
int ph = 20;             // Elevation of view angle
int th = 0;              // Azimuth of view angle
double Ex = 0.0;         // First person camera x position
double Ey = 2.0;         // first person camera y position
double Ez = -5.0;        // first person camera z position

//-----------------------------------------------------------
// Global lighting variables
//-----------------------------------------------------------
int light = 1; // Lighting on or off
int smooth = 1; // Smooth/Flat shading
int moveLight = 1; // Move light in idle or not

int one       =   1;  // Unit value
int distance  =   5;  // Light distance
int inc       =  10;  // Ball increment
int local     =   0;  // Local Viewer Model
int emission  =   0;  // Emission intensity (%)
int ambient   =  10;  // Ambient intensity (%)
int diffuse   =  50;  // Diffuse intensity (%)
int specular  =   0;  // Specular intensity (%)
int shininess =   0;  // Shininess (power of two)
float shiny   =   1;  // Shininess (value)
int zh        =  90;  // Light azimuth
float ylight  =  5;  // Elevation of light 

//-----------------------------------------------------------
// Global mouse variables
//-----------------------------------------------------------

int mouseX = 0, mouseY = 0;  // Current mouse position
int mouseCaptured = 0;       // Whether mouse is captured for camera control

//-----------------------------------------------------------
// Global LS variables
//-----------------------------------------------------------
int axes = 1; // Display axes or not
int waves = 0; // Animate waves or not
double xlb = 0.0; double xub = 3.14159265; // x bounds
double zlb = 0.0; double zub = 3.14159265; // x bounds

double nWaves = 10.0; // Number of waves in the x and z directions (frequency of the sine functions)

double xoff = 0; // x period shift for wave function
double yoff = 0; // y period shift for wave function
double crossOff = 0.0; // Offset for the xy term
double zoff = 0; // z intercept shift
double xscale = 1.0;
double zscale = 1.0;
int xscaleDir = 1, zscaleDir = 1, xoffDir = 1, yoffDir = 1, crossOffDir = 1, zoffDir = 1; // Direction of change for animation

// Level set function values
double func( double x, double z )
{
   return Sin( xscale * x * nWaves * 3.14159265  - xoff )  * Cos( zscale * z * nWaves * 3.14159265 - yoff ) + Sin( x * z - crossOff) + zoff;
}

// X gradient of Level set function (for normal)
double gradX( double x, double z )
{
   return xscale * nWaves * 3.14159265 * Cos( xscale * x * nWaves * 3.14159265 - xoff ) * Cos( zscale * z * nWaves * 3.14159265 - yoff ) + z * Cos( x * z - crossOff );
}

// Y gradient of Level set function (for normal)
double gradZ( double x, double z )
{
   return -zscale * nWaves * 3.14159265 * Sin( xscale * x * nWaves * 3.14159265 - xoff ) * Sin( zscale * z * nWaves * 3.14159265 - yoff ) + x * Cos( x * z - crossOff );
}

// For generating sampling points
double* linspace( double start, double end, int num )
{
   double* arr = malloc( num * sizeof(double) );
   double step = (end - start) / (num - 1);
   for( int i = 0; i < num; i++ )
      arr[i] = start + i * step;
   return arr;
}

/*
 *  Draw vertex in polar coordinates with normal
 */
static void Vertex(double th,double ph)
{
   double x = Sin(th)*Cos(ph);
   double y = Cos(th)*Cos(ph);
   double z =         Sin(ph);
   //  For a sphere at the origin, the position
   //  and normal vectors are the same
   glNormal3d(x,y,z);
   glVertex3d(x,y,z);
}

/*
 *  Draw a ball
 *     at (x,y,z)
 *     radius (r)
 */
static void ball(double x,double y,double z,double r)
{
   //  Save transformation
   glPushMatrix();
   //  Offset, scale and rotate
   glTranslated(x,y,z);
   glScaled(r,r,r);
   //  White ball with yellow specular
   float yellow[]   = {1.0,1.0,0.0,1.0};
   float Emission[] = {0.0,0.0,0.01*emission,1.0};
   glColor3f(1,1,1);
   glMaterialf(GL_FRONT,GL_SHININESS,shiny);
   glMaterialfv(GL_FRONT,GL_SPECULAR,yellow);
   glMaterialfv(GL_FRONT,GL_EMISSION,Emission);
   //  Bands of latitude
   for (int ph=-90;ph<90;ph+=inc)
   {
      glBegin(GL_QUAD_STRIP);
      for (int th=0;th<=360;th+=2*inc)
      {
         Vertex(th,ph);
         Vertex(th,ph+inc);
      }
      glEnd();
   }
   //  Undo transofrmations
   glPopMatrix();
}

void drawLS()
{   
   glPushMatrix();
   
   double* xvals = linspace( xlb, xub, NUM_POINTS );
   double* yvals = linspace( zlb, zub, NUM_POINTS );

   // Draw the surface
   for( int i = 0; i < NUM_POINTS - 1; i++ )
   {
      glBegin(GL_TRIANGLE_STRIP);
      for( int j = 0; j < NUM_POINTS; j++ )
      {
         double x = xvals[i];
         double z = yvals[j];
         double y = func( x, z );
         double nx = -gradX( x, z );
         double ny = 1.0;
         double nz = -gradZ( x, z );
         double length = sqrt( nx * nx + ny * ny + nz * nz );
         nx /= length; ny /= length; nz /= length;
         glColor3d( 1.0, 1.0, 1.0 );
         glNormal3d( nx, ny, nz );
         glVertex3d( x, y, z );

         x = xvals[i+1];
         z = yvals[j];
         y = func( x, z );
         nx = -gradX( x, z );
         ny = 1.0;
         nz = -gradZ( x, z );
         length = sqrt( nx * nx + ny * ny + nz * nz );
         nx /= length; ny /= length; nz /= length;
         glColor3d( 1.0, 1.0, 1.0 );
         glNormal3d( nx, ny, nz );
         glVertex3d( x, y, z );
      }
      glEnd();
   }

   free(xvals);
   free(yvals);

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
   glRotated(ph, 1.0, 0.0, 0.0);
   glRotated(th, 0.0, 1.0, 0.0);

   // Set the plot to be centered at the origin
   glTranslated( (-0.5 * (xlb + xub)), 0.0, (-0.5 * (zlb + zub)) );

   //  Flat or smooth shading
   glShadeModel(smooth ? GL_SMOOTH : GL_FLAT);

   //  Light switch
   if (light)
   {
      //  Translate intensity to color vectors
      float Ambient[]   = {0.01*ambient ,0.01*ambient ,0.01*ambient ,1.0};
      float Diffuse[]   = {0.01*diffuse ,0.01*diffuse ,0.01*diffuse ,1.0};
      float Specular[]  = {0.01*specular,0.01*specular,0.01*specular,1.0};
      //  Light position
      float Position[]  = {distance*Cos(zh),ylight,distance*Sin(zh),1.0};
      //  Draw light position as ball (still no lighting here)
      glColor3f(1,1,1);
      ball(Position[0],Position[1],Position[2] , 0.1);
      //  OpenGL should normalize normal vectors
      glEnable(GL_NORMALIZE);
      //  Enable lighting
      glEnable(GL_LIGHTING);
      //  Location of viewer for specular calculations
      //glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,local);
      //  glColor sets ambient and diffuse color materials
      glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
      glEnable(GL_COLOR_MATERIAL);
      //  Enable light 0
      glEnable(GL_LIGHT0);
      //  Set ambient, diffuse, specular components and position of light 0
      glLightfv(GL_LIGHT0,GL_AMBIENT ,Ambient);
      glLightfv(GL_LIGHT0,GL_DIFFUSE ,Diffuse);
      glLightfv(GL_LIGHT0,GL_SPECULAR,Specular);
      glLightfv(GL_LIGHT0,GL_POSITION,Position);
   }
   else
      glDisable(GL_LIGHTING);

   //  Draw the level set surface
   drawLS();

   glDisable(GL_LIGHTING); // No lighting for axes and text
   glColor3f(1, 1, 1);     // white
   if (axes)
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
   glColor3f(1.0, 1.0, 1.0 );
   glWindowPos2i(5, 5);
   Print("Angle=%d,%d  Dim=%.1f FOV=%d Projection=%s Light=%s",
         th, ph, dim, fov, m == 1 ? "Perspective" : "Orthogonal", light ? "On" : "Off");

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
   asp = (height > 0) ? (double)width / height : 1;

   glViewport(0, 0, width, height);

   // Apply projection based on the current mode
   if (m == 0)
      Project(0, asp, dim);
   else
      Project(fov, asp, dim);
}

void key(unsigned char ch, int x, int y)
{
   if( ch == 'm' || ch == 'M' )
   {
      m = 1 - m;
   }
   else if( ch == 'l' || ch == 'L' )
   {
      light = 1 - light;
   }
   else if (ch == 's' || ch == 'S')
   {
      smooth = 1 - smooth;
   }
   else if (ch == 'a' || ch == 'A')
   {
      axes = 1 - axes;
   }
   else if( ch == 'w' || ch == 'W')
   {
      waves = 1 - waves;
   }
   else if( ch == 'n' || ch == 'N' )
   {
      moveLight = 1 - moveLight;
   }
   else if (ch == 27) // Escape key
      exit(0);

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
   if( waves )
   {
      // Define min/max ranges for each parameter
      double xscaleMin = 0.5, xscaleMax = 2.0;
      double zscaleMin = 0.5, zscaleMax = 2.0;
      double xoffMin = 0.0, xoffMax = 6.2831853;  // 0 to 2 pi
      double yoffMin = 0.0, yoffMax = 6.2831853;
      double crossOffMin = 0.0, crossOffMax = 6.2831853;
      double zoffMin = 0.0, zoffMax = 1.0;
      
      // Generate random rates for the wave parameters to change
      double xscaleRate = 0.01 + 0.01 * rand() / (double)RAND_MAX;
      double zscaleRate = 0.01 + 0.01 * rand() / (double)RAND_MAX;
      double xoffRate = 0.01 + 0.01 * rand() / (double)RAND_MAX;
      double yoffRate = 0.01 + 0.01 * rand() / (double)RAND_MAX;
      double crossOffRate = 0.01 + 0.01 * rand() / (double)RAND_MAX;
      double zoffRate = 0.01 + 0.01 * rand() / (double)RAND_MAX;

      // Update xscale with proper min/max bounds
      xscale += xscaleDir * xscaleRate;
      if (xscale >= xscaleMax) { xscale = xscaleMax; xscaleDir = -1; }
      else if (xscale <= xscaleMin) { xscale = xscaleMin; xscaleDir = 1; }
      
      // Update zscale with proper min/max bounds
      zscale += zscaleDir * zscaleRate;
      if (zscale >= zscaleMax) { zscale = zscaleMax; zscaleDir = -1; }
      else if (zscale <= zscaleMin) { zscale = zscaleMin; zscaleDir = 1; }
      
      // Update xoff with proper min/max bounds
      xoff += xoffDir * xoffRate;
      if (xoff >= xoffMax) { xoff = xoffMax; xoffDir = -1; }
      else if (xoff <= xoffMin) { xoff = xoffMin; xoffDir = 1; }
      
      // Update yoff with proper min/max bounds
      yoff += yoffDir * yoffRate;
      if (yoff >= yoffMax) { yoff = yoffMax; yoffDir = -1; }
      else if (yoff <= yoffMin) { yoff = yoffMin; yoffDir = 1; }
      
      // Update crossOff with proper min/max bounds
      crossOff += crossOffDir * crossOffRate;
      if (crossOff >= crossOffMax) { crossOff = crossOffMax; crossOffDir = -1; }
      else if (crossOff <= crossOffMin) { crossOff = crossOffMin; crossOffDir = 1; }
      
      // Update zoff with proper min/max bounds
      zoff += zoffDir * zoffRate;
      if (zoff >= zoffMax) { zoff = zoffMax; zoffDir = -1; }
      else if (zoff <= zoffMin) { zoff = zoffMin; zoffDir = 1; }

   }
   if( moveLight )
   {
      zh = (zh + 1) % 360;
   }

   if( waves || moveLight )
   {
      glutPostRedisplay();
   }
}

/*
 *  Mouse motion callback
 */
void motion(int x, int y)
{
   if (mouseCaptured)
   {
      // Calculate mouse movement
      int deltaX = x - mouseX;
      int deltaY = y - mouseY;
      
      // Update angles based on mouse movement
      th += deltaX * 0.1;  // Horizontal movement controls azimuth (yaw)
      ph += deltaY * 0.1;  // Vertical movement controls elevation (pitch)
      
      // Keep phi within reasonable bounds
      if (ph > 89) ph = 89;
      if (ph < -89) ph = -89;
      
      // Wrap theta around 360 degrees
      if (th > 360) th -= 360;
      if (th < 0) th += 360;
      
      // Store current mouse position
      mouseX = x;
      mouseY = y;
      
      // Apply projection based on the current mode
      if (m == 0)
         Project(0, asp, dim);
      else
         Project(fov, asp, dim);
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
      if (state == GLUT_UP) return; // Only handle wheel on button down
      
      if (button == 3) // Scroll up - zoom in
      {
         dim *= 0.9;
      }
      else if (button == 4) // Scroll down - zoom out
      {
         dim *= 1.1;
      }
      
      // Keep dim within reasonable bounds
      if (dim < 0.1) dim = 0.1;
      if (dim > 50.0) dim = 50.0;
      
      // Update the projection
      if (m == 0)
         Project(0, asp, dim);
      else
         Project(fov, asp, dim);
         
      glutPostRedisplay();
      return;
   }
   
   if (button == GLUT_LEFT_BUTTON)
   {
      if (state == GLUT_DOWN)
      {
         // Capture mouse for camera control
         mouseCaptured = 1;
         mouseX = x;
         mouseY = y;
         glutSetCursor(GLUT_CURSOR_NONE);  // Hide cursor
      }
      else
      {
         // Release mouse capture
         mouseCaptured = 0;
         glutSetCursor(GLUT_CURSOR_INHERIT);  // Show cursor
      }
   }
}

// Main
int main(int argc, char *argv[])
{
   //  Initialize GLUT
   glutInit(&argc, argv);
   //  Request double buffered true color window without Z-buffer
   glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
   //  Create window
   glutCreateWindow("Brendan Chong - Ocean Viewer");
#ifdef USEGLEW
   //  Initialize GLEW
   if (glewInit() != GLEW_OK)
      Fatal("Error initializing GLEW\n");
#endif
   //  Register display, reshape, idle and key callbacks
   glutDisplayFunc(display);
   glutReshapeFunc(reshape);
   glutKeyboardFunc(key);
   glutSpecialFunc(special);
   glutIdleFunc(idle);
   glutMouseFunc(mouse);
   glutMotionFunc(motion);  
   //  Enable Z-buffer depth test
   glEnable(GL_DEPTH_TEST);
   //  Pass control to GLUT for events
   glutMainLoop();
   return 0;
}