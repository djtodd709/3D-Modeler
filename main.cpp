#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <list>
#include <iostream>
#include <string>
#include <fstream>
#include <cstdlib>
#include "mathHelper.h"

#ifdef __APPLE__
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
#  include <GLUT/glut.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#  include <GL/freeglut.h>
#include <valarray>
#endif


#define X 0
#define Y 1
#define Z 2

using namespace std;

//Global Variables
int selectedMaterial = 0;

//Initialize here because they're used in the Object class
bool showBounding = false;
float m_temp[4] = {1.0f,1.0f,1.0f,1}; //Temporary material
float no_mat[4] = { 0,0,0,1 }; //Empty material

//Ray vars
double* m_start = new double[3];
double* m_end = new double[3];

//Material 0 properties - kinda grassy
float m0_dif[4] = {0.6f,0.9f,0.4f,1};
float m0_amb[4] = {0.3f,0.45f,0.2f,1};
float m0_spe[4] = {0.6f,0.9f,0.4f,1};
float shiny0 = 10;

//Material 1 properties - kinda gold
float m1_dif[4] = {0.9f,0.8f,0.2f,1};
float m1_amb[4] = {0.5f,0.4f,0,1};
float m1_spe[4] = {1,1,1,1};
float shiny1 = 60;

//Material 2 properties - white
float m2_dif[4] = {0.9f,0.9f,0.9f,1};
float m2_amb[4] = {0.5f,0.5f,0.5f,1};
float m2_spe[4] = {1,1,1,1};
float shiny2 = 10;

//Material 3 properties - red
float m3_dif[4] = {0.9f,0.2f,0.2f,1};
float m3_amb[4] = {0.5f,0.1f,0.1f,1};
float m3_spe[4] = {1,1,1,1};
float shiny3 = 30;

//Material 4 properties - blue
float m4_dif[4] = {0.2f,0.2f,0.9f,1};
float m4_amb[4] = {0.1f,0.1f,0.5f,1};
float m4_spe[4] = {1,1,1,1};
float shiny4 = 30;

struct Object{
	//Transform data
	float position[3];
	float rotation[3];
	float scale[3];
	//Type of object
	int objType;
	//Material data
	int materialType;

        //Bounding box vertices
        float* bpvertices[8];
        int* bpfaces[6];

        //ModelView matrix
        double matModelView[16];
        double invModelView[16];

        //Local ray
        double lm_start[3];
        double lm_end[3];

        //Intersection status
        bool rayHit;
        double intersect1[3];
        double intersect2[3];

        //Notes:
        //Since bounding box rotates, scales, translates, need to keep track of its surfaces as planes
        //Then test ray intersection with individual planes
        //Wonder if there's an easier way than doing it manually?

        //WAIT NO
        //Transform the ray with the inverse of the box transformation matrix
        //Then do aabb slab ray intersection, then transform it by the box matrix again
        //WAY easier!

        //Get world to local transformation matrix by inverting the object's modelview matrix
        //Since M * M^-1 = I, A * M = A * M^-1
        void getLocalMatrix(){

            //Get the modelview matrix for this object by applying transform commands to identity matrix
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix(); //Create new matrix on stack so we can call this from anywhere
                glLoadIdentity();
                glTranslatef(position[X],position[Y],position[Z]);
                glRotatef(rotation[X],1,0,0);
                glRotatef(rotation[Y],0,1,0);
                glRotatef(rotation[Z],0,0,1);
                glScalef(scale[X],scale[Y],scale[Z]);
                glGetDoublev(GL_MODELVIEW_MATRIX, matModelView); //Read matrix
            glPopMatrix(); //Restore matrix stack to original state

            //Figure out matrix inverse

            //Debug output
            for(int i=0; i<sizeof(matModelView)/sizeof(matModelView[0]); i++){
                if (i%4==3)
                    cout<<matModelView[i]<<"\n";
                else
                    cout<<matModelView[i]<<" ";
            }

            cout<<"Inverse->\n";
            gluInvertMatrix(matModelView, invModelView); //Calculate inverse matrix

            for(int i=0; i<sizeof(invModelView)/sizeof(invModelView[0]); i++){
                if (i%4==3)
                    cout<<invModelView[i]<<"\n";
                else
                    cout<<invModelView[i]<<" ";
            }
            cout<<"\n";
        }

        //Calculate a ray that is relative to the origin of the box
        void getLocalRay(){
            getLocalMatrix(); //Make sure we have latest inverse modelview
            //World ray => Local ray
            matrixMultiplyRay(invModelView, m_start, lm_start);
            matrixMultiplyRay(invModelView, m_end, lm_end);
            //Debug
            printf("Local ray\n");
            printf("(%f,%f,%f)-->(%f,%f,%f)\n\n", lm_start[0], lm_start[1], lm_start[2], lm_end[0], lm_end[1], lm_end[2]);
        }

        //Undo the transformation - this is just for checking right now
        //but once we calculate intersection points in local space, they can be transformed to world space in the same way
        void getWorldRay(){
            //Local ray => World ray
            matrixMultiplyRay(matModelView, lm_start, m_start);
            matrixMultiplyRay(matModelView, lm_end, m_end);
            printf("Local ray\n");
            printf("(%f,%f,%f)-->(%f,%f,%f)\n", lm_start[0], lm_start[1], lm_start[2], lm_end[0], lm_end[1], lm_end[2]);
        }

        //Future home for slab intersection
        bool rayBoxIntersect(void){
            getLocalRay();
            double rayDir[3] = {
                lm_end[0]-lm_start[0],
                lm_end[1]-lm_start[1],
                lm_end[2]-lm_start[2]
            };
            //Normalize
            double rayMag = sqrt(rayDir[0]*rayDir[0]+rayDir[1]*rayDir[1]+rayDir[2]*rayDir[2]);
            rayDir[0] = rayDir[0]/rayMag; rayDir[1] = rayDir[1]/rayMag; rayDir[2] = rayDir[2]/rayMag;

            printf("Ray Vector\n");
            printf("(%f,%f,%f)\n", rayDir[0], rayDir[1], rayDir[2]);

            double t1, t2;
            double tmin = -INFINITY, tmax = INFINITY;
            //Since we're always intersecting with a vertex aligned box,
            //We can assume the vertices are defined correctly and we can use vertex 0 and vertex 6 to define it
            //0 is l and 6 is h
            for (int i=0; i<3;i++){
                if (rayDir[i]==0){
                    if (lm_start[i]< bpvertices[0][i] || lm_start[i]>bpvertices[6][i]){
                        return false;
                    }
                } else {
                    t1 = (bpvertices[0][i] - lm_start[i]) / rayDir[i];
                    t2 = (bpvertices[6][i] - lm_start[i]) / rayDir[i];
                }
                if (t1 > t2)
                    swap(t1,t2);
                if (t1 > tmin)
                    tmin = t1;
                if (t2 < tmax)
                    tmax = t2;
                if (tmin > tmax)
                    return false;
                if (tmax < 0)
                    return false;
            }


            //For each slab do:	//example using X planes
            //if (xd = 0)	//ray is parallel to the planes
            //if (x0 < xl or x0 > xh)
            //return false;	//outside slab
            //else
            //T1 = (xl – x0) / xd
            //T2 = (xh – x0) / xd
            //if (T1 > T2)
            //swap (T1, T2) //since T1 intersection with near plane
            //if (T1 > Tnear)
            //Tnear = T1 //want largest Tnear
            //if (T2 < Tfar)
            //Tfar = T2 //want smallest Tfar
            //if (Tnear > Tfar)
            //return false	//box is missed
            //if (Tfar < 0)
            //return false //box behind ray origin
            //End for;
            //Return true;
            printf ("Intersection!\n");
            return true;
        }

        //Method to apply material based on variable
        void selectMaterial(){
                switch(materialType){
                        case 0:		//Apply material settings
                                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, m0_dif);
                                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, m0_amb);
                                glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, m0_spe);
                                glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shiny0);
                                break;
                        case 1:	//Apply material settings
                                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, m1_dif);
                                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, m1_amb);
                                glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, m1_spe);
                                glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shiny1);
                                break;
                        case 2:	//Apply material settings
                                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, m2_dif);
                                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, m2_amb);
                                glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, m2_spe);
                                glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shiny2);
                                break;
                        case 3:	//Apply material settings
                                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, m3_dif);
                                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, m3_amb);
                                glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, m3_spe);
                                glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shiny3);
                                break;
                        case 4:	//Apply material settings
                                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, m4_dif);
                                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, m4_amb);
                                glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, m4_spe);
                                glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shiny4);
                                break;
                }

        }

	//method that actually draws the object
	void draw(){
                //Local coordinate bounding box for debug
                if (showBounding && objType != -1 ){
                    m_temp[0] = 1; m_temp[1] = 1; m_temp[2] = 0;
                    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, m_temp);
                    for(int i=0; i<6; i++){

                        glBegin(GL_LINE_STRIP);
                            glVertex3fv(bpvertices[bpfaces[i][0]]);
                            glVertex3fv(bpvertices[bpfaces[i][1]]);
                            glVertex3fv(bpvertices[bpfaces[i][2]]);
                            glVertex3fv(bpvertices[bpfaces[i][3]]);
                            glVertex3fv(bpvertices[bpfaces[i][0]]);
                        glEnd();

                    }
                    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, no_mat);
                }
		glPushMatrix();
			//Apply Material
			selectMaterial();
			//Apply transformations
			glTranslatef(position[X],position[Y],position[Z]);
			glRotatef(rotation[X],1,0,0);
			glRotatef(rotation[Y],0,1,0);
			glRotatef(rotation[Z],0,0,1);
			glScalef(scale[X],scale[Y],scale[Z]);
			//Render object
			switch (objType) {
				case 0: glutSolidCube(1);
								break;
				case 1: glutSolidSphere(1,16,16);
								break;
				case 2: glRotatef(-90,1,0,0);
								glutSolidCone(1,1,16,16);
								break;
				case 3: glutSolidTorus(0.5,1,16,16);
								break;
				case 4: glutSolidOctahedron();
								break;
			}

                        //Draw the bounding box - oops something is connected wrong
                        if (showBounding && objType != -1 ){
                            m_temp[0] = 1; m_temp[1] = 0; m_temp[2] = 0;
                            glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, m_temp);
                            for(int i=0; i<6; i++){

                                glBegin(GL_LINE_STRIP);
                                    glVertex3fv(bpvertices[bpfaces[i][0]]);
                                    glVertex3fv(bpvertices[bpfaces[i][1]]);
                                    glVertex3fv(bpvertices[bpfaces[i][2]]);
                                    glVertex3fv(bpvertices[bpfaces[i][3]]);
                                    glVertex3fv(bpvertices[bpfaces[i][0]]);
                                glEnd();

                            }
                            glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, no_mat);
                        }
		glPopMatrix();
	}
};

//Constructor
Object MakeObject(int model){
	Object o;
	o.materialType = selectedMaterial;
	o.objType = model;
	//position
	o.position[X] = 0;
	o.position[Y] = 0;
	o.position[Z] = 0;
	//rotation
	o.rotation[X] = 0;
	o.rotation[Y] = 0;
	o.rotation[Z] = 0;
	//scale
	o.scale[X] = 1;
	o.scale[Y] = 1;
	o.scale[Z] = 1;

        //Initialize the bounding box
        //Sorry for the trash code :(
        //Mostly from that one lecture on meshes
        //Vertices
        o.bpvertices[0] = new float[3];
        o.bpvertices[0][0] = -1.0; o.bpvertices[0][1] = -1.0; o.bpvertices[0][2] = 1.0;
        o.bpvertices[1] = new float[3];
        o.bpvertices[1][0] = 1.0; o.bpvertices[1][1] = -1.0; o.bpvertices[1][2] = 1.0;
        o.bpvertices[2] = new float[3];
        o.bpvertices[2][0] = 1.0; o.bpvertices[2][1] = 1.0; o.bpvertices[2][2] = 1.0;
        o.bpvertices[3] = new float[3];
        o.bpvertices[3][0] = -1.0; o.bpvertices[3][1] = 1.0; o.bpvertices[3][2] = 1.0;
        o.bpvertices[4] = new float[3];
        o.bpvertices[4][0] = -1.0; o.bpvertices[4][1] = -1.0; o.bpvertices[4][2] = -1.0;
        o.bpvertices[5] = new float[3];
        o.bpvertices[5][0] = 1.0; o.bpvertices[5][1] = -1.0; o.bpvertices[5][2] = -1.0;
        o.bpvertices[6] = new float[3];
        o.bpvertices[6][0] = 1.0; o.bpvertices[6][1] = 1.0; o.bpvertices[6][2] = -1.0;
        o.bpvertices[7] = new float[3];
        o.bpvertices[7][0] = -1.0; o.bpvertices[7][1] = 1.0; o.bpvertices[7][2] = -1.0;

        //Faces
        o.bpfaces[0] = new int[4];
        o.bpfaces[0][0] = 0; o.bpfaces[0][1] = 1; o.bpfaces[0][2] = 2; o.bpfaces[0][3] = 3;
        o.bpfaces[1] = new int[4];
        o.bpfaces[1][0] = 1; o.bpfaces[1][1] = 5; o.bpfaces[1][2] = 6; o.bpfaces[1][3] = 2;
        o.bpfaces[2] = new int[4];
        o.bpfaces[2][0] = 0; o.bpfaces[2][1] = 1; o.bpfaces[2][2] = 5; o.bpfaces[2][3] = 4;
        o.bpfaces[3] = new int[4];
        o.bpfaces[3][0] = 4; o.bpfaces[3][1] = 5; o.bpfaces[3][2] = 6; o.bpfaces[3][3] = 7;
        o.bpfaces[4] = new int[4];
        o.bpfaces[4][0] = 0; o.bpfaces[4][1] = 4; o.bpfaces[4][2] = 7; o.bpfaces[4][3] = 3;
        o.bpfaces[5] = new int[4];
        o.bpfaces[5][0] = 3; o.bpfaces[5][1] = 2; o.bpfaces[5][2] = 6; o.bpfaces[5][3] = 7;

        //Init ray
        o.rayHit = false;

	return o;
}

list<Object> sceneGraph;
Object* selectedObject;
Object placeholder;

//Light 0 properties - orange
float l0pos[4] = {10.0f,5.0f,10.0f,1};
float l0dif[4] = {0.7f,0.4f,0.2f,1};
float l0amb[4] = {0.2f,0.2f,0.2f,1};
float l0spe[4] = {0.9f,0.7f,0.5f,1};

//Light 1 properties - blue
float l1pos[4] = {-10.0f,5.0f,-10.0f,1};
float l1dif[4] = {0.2f,0.4f,0.7f,1};
float l1amb[4] = {0.2f,0.2f,0.2f,1};
float l1spe[4] = {0.5f,0.7f,0.9f,1};


//viewing angles, used in rotating the scene
float angle = 0.0f;
float Vangle = 0.0f;

float camDist;
float camTwist;
float camElev;
float camAzimuth;
float camX = 0;
float camZ = 0;

//position of camera and target.
float camPos[] = {0, 9, 27};	//where the camera is
float camTarget[] = {0,0,0};


//initial settings for main window. Called (almost) at the beginning of the program.
void init(void){
	glClearColor(0.3f, 0.3f, 0.3f, 0);	//black background
	glColor3f(1, 1, 1);

	glMatrixMode(GL_PROJECTION);

	//Enable lights and depth testing
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);

	//Implement backface culling
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	//Apply light 0 settings
	glLightfv(GL_LIGHT0, GL_POSITION, l0pos);
	glLightfv(GL_LIGHT0, GL_AMBIENT, l0amb);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, l0dif);
	glLightfv(GL_LIGHT0, GL_SPECULAR, l0spe);

	//Apply light 1 settings
	glLightfv(GL_LIGHT1, GL_POSITION, l1pos);
	glLightfv(GL_LIGHT1, GL_AMBIENT, l1amb);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, l1dif);
	glLightfv(GL_LIGHT1, GL_SPECULAR, l1spe);

	//Apply material settings
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, m1_dif);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, m1_amb);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, m1_spe);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shiny1);

	//adjust view
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluPerspective(45, 1, 1, 1000);

	//Add empty model to be selected at start
	placeholder = MakeObject(-1);
	selectedObject = &placeholder;

	//print instructions
	printf("3D Modeler\n");
	printf("-----------Instructions----------\n");
	printf("Q/Esc - Quit\n");
	printf("Arrow - Turn (left/right) or tilt (up/down) scene\n");
	printf("WSADZX - Move the object forward/back/left/right/up/down\n");
	printf("ALT+WASDZX - Rotate the object forward/back/left/right/up/down\n");
	printf("SHIFT+WASDZX - Rotate the object forward/back/left/right/up/down\n");
	printf("12345 - Change selected material to green/yellow/white/red/blue\n");
	printf("YUIOP - Create cube/sphere/cone/torus/octahedron\n");
	printf("M - Apply selected material to selected shape\n");
	printf("B/N - Save/Load\n");
        printf("-----------Debug-----------------\n");
        printf("k - enable object bounding boxes, yellow is relative to center of object\n");
        printf("l - recalculate ray relative to the box\n");
	printf("---------------------------------\n");

}

//function for moving an object, dependent on the camera angle
void moveObject(Object* o, int direction, float distance){
	//math equation to calculate the direction the terrain is currently facing (from 0 to 3 inclusive)
	int facing = (((int)angle+45)/90)%4;
	//equation adjusted if angle is negative
	if(angle < 0){
		facing = ((((int)angle-45)/90)+4)%4;
	}
	//object is moved in one of the 4 directions, depending on the facing value.
	switch(facing){
		case 0:
			o->position[direction] += distance;
			break;
		case 1:
			o->position[(-direction)+2] -= distance*(direction-1);	//(-direction)+2 swaps the X and Z values (0 and 2)
			break;
		case 2:
			o->position[direction] -= distance;
			break;
		case 3:
			o->position[(-direction)+2] += distance*(direction-1);	//we multiply distance by direction-1 as we need to reverse the distance if travelling on the Z axis
			break;
	}
}

//function for scaling an object, dependent on the camera angle
void scaleObject(Object* o, int direction, float distance){
	//math equation to calculate the direction the terrain is currently facing (from 0 to 3 inclusive)
	int facing = (((int)angle+45)/90)%4;
	//equation adjusted if angle is negative
	if(angle < 0){
		facing = ((((int)angle-45)/90)+4)%4;
	}
	//object is moved in one of the 4 directions, depending on the facing value.
	switch(facing){
		case 0:
			if(o->scale[direction] > 0.3 || distance > 0)
				o->scale[direction] += distance;
			break;
		case 1:
			if(direction == X){
				if(o->scale[Z] > 0.3 || distance < 0)
					o->scale[(-direction)+2] -= distance;	//(-direction)+2 swaps the X and Z values (0 and 2)
			}
			else{
				if(o->scale[(-direction)+2] > 0.3 || distance < 0)
					o->scale[(-direction)+2] -= distance*(direction-1);	//(-direction)+2 swaps the X and Z values (0 and 2)
			}
			break;
		case 2:
			if(o->scale[direction] > 0.3 || distance < 0)
				o->scale[direction] -= distance;
			break;
		case 3:
			if(direction == X){
				if(o->scale[Z] > 0.3 || distance > 0)
					o->scale[(-direction)+2] += distance;	//we multiply distance by direction-1 as we need to reverse the distance if travelling on the Z axis
			}
			else{
				if(o->scale[(-direction)+2] > 0.3 || distance > 0)
					o->scale[(-direction)+2] += distance*(direction-1);	//(-direction)+2 swaps the X and Z values (0 and 2)
			}
			break;
	}
}

//function for rotating an object
void rotateObject(Object* o, int direction, float distance){
	//math equation to calculate the direction the terrain is currently facing (from 0 to 3 inclusive)
	int facing = (((int)angle+45)/90)%4;
	//equation adjusted if angle is negative
	if(angle < 0){
		facing = ((((int)angle-45)/90)+4)%4;
	}
	//object is moved in one of the 4 directions, depending on the facing value.
	switch(facing){
		case 0:
			o->rotation[direction] += distance;
			break;
		case 1:
			o->rotation[(-direction)+2] -= distance*(direction-1);	//(-direction)+2 swaps the X and Z values (0 and 2)
			break;
		case 2:
			o->rotation[direction] -= distance;
			break;
		case 3:
			o->rotation[(-direction)+2] += distance*(direction-1);	//we multiply distance by direction-1 as we need to reverse the distance if travelling on the Z axis
			break;
	}
}

void save(){

	string filename;
	cout << "What name would you like to give your file? (no .txt):\n";
	getline (cin, filename);

	filename += ".txt";
	ofstream savefile (filename.c_str());
	for(list<Object>::const_iterator iterator = sceneGraph.begin(); iterator != sceneGraph.end(); iterator++){
		Object o = ((Object) *iterator);
		savefile << o.position[X] << "\n" << o.position[Y] << "\n" << o.position[Z] << "\n";
		savefile << o.rotation[X] << "\n" << o.rotation[Y] << "\n" << o.rotation[Z] << "\n";
		savefile << o.scale[X] << "\n" << o.scale[Y] << "\n" << o.scale[Z] << "\n";
		savefile << o.objType << "\n" << o.materialType << "\n";
	}
	savefile.close();
	cout << "Saved\n";
}

void load(){

	string filename;
	cout << "What is the name of your file? (no .txt):\n";
	getline (cin, filename);

	filename += ".txt";
	ifstream savefile (filename.c_str());
	string objData;
	if (savefile.is_open())
  {
		angle = 0.0f;
		Vangle = 0.0f;
		sceneGraph.clear();
		//Add empty model to be selected at start
		selectedObject = &placeholder;
    while ( getline (savefile,objData) )
    {
			if(objData != ""){
				Object o;
				o.position[X] = atof(objData.c_str());
				getline(savefile,objData);
				o.position[Y] = atof(objData.c_str());
				getline(savefile,objData);
				o.position[Z] = atof(objData.c_str());
				getline(savefile,objData);
				o.rotation[X] = atof(objData.c_str());
				getline(savefile,objData);
				o.rotation[Y] = atof(objData.c_str());
				getline(savefile,objData);
				o.rotation[Z] = atof(objData.c_str());
				getline(savefile,objData);
				o.scale[X] = atof(objData.c_str());
				getline(savefile,objData);
				o.scale[Y] = atof(objData.c_str());
				getline(savefile,objData);
				o.scale[Z] = atof(objData.c_str());
				getline(savefile,objData);
				o.objType = atoi(objData.c_str());
				getline(savefile,objData);
				o.materialType = atoi(objData.c_str());

				sceneGraph.push_back(o);
			}
    }
    savefile.close();
		cout << "Loaded\n";
  }
	else{
		cout << "Unable to open file";
	}

}

//function for keyboard commands
void keyboard(unsigned char key, int xIn, int yIn){
	int mod = glutGetModifiers();
	Object o;
	switch (key){
		case 'q':	//quit
		case 27:	//27 is the esc key
			exit(0);
			break;
		case 'r':
			angle = 0.0f;
			Vangle = 0.0f;
			sceneGraph.clear();
			//Add empty model to be selected at start
			selectedObject = &placeholder;
			break;

                /*
               //camera zoom
                case '.':
                        camDist += 1;
                        break;
                case ',':
                        camDist -= 1;
                        break;
                */
		///movement controls - explained more in moveObject method
		case 'w':	//move light forward
			if ( mod == GLUT_ACTIVE_ALT)
                            rotateObject(selectedObject, X, -1);
			else
                            moveObject(selectedObject,Z,-0.3);
			break;
		case 'W':
			scaleObject(selectedObject, Z, 0.3);
			break;
		case 's':	//move light backwards
			if ( mod == GLUT_ACTIVE_ALT)
                            rotateObject(selectedObject, X, 1);
			else
                            moveObject(selectedObject,Z,0.3);
			break;
		case 'S':
			scaleObject(selectedObject, Z, -0.3);
			break;
		case 'd':	//move light right
			if ( mod == GLUT_ACTIVE_ALT)
                            rotateObject(selectedObject, Z, -1);
			else
                            moveObject(selectedObject,X,0.3);
			break;
		case 'D':
			scaleObject(selectedObject, X, 0.3);
			break;
		case 'a':	//move light left
			if ( mod == GLUT_ACTIVE_ALT)
                            rotateObject(selectedObject, Z, 1);
			else
                            moveObject(selectedObject,X,-0.3);
			break;
		case 'A':
			scaleObject(selectedObject, X, -0.3);
			break;
                case 'z':
                    if (mod == GLUT_ACTIVE_ALT)
                        selectedObject->rotation[Y] -= 1;
                    else
                        selectedObject->position[Y] += 0.3;
                    break;
                case 'Z':
                    selectedObject->scale[Y] += 0.3;
                    break;
                case 'x':
                    if (mod == GLUT_ACTIVE_ALT)
                        selectedObject->rotation[Y] += 1;
                    else
                        selectedObject->position[Y] -= 0.3;
                    break;
                case 'X':
                    if (selectedObject->scale[Y] > 0.3)
                        selectedObject->scale[Y] -= 0.3;
                    break;

                //Debug controls
                case 'k':
                    selectedObject->rayBoxIntersect();
                    break;
                case 'l':
                    showBounding = !showBounding;
                    break;
                case 'm':
                    selectedObject->materialType = selectedMaterial;
                    break;

                case '1':
                    selectedMaterial = 0;
                    break;
                case '2':
                    selectedMaterial = 1;
                    break;
                case '3':
                    selectedMaterial = 2;
                    break;
                case '4':
                    selectedMaterial = 3;
                    break;
                case '5':
                    selectedMaterial = 4;
                    break;
								case 'y':
										o = MakeObject(0);
										sceneGraph.push_back(o);
										selectedObject = &sceneGraph.back();
										break;
								case 'u':
										o = MakeObject(1);
										sceneGraph.push_back(o);
										selectedObject = &sceneGraph.back();
										break;
								case 'i':
										o = MakeObject(2);
										sceneGraph.push_back(o);
										selectedObject = &sceneGraph.back();
										break;
								case 'o':
										o = MakeObject(3);
										sceneGraph.push_back(o);
										selectedObject = &sceneGraph.back();
										break;
								case 'p':
										o = MakeObject(4);
										sceneGraph.push_back(o);
										selectedObject = &sceneGraph.back();
										break;
								case 'n':
										load();
										break;
								case 'b':
										save();
										break;
	}
}

//Raycasting - mostly class code
//Move ray logic to separate file once sure about the inputs/outputs

void rayCast(int x, int y){
    printf("Projecting\n");

    printf("(%f,%f,%f)-->(%f,%f,%f)\n", m_start[0], m_start[1], m_start[2], m_end[0], m_end[1], m_end[2]);
    double matModelView[16], matProjection[16];
    int viewport[4];

    // get matrix and viewport:
    glGetDoublev(GL_MODELVIEW_MATRIX, matModelView);
    glGetDoublev(GL_PROJECTION_MATRIX, matProjection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    // window pos of mouse, Y is inverted on Windows
    double winX = (double) x;
    double winY = viewport[3] - (double) y;

    // get point on the 'near' plane (third param is set to 0.0)
    gluUnProject(winX, winY, 0.0, matModelView, matProjection,
            viewport, &m_start[0], &m_start[1], &m_start[2]);

    // get point on the 'far' plane (third param is set to 1.0)
    gluUnProject(winX, winY, 1.0, matModelView, matProjection,
            viewport, &m_end[0], &m_end[1], &m_end[2]);

    // now you can create a ray from m_start to m_end
    printf("(%f,%f,%f)-->(%f,%f,%f)\n\n", m_start[0], m_start[1], m_start[2], m_end[0], m_end[1], m_end[2]);

}

//display call for the ray
void drawRay(){

    m_temp[0] = 0; m_temp[1] = 1; m_temp[2] = 0;
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, m_temp); //Ray should not be affected by lighting
    glBegin(GL_LINES);
            glVertex3f(m_start[0], m_start[1], m_start[2]);
            glVertex3f(m_end[0], m_end[1], m_end[2]);
    glEnd();
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, no_mat);

    //Draw local object ray
    if (showBounding && selectedObject->objType != -1 ){
        m_temp[0] = 1; m_temp[1] = 1; m_temp[2] = 0;
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, m_temp);
        glBegin(GL_LINES);
            glVertex3dv(selectedObject->lm_start);
            glVertex3dv(selectedObject->lm_end);
        glEnd();
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, no_mat);
    }
}

void mouse(int btn, int state, int x, int y){
    if (btn == GLUT_LEFT_BUTTON){
        if (state == GLUT_UP){
        }

        if (state == GLUT_DOWN){
            rayCast(x, y);
        }
    }
}

void mouseMotion(int x, int y){
}

void mousePassiveMotion(int x, int y){
}

//Camera movement function
void orbitView(float dist, float twist, float elev, float azimuth) {
	glTranslatef(0.0, 0.0, dist);
	glRotatef(-twist, 0.0, 0.0, 1.0);
	glRotatef(-elev, 1.0, 0.0, 0.0);
	glTranslatef(-camX, 0, 0);
	glTranslatef(0, 0, -camZ);
	glRotatef(azimuth, 0.0, 1.0, 0.0);
	//glTranslatef(-(float)gridsize / 2, 0, -(float)gridsize/2);
}

/* display function - GLUT display callback function
 *		clears the screen, sets the camera position
 */
void display(void){

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gluLookAt(camPos[X], camPos[Y], camPos[Z], camTarget[X], camTarget[Y], camTarget[Z], 0,1,0);

        //Camera call
	if (camDist > 1) {
		camDist = 1;
	}
	if (camElev < -85) {
		camElev = -85;
	}
	else if (camElev > 85) {
		camElev = 85;
	}
	orbitView(camDist, camTwist, camElev, angle);

	glPushMatrix();	//Push base matrix that everything else will be pushed onto
                /*
                 * Wrong place for camera transforms apparently :/
                //Camera transforms
		glRotatef(angle, 0, 1, 0);	//make rotations based on angle and Vangle
		glRotatef(Vangle, 1, 0, 0);	//now all other content in the scene will have these rotations applied
                */

                //Call the ray draw function
                drawRay();

		glPushMatrix();
			//set positions of both lights
			glLightfv(GL_LIGHT0, GL_POSITION, l0pos);
			glLightfv(GL_LIGHT1, GL_POSITION, l1pos);
		//pop matrix to ignore upcoming transformations
		glPopMatrix();

                glPushMatrix();
                    glTranslatef(0,-1.2,0); //Move the ground plane down from the origin a bit
                    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, m2_dif);
                    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, m2_amb);
                    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, m2_spe);
                    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shiny2);
                    glBegin(GL_QUADS);
                            glNormal3f(0,1,0);
                            glVertex3i(10,0,10);
                            glVertex3i(10,0,-10);
                            glVertex3i(-10,0,-10);
                            glVertex3i(-10,0,10);
                    glEnd();
		glPopMatrix();

		glPushMatrix();
			for(list<Object>::const_iterator iterator = sceneGraph.begin(); iterator != sceneGraph.end(); iterator++){
				((Object) *iterator).draw();
			}
		glPopMatrix();

		//disable lighting so spheres look flat and noticable
		glDisable(GL_LIGHTING);
		//push matrix for lights
		glPushMatrix();
				glTranslatef(l0pos[X],l0pos[Y],l0pos[Z]);
				glColor3f(0.9f,0.6f,0.4f);
				glutSolidSphere(1,10,10);
		glPopMatrix();
		glPushMatrix();
				glTranslatef(l1pos[X],l1pos[Y],l1pos[Z]);
				glColor3f(0.4f,0.6f,0.9f);
				glutSolidSphere(1,10,10);
		//pop active matrices
		glPopMatrix();
		//re-enable lighting
		glEnable(GL_LIGHTING);

	glPopMatrix();
	//swap buffers
	glutSwapBuffers();
}

//Reshape function for main window.
void reshape(int w, int h){
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	gluPerspective(45, (float)((w+0.0f)/h), 1, 1000);	//adjust perspective to keep terrain in view

	glMatrixMode(GL_MODELVIEW);
	glViewport(0, 0, w, h);			//adjust viewport to new height and width
}

//FPS controller
void FPSTimer(int value){
	glutTimerFunc(17, FPSTimer, 0);
	glutPostRedisplay();
}

//function for arrow key handling
void special(int key, int xIn, int yIn){
	switch(key){
		case GLUT_KEY_LEFT:	//Left and right arrow adjust the angle of the scene.
			angle -= 1.5f;		//This angle variable is used in a rotation around the y
			break;						//axis in the display function.
		case GLUT_KEY_RIGHT:
			angle += 1.5f;
			break;
		case GLUT_KEY_UP:		//Up and down arrow adjust V(ertical)angle, which
			//Vangle += 1.5f;		//again is used in the display function, rotating around x.
                        camElev += 1;
			break;
		case GLUT_KEY_DOWN:
                        camElev -= 1;
			//Vangle -= 1.5f;
			break;
	}
}

/* main function - program entry point */
int main(int argc, char** argv)
{
	srand(time(NULL));				//initialize randomization
	glutInit(&argc, argv);		//starts up GLUT
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);

	//MAIN WINDOW
	glutInitWindowSize(800, 400);	//setup window size and position
	glutInitWindowPosition(50, 50);
	int mainWindow = glutCreateWindow("3D Modeler");	//creates the window

	glutDisplayFunc(display);					//registers "display" as the display callback function
	glutKeyboardFunc(keyboard);				//registers "keyboard" as the keyboard callback function
	//mouse callbacks
	glutMouseFunc(mouse);
	glutMotionFunc(mouseMotion);
	glutPassiveMotionFunc(mousePassiveMotion);

	glutTimerFunc(17, FPSTimer, 0);		//registers "FPSTimer" as the timer callback function
	glutSpecialFunc(special);					//registers "special" as the special callback function
	glutReshapeFunc(reshape);					//registers "reshape" as the reshape callback function

	init();						//initialize main window

	glutMainLoop();				//starts the event glutMainLoop
	return(0);					//return may not be necessary on all compilers
}
