/*
 * Program 4 example with diffuse and spline camera PRESS 'g'
 * CPE 471 Cal Poly Z. Wood + S. Sueda + I. Dunn (spline D. McGirr)
 */

#include <iostream>
#include <numeric>
#include <glad/glad.h>

#include "GLSL.h"
#include "Program.h"
#include "Shape.h"
#include "MatrixStack.h"
#include "WindowManager.h"
#include "Texture.h"
#include "stb_image.h"
#include "Bezier.h"
#include "Spline.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader/tiny_obj_loader.h>
#define PI 3.1415927

// value_ptr for glm
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;
using namespace glm;

float midpoint(float a, float b) {
	return (a+b)/2.0f;
}
bool restart = false;

class Application : public EventCallbacks
{

public:

	WindowManager * windowManager = nullptr;

	// Our shader program - use this one for Blinn-Phong has diffuse
	std::shared_ptr<Program> prog;

	//Our shader program for textures
	std::shared_ptr<Program> texProg;

	//Our shader program for skybox
	std::shared_ptr<Program> cubeProg;

	//our geometry
	shared_ptr<Shape> cube;
	shared_ptr<Shape> tower;
	shared_ptr<Shape> fence;
	std::vector<shared_ptr<Shape> > dummy;
	std::vector<shared_ptr<Shape> > crowd;
	std::vector<shared_ptr<Shape> > trees;

	//global data for ground plane - direct load constant defined CPU data to GPU (not obj)
	GLuint GrndBuffObj, GrndNorBuffObj, GrndTexBuffObj, GIndxBuffObj;
	int g_GiboLen;
	//ground VAO
	GLuint GroundVertexArrayID;

	//the image to use as a texture (ground)
	shared_ptr<Texture> texture0;
	shared_ptr<Texture> texture3;	
	shared_ptr<Texture> texture5;
	shared_ptr<Texture> texture6;

	// for skybox
	vector<std::string> faces {
		"px.jpg",
		"nx.jpg",
		"py.jpg",
		"ny.jpg",
		"pz.jpg",
		"nz.jpg"
	};
	unsigned int cubeMapTexture;

	//animation data
	float rsTheta, reTheta, rwTheta, lsTheta, leTheta, lwTheta = 0;
	int phase = 0;
	float angles[12] = {0,0,0,0,0,0,0,0,0,0,0,0};

	mat4 rightfootMVMatrix;
	mat4 rightshoulderMVMatrix;
	mat4 leftshoulderMVMatrix;
	mat4 headMVMatrix;

	//camera
	double g_phi, g_theta;
	float camera_speed = 1.0f;
	vec3 g_eye = vec3(0, 3, 85);
	vec3 g_lookAt = vec3(0, 1, -4);
	vec3 view = normalize(g_lookAt-g_eye);
	vec3 strafe = normalize(cross(view, vec3(0,1,0)));

	//locations
	vec3 location;
	float crowdLocation[15];
	
	// find initial pitch and yaw from lookat point
	double dx = (g_lookAt-g_eye).x;
	double dy = (g_lookAt-g_eye).y;
	double dz = (g_lookAt-g_eye).z;
	double pitch = -atan2(dy,sqrt((dx*dx)+(dz*dz))); 
	double yaw = -(atan2(dz,dx) - 90);

	Spline splinepath[3];
	Spline splineWalk[10];
	Spline splineAngles[8];
	bool goCamera = false;

	bool frustrating = false;
	int numCollisions = 0;

	void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			cout << "Number of Collisions: " << numCollisions << endl;
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
		if (key == GLFW_KEY_R && action == GLFW_PRESS){
			restart = true;
		}
		if (key == GLFW_KEY_Z && action == GLFW_PRESS) {
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		if (key == GLFW_KEY_G && action == GLFW_RELEASE) {
			location = g_eye;
			goCamera = !goCamera;
		}
		if (key == GLFW_KEY_M && action == GLFW_RELEASE) {
			frustrating = !frustrating;
		}
		if (key == GLFW_KEY_A && (action == GLFW_REPEAT || action == GLFW_PRESS)){
			// move left
			if(!CheckCollision(g_eye.x-camera_speed*strafe.x,g_eye.z-camera_speed*strafe.z)) {
				g_eye.x -= camera_speed*strafe.x;
				g_eye.z -= camera_speed*strafe.z;
				g_lookAt.x -= camera_speed*strafe.x;
				g_lookAt.z -= camera_speed*strafe.z;
			} else {
				if(frustrating) {
					g_eye = vec3(0, 3, 85);
					g_lookAt = vec3(0, 1, -4);
				} else {
					g_eye.x += camera_speed*strafe.x;
					g_eye.z += camera_speed*strafe.z;
					g_lookAt.x += camera_speed*strafe.x;
					g_lookAt.z += camera_speed*strafe.z;
				}
				
			}
		}
		if (key == GLFW_KEY_D && (action == GLFW_REPEAT || action == GLFW_PRESS)){
			// move right
			if(!CheckCollision(g_eye.x+camera_speed*strafe.x,g_eye.z+camera_speed*strafe.z)) {
				g_eye.x += camera_speed*strafe.x;
				g_eye.z += camera_speed*strafe.z;
				g_lookAt.x += camera_speed*strafe.x;
				g_lookAt.z += camera_speed*strafe.z;
			} else {
				if(frustrating) {
					g_eye = vec3(0, 3, 85);
					g_lookAt = vec3(0, 1, -4);
				} else {
					g_eye.x -= camera_speed*strafe.x;
					g_eye.z -= camera_speed*strafe.z;
					g_lookAt.x -= camera_speed*strafe.x;
					g_lookAt.z -= camera_speed*strafe.z;
				}
			}
		}
		if (key == GLFW_KEY_W && (action == GLFW_REPEAT || action == GLFW_PRESS)){
			// move forward
			if(!CheckCollision(g_eye.x+camera_speed*view.x,g_eye.z+camera_speed*view.z)) {
				g_eye.x += camera_speed*view.x;
				g_eye.z += camera_speed*view.z;
				g_lookAt.x += camera_speed*view.x;
				g_lookAt.z += camera_speed*view.z;
			} else {
				if(frustrating) {
					g_eye = vec3(0, 3, 85);
					g_lookAt = vec3(0, 1, -4);
				} else {
					g_eye.x -= camera_speed*view.x;
					g_eye.z -= camera_speed*view.z;
					g_lookAt.x -= camera_speed*view.x;
					g_lookAt.z -= camera_speed*view.z;
				}
			}
		}
		if (key == GLFW_KEY_S && (action == GLFW_REPEAT || action == GLFW_PRESS)){
			// move back
			if(!CheckCollision(g_eye.x-camera_speed*view.x,g_eye.z-camera_speed*view.z)) {
				g_eye.x -= camera_speed*view.x;
				g_eye.z -= camera_speed*view.z;
				g_lookAt.x -= camera_speed*view.x;
				g_lookAt.z -= camera_speed*view.z;
			} else {
				if(frustrating) {
					g_eye = vec3(0, 3, 85);
					g_lookAt = vec3(0, 1, -4);
				} else {
					g_eye.x += camera_speed*view.x;
					g_eye.z += camera_speed*view.z;
					g_lookAt.x += camera_speed*view.x;
					g_lookAt.z += camera_speed*view.z;
				}
			}
		}
	}

	void mouseCallback(GLFWwindow *window, int button, int action, int mods)
	{
		double posX, posY;

		if (action == GLFW_PRESS)
		{
			 glfwGetCursorPos(window, &posX, &posY);
		}
	}


	void scrollCallback(GLFWwindow* window, double deltaX, double deltaY) {

		yaw += deltaX;
		pitch += deltaY;

		pitch = glm::clamp(pitch,-80.0,80.0);

		g_lookAt = g_eye + vec3(
			cos(radians(pitch))*cos(radians(-yaw)),
			sin(radians(pitch)),
			cos(radians(pitch))*cos(PI/2.0-radians(-yaw))
		);
		view = normalize(g_lookAt-g_eye);
		strafe = normalize(cross(view, vec3(0,1,0)));
	}

	void resizeCallback(GLFWwindow *window, int width, int height)
	{
		glViewport(0, 0, width, height);
	}

	void init(const std::string& resourceDirectory)
	{
		GLSL::checkVersion();

		// Set background color.
		glClearColor(.72f, .84f, 1.06f, 1.0f);
		// Enable z-buffer test.
		glEnable(GL_DEPTH_TEST);

		g_theta = -PI/2.0;

		// Initialize the GLSL program that we will use for local shading
		prog = make_shared<Program>();
		prog->setVerbose(true);
		prog->setShaderNames(resourceDirectory + "/simple_vert.glsl", resourceDirectory + "/simple_frag.glsl");
		prog->init();
		prog->addUniform("P");
		prog->addUniform("V");
		prog->addUniform("M");
		prog->addUniform("MatAmb");
		prog->addUniform("MatDif");
		prog->addUniform("MatSpec");
		prog->addUniform("MatShine");
		prog->addAttribute("vertPos");
		prog->addAttribute("vertNor");

		// Initialize the GLSL program that we will use for texture mapping
		texProg = make_shared<Program>();
		texProg->setVerbose(true);
		texProg->setShaderNames(resourceDirectory + "/tex_vert.glsl", resourceDirectory + "/tex_frag0.glsl");
		texProg->init();
		texProg->addUniform("P");
		texProg->addUniform("V");
		texProg->addUniform("M");
		texProg->addUniform("Texture0");
		texProg->addAttribute("vertPos");
		texProg->addAttribute("vertNor");
		texProg->addAttribute("vertTex");

		// GLSL program for skybox
		cubeProg = make_shared<Program>();
		cubeProg->setVerbose(true);
		cubeProg->setShaderNames(resourceDirectory + "/cube_vert.glsl", resourceDirectory + "/cube_frag.glsl");
		cubeProg->init();
		cubeProg->addUniform("P");
		cubeProg->addUniform("V");
		cubeProg->addUniform("M");
		cubeProg->addUniform("skybox");
		cubeProg->addAttribute("vertPos");
		cubeProg->addAttribute("vertNor");

		//read in a load the texture
		texture0 = make_shared<Texture>();
  		texture0->setFilename(resourceDirectory + "/grass.jpg");
  		texture0->init();
  		texture0->setUnit(0);
  		texture0->setWrapModes(GL_REPEAT, GL_REPEAT);

		texture3 = make_shared<Texture>();
  		texture3->setFilename(resourceDirectory + "/tree_X12_+X1_Rock_Pack/_4_tree.png");
  		texture3->init();
  		texture3->setUnit(0);
  		texture3->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		texture5 = make_shared<Texture>();
  		texture5->setFilename(resourceDirectory + "/tree_X12_+X1_Rock_Pack/_6_tree.png");
  		texture5->init();
  		texture5->setUnit(0);
  		texture5->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		texture6 = make_shared<Texture>();
  		texture6->setFilename(resourceDirectory + "/tree_X12_+X1_Rock_Pack/_7_tree.png");
  		texture6->init();
  		texture6->setUnit(0);
  		texture6->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

  		// init splines up and down
		splinepath[0] = Spline(glm::vec3(location.x,g_eye.y,location.z), glm::vec3(-30,g_eye.y,0), glm::vec3(-30,g_eye.y,60), glm::vec3(0,g_eye.y,60),5);
		splinepath[1] = Spline(glm::vec3(0,g_eye.y,60), glm::vec3(60,g_eye.y,60), glm::vec3(60,g_eye.y,-120), glm::vec3(0,g_eye.y,-120), 5);
		splinepath[2] = Spline(glm::vec3(0,g_eye.y,-120), glm::vec3(-60,g_eye.y,-120), glm::vec3(-60,g_eye.y,0), glm::vec3(0, g_eye.y, 85), 5);    
		
		splineWalk[0] = Spline(glm::vec3(10,-10,-20), glm::vec3(0,-10,-20), glm::vec3(-30,-10,-20),10);
		splineWalk[1] = Spline(glm::vec3(-30,-10,-20), glm::vec3(0,-10,-20), glm::vec3(10,-10,-20),10);
		splineWalk[2] = Spline(glm::vec3(30,-10,-40), glm::vec3(0,-10,-40), glm::vec3(-25,-10,-40),10);
		splineWalk[3] = Spline(glm::vec3(-25,-10,-40), glm::vec3(0,-10,-40), glm::vec3(30,-10,-40),10);
		splineWalk[4] = Spline(glm::vec3(30,-10,-60), glm::vec3(0,-10,-60), glm::vec3(-40,-10,-60),15);
		splineWalk[5] = Spline(glm::vec3(-40,-10,-60), glm::vec3(0,-10,-60), glm::vec3(30,-10,-60),15);
		splineWalk[6] = Spline(glm::vec3(40,-10,30), glm::vec3(0,-10,30), glm::vec3(-40,-10,30),10);
		splineWalk[7] = Spline(glm::vec3(-40,-10,30), glm::vec3(0,-10,30), glm::vec3(40,-10,30),10);
		splineWalk[8] = Spline(glm::vec3(50,-10,75), glm::vec3(0,-10,75), glm::vec3(-15,-10,75),10);
		splineWalk[9] = Spline(glm::vec3(-15,-10,75), glm::vec3(0,-10,75), glm::vec3(50,-10,75),10);
		
		// interpolate angles for walking animation 0->0->0, 0,3,8,11
		splineAngles[0] = Spline(glm::vec3(0,0,0), glm::vec3(0,0,0), glm::vec3(0.1*PI,0,0),1);	//1
		splineAngles[1] = Spline(glm::vec3(-0.1*PI,0,0), glm::vec3(0,0,0), glm::vec3(0.1*PI,0,0),1);	//2,6,9
		splineAngles[2] = Spline(glm::vec3(0.1*PI,0,0), glm::vec3(0,0,0), glm::vec3(0,0,0),1);			//4
		splineAngles[3] = Spline(glm::vec3(0.1*PI,0,0), glm::vec3(0,0,0), glm::vec3(-0.1*PI,0,0),1);	//5
		splineAngles[4] = Spline(glm::vec3(0,0,0), glm::vec3(0,0,0), glm::vec3(0.2*PI,0,0),1);	//7
		splineAngles[5] = Spline(glm::vec3(-0.2*PI,0,0), glm::vec3(0,0,0), glm::vec3(0,0,0),1);	//10
		splineAngles[6] = Spline(glm::vec3(0.2*PI,0,0), glm::vec3(0,0,0), glm::vec3(0,0,0),1);	//-7
		splineAngles[7] = Spline(glm::vec3(0,0,0), glm::vec3(0,0,0), glm::vec3(-0.2*PI,0,0),1);	//-10
		
	}

	void initGeom(const std::string& resourceDirectory)
	{
		//EXAMPLE set up to read one shape from one obj file - convert to read several
		// Initialize mesh
		// Load geometry
 		// Some obj files contain material information.We'll ignore them for this assignment.
 		vector<tinyobj::shape_t> TOshapes;
 		vector<tinyobj::material_t> objMaterials;
 		string errStr;
		//load in the mesh and make the shape(s)

		// cube
		bool rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr, (resourceDirectory + "/cube.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			cube = make_shared<Shape>(0);
			cube->createShape(TOshapes[0]);
			cube->measure();
			cube->init();
		}

		// tower mesh
		vector<tinyobj::shape_t> TOshapesT;
 		vector<tinyobj::material_t> objMaterialsT;
 		rc = tinyobj::LoadObj(TOshapesT, objMaterialsT, errStr, (resourceDirectory + "/EiffelTower.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			tower = make_shared<Shape>(0);
			tower->createShape(TOshapesT[0]);
			tower->measure();
			tower->init();
		}

		// dummy
		vector<tinyobj::shape_t> TOshapesD;
 		vector<tinyobj::material_t> objMaterialsD;
		rc = tinyobj::LoadObj(TOshapesD, objMaterialsD, errStr, (resourceDirectory + "/dummy.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			for (size_t s = 0; s < TOshapesD.size(); s++) {
				dummy.push_back(make_shared<Shape>(0));
				dummy[s]->createShape(TOshapesD[s]);
				dummy[s]->measure();
				dummy[s]->init();
			}
		}

		// crowd
		vector<tinyobj::shape_t> TOshapesC;
 		vector<tinyobj::material_t> objMaterialsC;
		rc = tinyobj::LoadObj(TOshapesC, objMaterialsC, errStr, (resourceDirectory + "/crowd.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			for (size_t s = 0; s < TOshapesC.size(); s++) {
				crowd.push_back(make_shared<Shape>(0));
				crowd.back()->createShape(TOshapesC[s]);
				crowd.back()->measure();
				crowd.back()->init();
			}
		}

		// trees
		vector<tinyobj::shape_t> TOshapesN;
 		vector<tinyobj::material_t> objMaterialsN;
		rc = tinyobj::LoadObj(TOshapesN, objMaterialsN, errStr, (resourceDirectory + "/tree_X12_+X1_Rock_Pack/tree_X14_+X1_Rock_Pack.obj").c_str(), (resourceDirectory + "/tree_X12_+X1_Rock_Pack/").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			for (size_t s = 0; s < TOshapesN.size(); s++) {
				trees.push_back(make_shared<Shape>(1));
				trees.back()->createShape(TOshapesN[s]);
				trees.back()->measure();
				trees.back()->init();
			}
		}

		vector<tinyobj::shape_t> TOshapesF;
 		vector<tinyobj::material_t> objMaterialsF;
 		rc = tinyobj::LoadObj(TOshapesF, objMaterialsF, errStr, (resourceDirectory + "/fence01.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			fence = make_shared<Shape>(0);
			fence->createShape(TOshapesF[0]);
			fence->measure();
			fence->init();
		}

		//code to load in the ground plane (CPU defined data passed to GPU)
		initGround();
		cubeMapTexture = createSky((resourceDirectory + "/eiffel/").c_str(), faces);
	}

	//directly pass quad for the ground to the GPU
	void initGround() {

		float g_groundSize = 150;
		float g_groundY = -10.25;

  		// A x-z plane at y = g_groundY of dimension [-g_groundSize, g_groundSize]^2
		float GrndPos[] = {
			-g_groundSize, g_groundY, -g_groundSize,
			-g_groundSize, g_groundY,  g_groundSize,
			g_groundSize, g_groundY,  g_groundSize,
			g_groundSize, g_groundY, -g_groundSize
		};

		float GrndNorm[] = {
			0, 1, 0,
			0, 1, 0,
			0, 1, 0,
			0, 1, 0,
			0, 1, 0,
			0, 1, 0
		};

		static GLfloat GrndTex[] = {
      		0, 0, // back
      		0, 50,
      		50, 50,
      		50, 0};

      	unsigned short idx[] = {0, 1, 2, 0, 2, 3};

		//generate the ground VAO
      	glGenVertexArrays(1, &GroundVertexArrayID);
      	glBindVertexArray(GroundVertexArrayID);

      	g_GiboLen = 6;
      	glGenBuffers(1, &GrndBuffObj);
      	glBindBuffer(GL_ARRAY_BUFFER, GrndBuffObj);
      	glBufferData(GL_ARRAY_BUFFER, sizeof(GrndPos), GrndPos, GL_STATIC_DRAW);

      	glGenBuffers(1, &GrndNorBuffObj);
      	glBindBuffer(GL_ARRAY_BUFFER, GrndNorBuffObj);
      	glBufferData(GL_ARRAY_BUFFER, sizeof(GrndNorm), GrndNorm, GL_STATIC_DRAW);

      	glGenBuffers(1, &GrndTexBuffObj);
      	glBindBuffer(GL_ARRAY_BUFFER, GrndTexBuffObj);
      	glBufferData(GL_ARRAY_BUFFER, sizeof(GrndTex), GrndTex, GL_STATIC_DRAW);

      	glGenBuffers(1, &GIndxBuffObj);
     	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GIndxBuffObj);
      	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
      }

      //code to draw the ground plane
     void drawGround(shared_ptr<Program> curS) {
     	curS->bind();
     	glBindVertexArray(GroundVertexArrayID);
     	texture0->bind(curS->getUniform("Texture0"));
		//draw the ground plane 
  		SetModel(vec3(0, -1, 0), 0, 0, 1, curS);
  		glEnableVertexAttribArray(0);
  		glBindBuffer(GL_ARRAY_BUFFER, GrndBuffObj);
  		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

  		glEnableVertexAttribArray(1);
  		glBindBuffer(GL_ARRAY_BUFFER, GrndNorBuffObj);
  		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

  		glEnableVertexAttribArray(2);
  		glBindBuffer(GL_ARRAY_BUFFER, GrndTexBuffObj);
  		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);

   		// draw!
  		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GIndxBuffObj);
  		glDrawElements(GL_TRIANGLES, g_GiboLen, GL_UNSIGNED_SHORT, 0);

  		glDisableVertexAttribArray(0);
  		glDisableVertexAttribArray(1);
  		glDisableVertexAttribArray(2);
  		curS->unbind();
    }

	unsigned int createSky(string dir, vector<string> faces) {
		unsigned int textureID;
		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
		int width, height, nrChannels;
		stbi_set_flip_vertically_on_load(false);
		for(GLuint i = 0; i < faces.size(); i++) {
			unsigned char *data =
			stbi_load((dir+faces[i]).c_str(), &width, &height, &nrChannels, 0);
			if (data) {
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			} else {
				cout << "failed to load: " << (dir+faces[i]).c_str() << endl;
			}
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		// cout << " creating cube map any errors : " << glGetError() << endl;
		return textureID;
	}

     //helper function to pass material data to the GPU
	void SetMaterial(shared_ptr<Program> curS, int i) {
    	switch (i) {
    		case 0: // white rubber, 2d crowd
    			glUniform3f(curS->getUniform("MatAmb"), 0.05, 0.05, 0.05);
    			glUniform3f(curS->getUniform("MatDif"), 0.5, 0.5, 0.5);
    			glUniform3f(curS->getUniform("MatSpec"), 0.7, 0.7, 0.7);
    			glUniform1f(curS->getUniform("MatShine"), 10.0);
    		break;
    		case 1: // skin color, main dummies
    			glUniform3f(curS->getUniform("MatAmb"), 0.1, 0.08, 0.05);
    			glUniform3f(curS->getUniform("MatDif"), 1.0, 0.8, 0.5);
    			glUniform3f(curS->getUniform("MatSpec"), 0, 0, 0);
    			glUniform1f(curS->getUniform("MatShine"), 0);
    		break;
			case 2: // brass, eiffel tower and fence
    			glUniform3f(curS->getUniform("MatAmb"), 0.06, 0.04, 0);
    			glUniform3f(curS->getUniform("MatDif"), 0.6, 0.4, 0);
    			glUniform3f(curS->getUniform("MatSpec"), 0.5, 0.5, 0.37);
    			glUniform1f(curS->getUniform("MatShine"), 51);
    		break;
			case 3: // bronze, used for walking dummies
				glUniform3f(curS->getUniform("MatAmb"), 0.2125, 0.1275, 0.054);
    			glUniform3f(curS->getUniform("MatDif"), 0.714, 0.4284, 0.18144);
				glUniform3f(curS->getUniform("MatSpec"), 0.393548, 0.271906, 0.166721);
    			glUniform1f(curS->getUniform("MatShine"), 0);
    		break;
		}
	}

	/* helper function to set model transforms */
  	void SetModel(vec3 trans, float rotY, float rotX, float sc, shared_ptr<Program> curS) {
  		mat4 Trans = glm::translate( glm::mat4(1.0f), trans);
  		mat4 RotX = glm::rotate( glm::mat4(1.0f), rotX, vec3(1, 0, 0));
  		mat4 RotY = glm::rotate( glm::mat4(1.0f), rotY, vec3(0, 1, 0));
  		mat4 ScaleS = glm::scale(glm::mat4(1.0f), vec3(sc));
  		mat4 ctm = Trans*RotX*RotY*ScaleS;
  		glUniformMatrix4fv(curS->getUniform("M"), 1, GL_FALSE, value_ptr(ctm));
  	}

	void setModel(std::shared_ptr<Program> prog, std::shared_ptr<MatrixStack>M) {
		glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, value_ptr(M->topMatrix()));
   	}

   	/* camera controls - do not change */
	void SetView(shared_ptr<Program>  shader) {
  		glm::mat4 Cam = glm::lookAt(g_eye, g_lookAt, vec3(0, 1, 0));
  		glUniformMatrix4fv(shader->getUniform("V"), 1, GL_FALSE, value_ptr(Cam));
	}

   	/* code to draw waving hierarchical model */
   	void drawHierWave(shared_ptr<MatrixStack> Model, shared_ptr<Program> prog, float x, float y, float z, double yaw) {
		Model->pushMatrix();
			Model->loadIdentity();
			Model->translate(vec3(x, y, z));
			Model->scale(vec3(0.07, 0.07, 0.07));
			Model->rotate(-0.5*M_PI, vec3(1, 0, 0));
			Model->rotate(-radians(-yaw), vec3(0, 0, 1));
			Model->pushMatrix();
				Model->translate(vec3(midpoint(dummy[15]->min.x,dummy[15]->max.x), midpoint(dummy[15]->min.y,dummy[15]->max.y), midpoint(dummy[15]->min.z,dummy[15]->max.z)));
				Model->rotate(-rsTheta, vec3(1, 0, 0));
				Model->translate(vec3(-midpoint(dummy[15]->min.x,dummy[15]->max.x), -midpoint(dummy[15]->min.y,dummy[15]->max.y), -midpoint(dummy[15]->min.z,dummy[15]->max.z)));
				Model->pushMatrix();
					Model->translate(vec3(midpoint(dummy[17]->min.x,dummy[17]->max.x), midpoint(dummy[17]->min.y,dummy[17]->max.y), midpoint(dummy[17]->min.z,dummy[17]->max.z)));
					Model->rotate(-reTheta, vec3(1, 0, 0));
					Model->translate(vec3(-midpoint(dummy[17]->min.x,dummy[17]->max.x), -midpoint(dummy[17]->min.y,dummy[17]->max.y), -midpoint(dummy[17]->min.z,dummy[17]->max.z)));
					Model->pushMatrix();
						Model->translate(vec3(midpoint(dummy[19]->min.x,dummy[19]->max.x), midpoint(dummy[19]->min.y,dummy[19]->max.y), midpoint(dummy[19]->min.z,dummy[19]->max.z)));
						Model->rotate(-rwTheta, vec3(1, 0, 0));
						Model->rotate(-0.5*PI, vec3(0, 1, 0));
						Model->translate(vec3(-midpoint(dummy[19]->min.x,dummy[19]->max.x), -midpoint(dummy[19]->min.y,dummy[19]->max.y), -midpoint(dummy[19]->min.z,dummy[19]->max.z)));
						setModel(prog, Model);
						dummy[19]->draw(prog);
						dummy[20]->draw(prog);
					Model->popMatrix();
					setModel(prog, Model);
					dummy[17]->draw(prog);
					dummy[18]->draw(prog);
				Model->popMatrix();
				setModel(prog, Model);
				dummy[15]->draw(prog);
				dummy[16]->draw(prog);
			Model->popMatrix();
			Model->pushMatrix();
				Model->translate(vec3(midpoint(dummy[21]->min.x,dummy[21]->max.x), midpoint(dummy[21]->min.y,dummy[21]->max.y), midpoint(dummy[21]->min.z,dummy[21]->max.z)));
				Model->rotate(-lsTheta, vec3(1, 0, 0));
				Model->translate(vec3(-midpoint(dummy[21]->min.x,dummy[21]->max.x), -midpoint(dummy[21]->min.y,dummy[21]->max.y), -midpoint(dummy[21]->min.z,dummy[21]->max.z)));
				Model->pushMatrix();
					Model->translate(vec3(midpoint(dummy[23]->min.x,dummy[23]->max.x), midpoint(dummy[23]->min.y,dummy[23]->max.y), midpoint(dummy[23]->min.z,dummy[23]->max.z)));
					Model->rotate(-leTheta, vec3(1, 0, 0));
					Model->translate(vec3(-midpoint(dummy[23]->min.x,dummy[23]->max.x), -midpoint(dummy[23]->min.y,dummy[23]->max.y), -midpoint(dummy[23]->min.z,dummy[23]->max.z)));
					Model->pushMatrix();
						Model->translate(vec3(midpoint(dummy[25]->min.x,dummy[25]->max.x), midpoint(dummy[25]->min.y,dummy[25]->max.y), midpoint(dummy[25]->min.z,dummy[25]->max.z)));
						Model->rotate(-lwTheta, vec3(1, 0, 0));
						Model->translate(vec3(-midpoint(dummy[25]->min.x,dummy[25]->max.x), -midpoint(dummy[25]->min.y,dummy[25]->max.y), -midpoint(dummy[25]->min.z,dummy[25]->max.z)));
						setModel(prog, Model);
						dummy[25]->draw(prog);
						dummy[26]->draw(prog);
					Model->popMatrix();
					setModel(prog, Model);
					dummy[23]->draw(prog);
					dummy[24]->draw(prog);
				Model->popMatrix();
				setModel(prog, Model);
				dummy[21]->draw(prog);
				dummy[22]->draw(prog);
			Model->popMatrix();
			setModel(prog, Model);
			for(int i = 0; i<=14; i++) dummy[i]->draw(prog);
			for(int i = 27; i<=28; i++) dummy[i]->draw(prog);
		Model->popMatrix();
   	}

	// code for hierarchical walking animation
	void drawHierWalk(shared_ptr<MatrixStack> Model, shared_ptr<Program> prog, float x, float y, float z, float angles[], double yaw, bool player) {
		Model->pushMatrix();
			Model->loadIdentity();
			Model->translate(vec3(x, y, z));
			Model->scale(vec3(0.07, 0.07, 0.07));
			Model->rotate(-0.5*M_PI, vec3(1, 0, 0));
			Model->rotate(-radians(-yaw), vec3(0, 0, 1));
			drawRightArm(Model, prog, angles, player);
			drawLeftArm(Model, prog, angles, player);
			drawRightLeg(Model, prog, angles, player);
			drawLeftLeg(Model, prog, angles);
			setModel(prog, Model);
			for(int i = 12; i<=14; i++) dummy[i]->draw(prog);
			for(int i = 27; i<=28; i++) dummy[i]->draw(prog);
		Model->popMatrix();
	}
	void drawRightArm(shared_ptr<MatrixStack> Model, shared_ptr<Program> prog, float angles[], bool player) {
		Model->pushMatrix();
			Model->translate(vec3(midpoint(dummy[15]->min.x,dummy[15]->max.x), midpoint(dummy[15]->min.y,dummy[15]->max.y), midpoint(dummy[15]->min.z,dummy[15]->max.z)));
			Model->rotate(0.5*M_PI, vec3(1, 0, 0));
			Model->rotate(angles[6], vec3(0, 0, 1));
			Model->translate(vec3(-midpoint(dummy[15]->min.x,dummy[15]->max.x), -midpoint(dummy[15]->min.y,dummy[15]->max.y), -midpoint(dummy[15]->min.z,dummy[15]->max.z)));
			Model->pushMatrix();
				Model->translate(vec3(midpoint(dummy[17]->min.x,dummy[17]->max.x), midpoint(dummy[17]->min.y,dummy[17]->max.y), midpoint(dummy[17]->min.z,dummy[17]->max.z)));
				Model->rotate(angles[7], vec3(0, 0, 1));
				Model->translate(vec3(-midpoint(dummy[17]->min.x,dummy[17]->max.x), -midpoint(dummy[17]->min.y,dummy[17]->max.y), -midpoint(dummy[17]->min.z,dummy[17]->max.z)));
				Model->pushMatrix();
					Model->translate(vec3(midpoint(dummy[19]->min.x,dummy[19]->max.x), midpoint(dummy[19]->min.y,dummy[19]->max.y), midpoint(dummy[19]->min.z,dummy[19]->max.z)));
					Model->rotate(angles[8], vec3(0, 0, 1));
					Model->translate(vec3(-midpoint(dummy[19]->min.x,dummy[19]->max.x), -midpoint(dummy[19]->min.y,dummy[19]->max.y), -midpoint(dummy[19]->min.z,dummy[19]->max.z)));
					setModel(prog, Model);
					dummy[19]->draw(prog);
					dummy[20]->draw(prog);
				Model->popMatrix();
				setModel(prog, Model);
				dummy[17]->draw(prog);
				dummy[18]->draw(prog);
			Model->popMatrix();
			setModel(prog, Model);
			dummy[15]->draw(prog);
			dummy[16]->draw(prog);
		Model->popMatrix();
	}
	void drawLeftArm(shared_ptr<MatrixStack> Model, shared_ptr<Program> prog, float angles[], bool player) {
		Model->pushMatrix();
			Model->translate(vec3(midpoint(dummy[21]->min.x,dummy[21]->max.x), midpoint(dummy[21]->min.y,dummy[21]->max.y), midpoint(dummy[21]->min.z,dummy[21]->max.z)));
			Model->rotate(-0.5*M_PI, vec3(1, 0, 0));
			Model->rotate(angles[9], vec3(0, 0, 1));
			Model->translate(vec3(-midpoint(dummy[21]->min.x,dummy[21]->max.x), -midpoint(dummy[21]->min.y,dummy[21]->max.y), -midpoint(dummy[21]->min.z,dummy[21]->max.z)));
			Model->pushMatrix();
				Model->translate(vec3(midpoint(dummy[23]->min.x,dummy[23]->max.x), midpoint(dummy[23]->min.y,dummy[23]->max.y), midpoint(dummy[23]->min.z,dummy[23]->max.z)));
				Model->rotate(angles[10], vec3(0, 0, 1));
				Model->translate(vec3(-midpoint(dummy[23]->min.x,dummy[23]->max.x), -midpoint(dummy[23]->min.y,dummy[23]->max.y), -midpoint(dummy[23]->min.z,dummy[23]->max.z)));
				Model->pushMatrix();
					Model->translate(vec3(midpoint(dummy[25]->min.x,dummy[25]->max.x), midpoint(dummy[25]->min.y,dummy[25]->max.y), midpoint(dummy[25]->min.z,dummy[25]->max.z)));
					Model->rotate(angles[11], vec3(0, 0, 1));
					Model->translate(vec3(-midpoint(dummy[25]->min.x,dummy[25]->max.x), -midpoint(dummy[25]->min.y,dummy[25]->max.y), -midpoint(dummy[25]->min.z,dummy[25]->max.z)));
					setModel(prog, Model);
					dummy[25]->draw(prog);
					dummy[26]->draw(prog);
				Model->popMatrix();
				setModel(prog, Model);
				dummy[23]->draw(prog);
				dummy[24]->draw(prog);
			Model->popMatrix();
			setModel(prog, Model);
			dummy[21]->draw(prog);
			dummy[22]->draw(prog);
		Model->popMatrix();
	}
	void drawRightLeg(shared_ptr<MatrixStack> Model, shared_ptr<Program> prog, float angles[], bool player) {
		Model->pushMatrix();
			Model->translate(vec3(midpoint(dummy[5]->min.x,dummy[5]->max.x), midpoint(dummy[5]->min.y,dummy[5]->max.y), midpoint(dummy[5]->min.z,dummy[5]->max.z)));
			Model->rotate(angles[2], vec3(0, 1, 0));
			Model->translate(vec3(-midpoint(dummy[5]->min.x,dummy[5]->max.x), -midpoint(dummy[5]->min.y,dummy[5]->max.y), -midpoint(dummy[5]->min.z,dummy[5]->max.z)));
			Model->pushMatrix();
				Model->translate(vec3(midpoint(dummy[3]->min.x,dummy[3]->max.x), midpoint(dummy[3]->min.y,dummy[3]->max.y), midpoint(dummy[3]->min.z,dummy[3]->max.z)));
				Model->rotate(angles[1], vec3(0, 1, 0));
				Model->translate(vec3(-midpoint(dummy[3]->min.x,dummy[3]->max.x), -midpoint(dummy[3]->min.y,dummy[3]->max.y), -midpoint(dummy[3]->min.z,dummy[3]->max.z)));
				Model->pushMatrix();
					Model->translate(vec3(midpoint(dummy[1]->min.x,dummy[1]->max.x), midpoint(dummy[1]->min.y,dummy[1]->max.y), midpoint(dummy[1]->min.z,dummy[1]->max.z)));
					Model->rotate(angles[0], vec3(0, 1, 0));
					Model->translate(vec3(-midpoint(dummy[1]->min.x,dummy[1]->max.x), -midpoint(dummy[1]->min.y,dummy[1]->max.y), -midpoint(dummy[1]->min.z,dummy[1]->max.z)));
					setModel(prog, Model);
					dummy[0]->draw(prog);
					dummy[1]->draw(prog);
				Model->popMatrix();
				setModel(prog, Model);
				dummy[2]->draw(prog);
				dummy[3]->draw(prog);
			Model->popMatrix();
			setModel(prog, Model);
			dummy[4]->draw(prog);
			dummy[5]->draw(prog);
		Model->popMatrix();
	}
	void drawLeftLeg(shared_ptr<MatrixStack> Model, shared_ptr<Program> prog, float angles[]) {
		Model->pushMatrix();
			Model->translate(vec3(midpoint(dummy[11]->min.x,dummy[11]->max.x), midpoint(dummy[11]->min.y,dummy[11]->max.y), midpoint(dummy[11]->min.z,dummy[11]->max.z)));
			Model->rotate(angles[5], vec3(0, 1, 0));
			Model->translate(vec3(-midpoint(dummy[11]->min.x,dummy[11]->max.x), -midpoint(dummy[11]->min.y,dummy[11]->max.y), -midpoint(dummy[11]->min.z,dummy[11]->max.z)));
			Model->pushMatrix();
				Model->translate(vec3(midpoint(dummy[9]->min.x,dummy[9]->max.x), midpoint(dummy[9]->min.y,dummy[9]->max.y), midpoint(dummy[9]->min.z,dummy[9]->max.z)));
				Model->rotate(angles[4], vec3(0, 1, 0));
				Model->translate(vec3(-midpoint(dummy[9]->min.x,dummy[9]->max.x), -midpoint(dummy[9]->min.y,dummy[9]->max.y), -midpoint(dummy[9]->min.z,dummy[9]->max.z)));
				Model->pushMatrix();
					Model->translate(vec3(midpoint(dummy[7]->min.x,dummy[7]->max.x), midpoint(dummy[7]->min.y,dummy[7]->max.y), midpoint(dummy[7]->min.z,dummy[7]->max.z)));
					Model->rotate(angles[3], vec3(0, 1, 0));
					Model->translate(vec3(-midpoint(dummy[7]->min.x,dummy[7]->max.x), -midpoint(dummy[7]->min.y,dummy[7]->max.y), -midpoint(dummy[7]->min.z,dummy[7]->max.z)));
					setModel(prog, Model);
					dummy[6]->draw(prog);
					dummy[7]->draw(prog);
				Model->popMatrix();
				setModel(prog, Model);
				dummy[8]->draw(prog);
				dummy[9]->draw(prog);
			Model->popMatrix();
			setModel(prog, Model);
			dummy[10]->draw(prog);
			dummy[11]->draw(prog);
		Model->popMatrix();
	}

	// code for walking dummies, using spline code for paths
	void drawWalkingCrowd(shared_ptr<MatrixStack> Model, shared_ptr<Program> prog, int splineWalk1, int splineWalk2, float z, int inOrder, int locationInd, float frametime) {
		double walkAngle;
		float cx;
		if(inOrder) {
			if(!splineWalk[splineWalk1].isDone()){
				splineWalk[splineWalk1].update(frametime);
				cx = splineWalk[splineWalk1].getPosition().x;
				walkAngle = 180;
			} else {
				splineWalk[splineWalk2].update(frametime);
				cx = splineWalk[splineWalk2].getPosition().x;
				walkAngle = 0;
				if(splineWalk[splineWalk2].isDone()) {
					splineWalk[splineWalk1].reset();
					splineWalk[splineWalk2].reset();
				}
			}
		} else {
			if(!splineWalk[splineWalk2].isDone()){
				splineWalk[splineWalk2].update(frametime);
				cx = splineWalk[splineWalk2].getPosition().x;
				walkAngle = 0;
			} else {
				splineWalk[splineWalk1].update(frametime);
				cx = splineWalk[splineWalk1].getPosition().x;
				walkAngle = 180;
				if(splineWalk[splineWalk1].isDone()) {
					splineWalk[splineWalk1].reset();
					splineWalk[splineWalk2].reset();
				}
			}
		}
		crowdLocation[locationInd] = cx;
		crowdLocation[locationInd+1] = -10;
		crowdLocation[locationInd+2] = z;
		drawHierWalk(Model,prog,cx,-10,z,angles,walkAngle,false);
	}

	void drawFullCrowd(shared_ptr<MatrixStack> Model, shared_ptr<Program> prog, float x, float y, float z, float angle) {
		Model->pushMatrix();
			Model->loadIdentity();
			Model->translate(vec3(x, y, z));
			Model->scale(vec3(0.006, 0.006, 0.006));
			Model->rotate(angle, vec3(0, 1, 0));
			setModel(prog, Model);
			for (size_t s = 0; s < crowd.size(); s++) {
				crowd[s]->draw(prog);
			}
		Model->popMatrix();
	}

	void drawFence(shared_ptr<MatrixStack> Model, shared_ptr<Program> prog, float x, float y, float z, float angle) {
		Model->pushMatrix();		
			Model->loadIdentity();
			Model->translate(vec3(x, y, z));
			Model->rotate(angle, vec3(0, 1, 0));
			Model->scale(vec3(0.05, 0.05, 0.05));
			Model->translate(vec3(-midpoint(fence->min.x,fence->max.x), -fence->min.y, -midpoint(fence->min.z,fence->max.z)));
			setModel(prog, Model);
			fence->draw(prog);
		Model->popMatrix();
	}

   	void updateUsingCameraPath(float frametime)  {
   	  if (goCamera) {
       if(!splinepath[0].isDone()){
       		splinepath[0].update(frametime);
            g_eye = splinepath[0].getPosition();
        } else if(!splinepath[1].isDone()){
			splinepath[1].update(frametime);
            g_eye = splinepath[1].getPosition();
        } else if(!splinepath[2].isDone()){
			splinepath[2].update(frametime);
            g_eye = splinepath[2].getPosition();
		} else {
			restart = true;
		}
      }
   	}

	bool CheckCollision(float x, float z) {
		float trees[] = {
			40,104,5,6,
			-45,100,5,5,
			35,83.5,4,6.5,
			-30,74,5,6,
			25,65,5,5,
			-50,68.5,4,6.5,
			-30,49,5,6,
			-50,40,5,5,
			40,28.5,4,6.5,
			25,14,5,6,
			-30,10,5,5,
			40,-6.5,4,6.5,
			-45,-16,5,6,
			35,-20,5,5,
			-30,-41.5,4,6.5,
			25,-51,5,6,
			-50,-55,5,5,
			40,-71.5,4,6.5
		};		// center x,z, size x,z
		float crowds[] = {
			10,20,4,2.5,
			-5,10,3.5,2.5,
			15,0,3,2,
			-20,-10,1.75,1,
			-5,-35,1.75,1.25,
			0,-45,1.5,1,
			-25,-55,1.5,1,
			15,-65,2.75,1.75
		};

		for(int i=0; i<72; i+=4) {
			// get difference vector between both centers
			vec2 difference = vec2(x,z) - vec2(trees[i],trees[i+1]);
			vec2 clamped = clamp(difference, -vec2(trees[i+2],trees[i+3]), vec2(trees[i+2],trees[i+3]));
			// add clamped value to AABB_center and we get the value of box closest to circle
			vec2 closest = vec2(trees[i],trees[i+1]) + clamped;
			// retrieve vector between center circle and closest point AABB and check if length <= radius
			difference = closest - vec2(x,z);
			if(length(difference) < 1.5) {
				numCollisions++;
				return true;
			}
		}
		for(int j=0; j<2; j++) {
			for(int i=0; i<32; i+=4) {
				// get difference vector between both centers
				vec2 difference = vec2(x,z) - vec2(crowds[i],-20+crowds[i+1]+65*j);
				vec2 clamped = clamp(difference, -vec2(crowds[i+2],crowds[i+3]), vec2(crowds[i+2],crowds[i+3]));
				// add clamped value to AABB_center and we get the value of box closest to circle
				vec2 closest = vec2(crowds[i],-20+crowds[i+1]+65*j) + clamped;
				// retrieve vector between center circle and closest point AABB and check if length <= radius
				difference = closest - vec2(x,z);
				if(length(difference) < 1.5) {
					numCollisions++;
					return true;
				}
			}
		}
		for(int i=0; i<15; i+=3) {
			// get difference vector between both centers
			vec2 difference = vec2(x,z) - vec2(crowdLocation[i],crowdLocation[i+2]);
			vec2 clamped = clamp(difference, -vec2(1.5,1.5), vec2(1.5,1.5));
			// add clamped value to AABB_center and we get the value of box closest to circle
			vec2 closest = vec2(crowdLocation[i],crowdLocation[i+2]) + clamped;
			// retrieve vector between center circle and closest point AABB and check if length <= radius
			difference = closest - vec2(x,z);
			if(length(difference) < 1.5) {
				numCollisions++;
				return true;
			}	
		}
		if(z>=85 || z<=-115 || x>=52 || x<=-52) {
			numCollisions++;
			return true;
		}
		vec2 difference = vec2(x,z) - vec2(0,-110);
		vec2 clamped = clamp(difference, -vec2(5,5), vec2(5,5));
		// add clamped value to AABB_center and we get the value of box closest to circle
		vec2 closest = vec2(0,-110) + clamped;
		// retrieve vector between center circle and closest point AABB and check if length <= radius
		difference = closest - vec2(x,z);
		if(length(difference) < 5) {
			location = g_eye;
			goCamera = true;
			return true;
		}
		return false;
	}

	void render(float frametime) {
		// Get current frame buffer size.
		int width, height;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		glViewport(0, 0, width, height);

		// Clear framebuffer.
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Use the matrix stack for Lab 6
		float aspect = width/(float)height;

		// Create the matrix stacks - please leave these alone for now
		auto Projection = make_shared<MatrixStack>();
		auto Model = make_shared<MatrixStack>();

		//update the camera position
		updateUsingCameraPath(frametime);

		// Apply perspective projection.
		Projection->pushMatrix();
		Projection->perspective(45.0f, aspect, 0.01f, 1000.0f);

		//to draw the sky box bind the right shader
		cubeProg->bind();
		//set the projection matrix - can use the same one
		glUniformMatrix4fv(cubeProg->getUniform("P"), 1, GL_FALSE,value_ptr(Projection->topMatrix()));
		//set the depth function to always draw the box!
		glDepthFunc(GL_LEQUAL);
		//set up view matrix to include your view transforms
		SetView(cubeProg);
		//set and send model transforms - likely want a bigger cube
		SetModel(vec3(0, 0, 0), 0, 0, 1000, cubeProg);
		//bind the cube map texture
		glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapTexture);
		//draw the actual cube
		cube->draw(cubeProg);
		//set the depth test back to normal!
		glDepthFunc(GL_LESS);
		//unbind the shader for the skybox
		cubeProg->unbind();

		// Draw the scene
		prog->bind();
		glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
		SetView(prog);

		// draw tower
		Model->pushMatrix();
			Model->loadIdentity();
			Model->translate(vec3(0, -11, -200));
			Model->scale(vec3(5, 5, 5));
			SetMaterial(prog, 2);
			setModel(prog, Model);
			tower->draw(prog);
		Model->popMatrix();

		// fence width: 0.7 each side, total length: 11.3872
		SetMaterial(prog, 2);
		float fenceX = -55;
		float fenceZ = 11.3872;
		for(int i=0; i<11; i++) {
			drawFence(Model, prog, fenceX,-10,fenceZ*i,0.5*M_PI);
			drawFence(Model, prog, fenceX,-10,-fenceZ*i,0.5*M_PI);
			drawFence(Model, prog, -fenceX,-10,fenceZ*i,0.5*M_PI);
			drawFence(Model, prog, -fenceX,-10,-fenceZ*i,0.5*M_PI);
		}
		drawFence(Model, prog, fenceX,-10,fenceZ*11,0.5*M_PI);
		drawFence(Model, prog, -fenceX,-10,fenceZ*11,0.5*M_PI);
		fenceX = 11.3872;
		fenceZ = -11.3872*10.5;
		for(int i=0; i<4; i++) {
			drawFence(Model, prog, -55+11.3872/2+fenceX*i,-10,fenceZ,0);
			drawFence(Model, prog, 55-11.3872/2-fenceX*i,-10,fenceZ,0);
		}

		SetMaterial(prog, 1);

		// person 2
		drawHierWave(Model,prog,0,-10,-110,270);

		float cx,cy,cz;
		if(!goCamera) {
			cx = g_eye.x;
			cy = g_eye.y-13;
			cz = g_eye.z;
			drawHierWalk(Model,prog,cx,cy,cz,angles,yaw,true);
		} else {
			cx = location.x;
			cy = location.y-13;
			cz = location.z;
			drawHierWave(Model,prog,cx,cy,cz,yaw);
		}
		
		// walking boundaries
		SetMaterial(prog, 3);
		drawWalkingCrowd(Model,prog,0,1,-20,0,0,frametime);
		drawWalkingCrowd(Model,prog,2,3,-40,1,3,frametime);
		drawWalkingCrowd(Model,prog,4,5,-60,0,6,frametime);
		drawWalkingCrowd(Model,prog,6,7,30,1,9,frametime);
		drawWalkingCrowd(Model,prog,8,9,75,0,12,frametime);

		// boundary crowds
		SetMaterial(prog, 0);
		drawFullCrowd(Model,prog,0,-10,115,0);
		for(int i=0; i<5; i++) {
			drawFullCrowd(Model,prog,90,-10,-100+50*i,-0.5*M_PI);
			drawFullCrowd(Model,prog,-90,-10,-100+50*i,1.5*M_PI);
		}

		// crowds inside fence
		Model->pushMatrix();
			int x1[] = {10, -5, 15, -20, -5, 0, -25, 15};
			int z1[] = {20, 10, 0, -10, -35, -45, -55, -65};
			Model->loadIdentity();
			for(int i=0; i<2; i++) {
				for (size_t s = 0; s < 8; s++) {
					Model->pushMatrix();
					Model->translate(vec3(x1[s], -10, -20+z1[s]+65*i));
					Model->scale(vec3(0.007, 0.007, 0.007));
					Model->translate(vec3(-midpoint(crowd[s]->min.x,crowd[s]->max.x), -crowd[s]->min.y, -midpoint(crowd[s]->min.z,crowd[s]->max.z)));
					setModel(prog, Model);
					crowd[s]->draw(prog);
					Model->popMatrix();
				}
			}
		Model->popMatrix();

		prog->unbind();

		//switch shaders to the texture mapping shader and draw the ground
		texProg->bind();
		glUniformMatrix4fv(texProg->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
		SetView(texProg);
		glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
		
		// trees
		Model->pushMatrix();
			Model->loadIdentity();
			int x[] = {40, -45, 35, -30, 25, -50, -30, -50, 40, 25, -30, 40, -45, 35, -30, 25, -50, 40};
			int z[] = {110, 100, 90, 80, 65, 75, 55, 40, 35, 20, 10, 0, -10, -20, -35, -45, -55, -65};
			for(int i=0; i<6; i++) {
				texture5->bind(texProg->getUniform("Texture0"));		// bounding box: -5 x 5, -10 z 1
				Model->pushMatrix();
				Model->translate(vec3(x[3*i], -10, z[3*i]));
				Model->scale(vec3(15, 15, 15));
				Model->translate(vec3(-midpoint(trees[5]->min.x,trees[5]->max.x), -trees[5]->min.y, -midpoint(trees[5]->min.z,trees[5]->max.z)));
				setModel(texProg, Model);
				trees[5]->draw(texProg);		// scale 15
				Model->popMatrix();
			}
			for(int i=0; i<6; i++) {
				texture6->bind(texProg->getUniform("Texture0"));		// bounding: 5 unit radius
				Model->pushMatrix();
				Model->translate(vec3(x[3*i+1], -10, z[3*i+1]));
				Model->scale(vec3(10, 10, 10));
				Model->translate(vec3(-midpoint(trees[6]->min.x,trees[6]->max.x), -trees[6]->min.y, -midpoint(trees[6]->min.z,trees[6]->max.z)));
				setModel(texProg, Model);
				trees[6]->draw(texProg);
				Model->popMatrix();
			}
			for(int i=0; i<6; i++) {
				texture3->bind(texProg->getUniform("Texture0"));		// bounding box: -4 x 4, -9 z 4
				Model->pushMatrix();
				Model->translate(vec3(x[3*i+2], -10, z[3*i+2]));
				Model->scale(vec3(20, 20, 20));
				Model->translate(vec3(-midpoint(trees[3]->min.x,trees[3]->max.x), -trees[3]->min.y, -midpoint(trees[3]->min.z,trees[3]->max.z)));
				setModel(texProg, Model);
				trees[3]->draw(texProg);
				Model->popMatrix();
			}
		Model->popMatrix();

		drawGround(texProg);

		texProg->unbind();

		// Pop matrix stacks.
		Projection->popMatrix();

		// animation
		// waving right arm
		rsTheta = sin(glfwGetTime());		// shoulder
		reTheta = abs(rsTheta)+0.7*rsTheta;	// elbow
		rwTheta = abs(rsTheta*0.5);			// wrist
		// mostly stationary left arm
		lsTheta = 0.85;						// shoulder, stationary
		leTheta = 1.8;		// elbow, slight swaying
		lwTheta = -1.0;			// wrist, slight swaying

		// right leg, ankle(0), knee(1), pelvis(2)
		// left leg, ankle(3), knee(4), pelvis(5)
		// right arm, shoulder(6), elbow(7), wrist(8)
		// left arm, shoulder(9), elbow(10), wrist(11)
		if(splineAngles[0].isDone()){
			for(int i=0; i<8; i++) {
				splineAngles[i].reset();
			}
			phase = !phase;
		}
		for(int i=0; i<8; i++) {
			splineAngles[i].update(frametime);
		}
		if(phase) {
			angles[1] = splineAngles[0].getPosition().x;
			angles[2] = splineAngles[1].getPosition().x;
			angles[4] = splineAngles[2].getPosition().x;
			angles[5] = splineAngles[3].getPosition().x;
			angles[6] = splineAngles[1].getPosition().x;
			angles[7] = splineAngles[4].getPosition().x;
			angles[9] = splineAngles[1].getPosition().x;
			angles[10] = splineAngles[5].getPosition().x;
		} else {
			angles[1] = splineAngles[2].getPosition().x;
			angles[2] = splineAngles[3].getPosition().x;
			angles[4] = splineAngles[0].getPosition().x;
			angles[5] = splineAngles[1].getPosition().x;
			angles[6] = splineAngles[3].getPosition().x;
			angles[7] = splineAngles[6].getPosition().x;
			angles[9] = splineAngles[3].getPosition().x;
			angles[10] = splineAngles[7].getPosition().x;
		}
	}
};

int main(int argc, char *argv[])
{
	// Where the resources are loaded from
	std::string resourceDir = "../resources";

	if (argc >= 2)
	{
		resourceDir = argv[1];
	}

	Application *application = new Application();

	// Your main will always include a similar set up to establish your window
	// and GL context, etc.

	WindowManager *windowManager = new WindowManager();
	windowManager->init(640, 480);
	windowManager->setEventCallbacks(application);
	application->windowManager = windowManager;

	// This is the code that will likely change program to program as you
	// may need to initialize or set up different data and state

	application->init(resourceDir);
	application->initGeom(resourceDir);

	auto lastTime = chrono::high_resolution_clock::now();
	// Loop until the user closes the window.
	while (! glfwWindowShouldClose(windowManager->getHandle()))
	{
		if(restart) {
			delete application;
			application = new Application();
			windowManager->setEventCallbacks(application);
			application->windowManager = windowManager;
			application->init(resourceDir);
			application->initGeom(resourceDir);

			auto lastTime = chrono::high_resolution_clock::now();
			restart = false;
		}
		// save current time for next frame
		auto nextLastTime = chrono::high_resolution_clock::now();

		// get time since last frame
		float deltaTime =
			chrono::duration_cast<std::chrono::microseconds>(
				chrono::high_resolution_clock::now() - lastTime)
				.count();
		// convert microseconds (weird) to seconds (less weird)
		deltaTime *= 0.000001;

		// reset lastTime so that we can calculate the deltaTime
		// on the next frame
		lastTime = nextLastTime;

		// Render scene.
		application->render(deltaTime);
		// Swap front and back buffers.
		glfwSwapBuffers(windowManager->getHandle());
		// Poll for and process events.
		glfwPollEvents();
	}

	// Quit program.
	windowManager->shutdown();
	return 0;
}
