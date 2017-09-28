#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include <string>
#include <vector>
#include <iomanip>
#include <iostream>
#include <fstream>

#define M_PI 3.141592654f

unsigned int g_windowWidth = 600;
unsigned int g_windowHeight = 600;
char* g_windowName = "HW2-Transform-Coding-Image";

#define IMAGE_FILE "data/cameraman.ppm"

GLFWwindow* g_window;

int g_image_width;
int g_image_height;

using namespace std;

std::vector<float> g_luminance_data;
std::vector<float> g_compressed_luminance_data;
std::vector<float> g_haar_compressed_luminance_data;

struct color
{
	unsigned char r, g, b;
};

int g_draw_origin = 1;

void printVector(float* a, int size)
{
	cout << "<";
	for (int i = 0; i < size; i++)
	{
		cout << a[i] << ", ";
	}
	cout << ">" << endl;
}
void printMatrix(const float* a, int width, int height)
{
	for (int h = 0; h < height; h++)
	{
		cout << "[";
		for (int w = 0; w < width; w++)
		{
			cout << setw(8) << setfill(' ') << setprecision(4)
				<< a[h * width + w]; 
			if (w < width - 1)
				cout << ", ";
		}
		cout << "]" << endl;
	}

}

float dotProduct(const float* a, const float* b, int size)
{
	float sum = 0;
	for (int i = 0; i < size; i++)
	{
		sum += a[i] * b[i];
	}
	return sum;
}

float* transpose(float* a, int width, int height)
{
	float *a_t = new float[8 * 8];
	for (int h = 0; h < height; h++)
	{
		for (int w = 0; w < width; w++)
		{
			a_t[w * height + h] = a[h * width + w];
		}
	}
	return a_t;
}

void normalize(float* a, int size)
{
	float len = 0;
	for (int i = 0; i < size; i++)
	{
		len += a[i] * a[i];
	}
	len = sqrt(len);
	for (int i = 0; i < size; i++)
	{
		a[i] = a[i] / len;
	}
}

void normalizeMatrix(float* &A, int width, int height)
{
	A = transpose(A, width, height);
	for (int i = 0; i < height; i++)
	{
		normalize(A + height * i, width);
	}
	A = transpose(A, width, height);
}

float* multiply(const float* a, float* b, int width, int height)
{
	float *mat = new float[width * height];
	b = transpose(b, 8, 8);

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			float dot = dotProduct(a + i * height, b + j * height, 8);

			if (abs(dot) < 0.0001)
				dot = 0.0f;

			mat[i * height + j] = dot;
		}
	}
	return mat;
}

void DCT(const float* imgBlockMat, float* dctMat, int u, int v, int m)
{
	// Zero out value
	dctMat[v * 8 + u] = 0;

	for (int y = 0; y < 8; y++)
	{
		for (int x = 0; x < 8; x++)
		{
			float c_u = 1.0f, c_v = 1.0f;

			if (u == 0)
				c_u = 1.0f / sqrt(2.0f);

			if (v == 0)
				c_v = 1.0f / sqrt(2.0f);

			if (u + v <= m || m == 0)
				dctMat[v * 8 + u] += 0.25f * c_u * c_v * imgBlockMat[y * 8 + x] * cos((u * M_PI) * (2.0f * x + 1.0f) / 16.0f) * cos((v * M_PI) * (2.0f * y + 1.0f) / 16.0f);
		}
	}

}
void IDCT(float* imgBlockMat, const float* dctMat, int x, int y)
{
	imgBlockMat[y * 8 + x] = 0;

	for (int v = 0; v < 8; v++)
	{
		for (int u = 0; u < 8; u++)
		{
			float c_u = 1.0f, c_v = 1.0f;

			if (u == 0)
				c_u = 1.0f / sqrt(2.0f);

			if (v == 0)
				c_v = 1.0f / sqrt(2.0f);

			imgBlockMat[y * 8 + x] += 0.25f * c_u * c_v * dctMat[v * 8 + u] * cos((u * M_PI) * (2.0f * x + 1) / 16.0f) * cos((v * M_PI) * (2.0f * y + 1.0f) / 16.0f);
		}

	}

}
void HaarCompress(float* imgBlockMat, float ratio)
{
	for (int v = 0; v < 8; v++)
	{
		for (int u = 0; u < 8; u++)
		{
			if (abs(imgBlockMat[v * 8 + u]) <= ratio)
				imgBlockMat[v * 8 + u] = 0.0f;
		}
	}
}

void CompressBlockHaar(const float* A, float* &B, float compressionRatio)
{

	//cout << "H: " << endl;
	// Create standard Haar matrix
	float *H = new float[8 * 8]
	{	0.125f,  0.125f,  0.25f,   0.0f,  0.5f,   0.0f,   0.0f,  0.0f,
		0.125f,  0.125f,  0.25f,   0.0f, -0.5f,   0.0f,   0.0f,  0.0f,
		0.125f,  0.125f, -0.25f,   0.0f,  0.0f,   0.5f,   0.0f,	 0.0f,
		0.125f,  0.125f, -0.25f,   0.0f,  0.0f,  -0.5f,   0.0f,  0.0f,
		0.125f, -0.125f,   0.0f,  0.25f,  0.0f,   0.0f,   0.5f,  0.0f,
		0.125f, -0.125f,   0.0f,  0.25f,  0.0f,   0.0f,  -0.5f,  0.0f,
		0.125f, -0.125f,   0.0f, -0.25f,  0.0f,   0.0f,   0.0f,  0.5f,
		0.125f, -0.125f,   0.0f, -0.25f,  0.0f,   0.0f,   0.0f, -0.5f};
	normalizeMatrix(H, 8, 8);

	//cout << "H_t: " << endl;

	float *H_t = transpose(H, 8, 8);
	normalizeMatrix(H_t, 8, 8);
	//printMatrix(H_t, 8, 8);
	//cout << endl;

	//cout << "H * H_t: " << endl;
	//printMatrix(multiply(H, H_t, 8, 8), 8, 8);

	//cout << "Initial Matrix: " << endl;
	//printMatrix(A, 8, 8);

	//printMatrix(A, 8, 8);
	//cout << endl;

	float* AHH_t = multiply(H_t, multiply(A, H, 8, 8), 8, 8);

	
	//cout << "Matrix after haar2: " << endl;
	//printMatrix(B, 8, 8);

	// Compress the output matrix by zeroing out unessisary frequency vals
	HaarCompress(AHH_t, compressionRatio);
	//printMatrix(AHH_t, 8, 8);
	//printMatrix(multiply(multiply(H, AHH_t, 8, 8), H_t, 8, 8), 8, 8);
	B = multiply(H, multiply(AHH_t, H_t, 8, 8), 8, 8);
}

void CompressBlock(const float* A, float* B, int m)
{
	float *C = new float[8 * 8];

	//cout << "Mat before DCT: " << endl;
	//printMatrix(A, 8, 8);
	//cout << endl;

	// DCT
	for (int v = 0; v < 8; v++)
	{
		for (int u = 0; u < 8; u++)
		{
			DCT(A, C, u, v, m);
		}
	}
	//cout << "Mat after DCT: " << endl;
	//printMatrix(C, 8, 8);
	//cout << endl;

	// IDCT
	for (int y = 0; y < 8; y++)
	{
		for (int x = 0; x < 8; x++)
		{
			IDCT(B, C, x, y);
		}
	}
	//cout << "Mat after IDCT: " << endl;
	//printMatrix(B, 8, 8);
	//cout << endl;

	// m represents a particular frequency that we must zero out, we zero out if 
	//    we get an x, y index of the matrix when added together that is > m

	// TODO: Homework Task 2 (see the PDF description)

}

void GrabBlock(const std::vector<float> &img, float *block, int x, int y, int width, int height)
{
	int i = 0;

	for (int h = y; h < y + height; h++)
	{
		for (int w = x; w < x + width; w++)
		{
			block[i++] = img[h * g_image_width + w] -0.5f;
		}
	}
}
void PlaceBlock(std::vector<float> &img, const float *block, int x, int y, int width, int height)
{
	int i = 0;

	for (int h = y; h < y + height; h++)
	{
		for (int w = x; w < x + width; w++)
		{
			img[h * g_image_width + w] = block[i++] +0.5f;
		}
	}
}

void PrintImage(const vector<float> img)
{
	for (int imgY = 0; imgY < g_image_height; imgY++)
	{
		cout << "[";
		for (int imgX = 0; imgX < g_image_width; imgX++)
		{
			cout << img[imgY * g_image_width + imgX] << ", ";
		}
		cout << "]" << endl << endl;
	}
}
void CompressImage(const std::vector<float> I, std::vector<float>& O, int m)
{
	for (int imgY = 0; imgY < g_image_height; imgY += 8)
	{
		for (int imgX = 0; imgX < g_image_width; imgX += 8)
		{
			float *block = new float[8 * 8];
			float *compressedBlock = new float[8 * 8];
			GrabBlock(I, block, imgX, imgY, 8, 8);

			CompressBlock(block, compressedBlock, m);

			PlaceBlock(O, compressedBlock, imgX, imgY, 8, 8);
		}
	}
}
void CompressImageHaar(const std::vector<float> I, std::vector<float>& O, float r)
{
	for (int imgY = 0; imgY < g_image_height; imgY += 8)
	{
		for (int imgX = 0; imgX < g_image_width; imgX += 8)
		{
			float *block = new float[8 * 8];
			float *compressedBlock = new float[8 * 8];
			GrabBlock(I, block, imgX, imgY, 8, 8);

			CompressBlockHaar(block, compressedBlock, r);
			PlaceBlock(O, compressedBlock, imgX, imgY, 8, 8);
		}
	}
}

int ReadLine(FILE *fp, int size, char *buffer)
{
	int i;
	for (i = 0; i < size; i++) {
		buffer[i] = fgetc(fp);
		if (feof(fp) || buffer[i] == '\n' || buffer[i] == '\r') {
			buffer[i] = '\0';
			return i + 1;
		}
	}
	return i;
}

//-------------------------------------------------------------------------------

bool LoadPPM(FILE *fp, int &width, int &height, std::vector<color> &data)
{
	const int bufferSize = 1024;
	char buffer[bufferSize];
	ReadLine(fp, bufferSize, buffer);
	if (buffer[0] != 'P' && buffer[1] != '6') return false;

	ReadLine(fp, bufferSize, buffer);
	while (buffer[0] == '#') ReadLine(fp, bufferSize, buffer);  // skip comments

	sscanf(buffer, "%d %d", &width, &height);

	ReadLine(fp, bufferSize, buffer);
	while (buffer[0] == '#') ReadLine(fp, bufferSize, buffer);  // skip comments

	data.resize(width*height);
	fread(data.data(), sizeof(color), width*height, fp);

	return true;
}

void glfwErrorCallback(int error, const char* description)
{
	std::cerr << "GLFW Error " << error << ": " << description << std::endl;
	exit(1);
}

void glfwKeyCallback(GLFWwindow* p_window, int p_key, int p_scancode, int p_action, int p_mods)
{
	if (p_key == GLFW_KEY_ESCAPE && p_action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(g_window, GL_TRUE);
	}
	else if (p_action == GLFW_PRESS)
	{
		switch (p_key)
		{
		case 49:	// press '1'
			g_draw_origin = 1;
			break;
		case 50:	// press '2'
			g_draw_origin = 2;
			break;
		case 51:	// press '3'
			g_draw_origin = 3;
			break;
		default:
			break;
		}
	}
}

void initWindow()
{
	// initialize GLFW
	glfwSetErrorCallback(glfwErrorCallback);
	if (!glfwInit())
	{
		std::cerr << "GLFW Error: Could not initialize GLFW library" << std::endl;
		exit(1);
	}

	g_window = glfwCreateWindow(g_windowWidth, g_windowHeight, g_windowName, NULL, NULL);
	if (!g_window)
	{
		glfwTerminate();
		std::cerr << "GLFW Error: Could not initialize window" << std::endl;
		exit(1);
	}

	// callbacks
	glfwSetKeyCallback(g_window, glfwKeyCallback);

	// Make the window's context current
	glfwMakeContextCurrent(g_window);

	// turn on VSYNC
	glfwSwapInterval(1);
}

void initGL()
{
	glClearColor(1.f, 1.f, 1.f, 1.0f);
}

void render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (g_draw_origin == 1)
		glDrawPixels(g_image_width, g_image_height, GL_LUMINANCE, GL_FLOAT, &g_luminance_data[0]);
	else if(g_draw_origin == 2)
		glDrawPixels(g_image_width, g_image_height, GL_LUMINANCE, GL_FLOAT, &g_compressed_luminance_data[0]);
	else if(g_draw_origin == 3)
		glDrawPixels(g_image_width, g_image_height, GL_LUMINANCE, GL_FLOAT, &g_haar_compressed_luminance_data[0]);
}

void renderLoop()
{
	while (!glfwWindowShouldClose(g_window))
	{
		// clear buffers
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		render();

		// Swap front and back buffers
		glfwSwapBuffers(g_window);

		// Poll for and process events
		glfwPollEvents();
	}
}

bool loadImage()
{
	std::vector<color> g_image_data;
	g_image_data.clear();
	g_image_width = 0;
	g_image_height = 0;
	FILE *fp = fopen(IMAGE_FILE, "rb");
	if (!fp) return false;

	bool success = false;
	success = LoadPPM(fp, g_image_width, g_image_height, g_image_data);

	g_luminance_data.resize(g_image_width * g_image_height);
	g_compressed_luminance_data.resize(g_image_width * g_image_height);
	g_haar_compressed_luminance_data.resize(g_image_width * g_image_height);
	for (int i = 0; i < g_image_height; i++)
	{
		for (int j = 0; j < g_image_width; j++)
		{
			// the index are not matching because of the difference between image space and OpenGl screen space
			g_luminance_data[i* g_image_width + j] = g_image_data[(g_image_height - i - 1)* g_image_width + j].r / 255.0f;
		}
	}

	g_windowWidth = g_image_width;
	g_windowHeight = g_image_height;

	return success;
}

bool writeImage()
{
	std::vector<color> tmpData;
	tmpData.resize(g_image_width * g_image_height);

	for (int i = 0; i < g_image_height; i++)
	{
		for (int j = 0; j < g_image_width; j++)
		{
			// make sure the value will not be larger than 1 or smaller than 0, which might cause problem when converting to unsigned char
			float tmp = g_compressed_luminance_data[i* g_image_width + j];
			if (tmp < 0.0f)	tmp = 0.0f;
			if (tmp > 1.0f)	tmp = 1.0f;

			tmpData[(g_image_height - i - 1)* g_image_width + j].r = unsigned char(tmp * 255.0);
			tmpData[(g_image_height - i - 1)* g_image_width + j].g = unsigned char(tmp * 255.0);
			tmpData[(g_image_height - i - 1)* g_image_width + j].b = unsigned char(tmp * 255.0);
		}
	}

	FILE *fp = fopen("data/out.ppm", "wb");
	if (!fp) return false;

	fprintf(fp, "P6\r");
	fprintf(fp, "%d %d\r", g_image_width, g_image_height);
	fprintf(fp, "255\r");
	fwrite(tmpData.data(), sizeof(color), g_image_width * g_image_height, fp);

	return true;
}
bool writeImageHaar()
{
	std::vector<color> tmpData;
	tmpData.resize(g_image_width * g_image_height);

	for (int i = 0; i < g_image_height; i++)
	{
		for (int j = 0; j < g_image_width; j++)
		{
			// make sure the value will not be larger than 1 or smaller than 0, which might cause problem when converting to unsigned char
			float tmp = g_haar_compressed_luminance_data[i* g_image_width + j];
			if (tmp < 0.0f)	tmp = 0.0f;
			if (tmp > 1.0f)	tmp = 1.0f;

			tmpData[(g_image_height - i - 1)* g_image_width + j].r = unsigned char(tmp * 255.0);
			tmpData[(g_image_height - i - 1)* g_image_width + j].g = unsigned char(tmp * 255.0);
			tmpData[(g_image_height - i - 1)* g_image_width + j].b = unsigned char(tmp * 255.0);
		}
	}

	FILE *fp = fopen("data/out_haar.ppm", "wb");
	if (!fp) return false;

	fprintf(fp, "P6\r");
	fprintf(fp, "%d %d\r", g_image_width, g_image_height);
	fprintf(fp, "255\r");
	fwrite(tmpData.data(), sizeof(color), g_image_width * g_image_height, fp);

	return true;
}


//-----------------------------------------------------------
// Test Functions
//-----------------------------------------------------------
void TestCompressBlock()
{
	float *A = new float[8 * 8] 
		{-76.0f, -73.0f, -67.0f, -62.0f, -58.0f, -67.0f, -64.0f, -55.0f,
		 -65.0f, -69.0f, -73.0f, -38.0f, -19.0f, -43.0f, -59.0f, -56.0f,
		 -66.0f, -69.0f, -60.0f, -15.0f,  16.0f, -24.0f, -62.0f, -55.0f,
		 -65.0f, -70.0f, -57.0f,  -6.0f,  26.0f, -22.0f, -58.0f, -59.0f,
		 -61.0f, -67.0f, -60.0f, -24.0f,  -2.0f, -40.0f, -60.0f, -58.0f,
		 -49.0f, -63.0f, -68.0f, -58.0f, -51.0f, -60.0f, -70.0f, -53.0f,
		 -43.0f, -57.0f, -64.0f, -69.0f, -73.0f, -67.0f, -63.0f, -45.0f,
		 -41.0f, -49.0f, -59.0f, -60.0f, -63.0f, -52.0f, -50.0f, -34.0f};

	float *B = new float[8 * 8];

	printMatrix(A, 8, 8);
	cout << endl;
	CompressBlock(A, B, 8);
}
void TestCompressBlockHaar()
{
	float *A = new float[8 * 8]
	{	88,88,89,90,92,94,96,97,
		90,90,91,92,93,95,97,97,
		92,92,93,94,95,96,97,97,
		93,93,94,95,96,96,96,96,
		92,93,95,96,96,96,96,95,
		92,94,96,98,99,99,98,97,
		94,96,99,101,103,103,102,101,
		95,97,101,104,106,106,105,105};


	float *B = new float[8 * 8];
	cout << endl;
	CompressBlockHaar(A, B, 0.25f);
}
//-----------------------------------------------------------

int main()
{
	loadImage();


	int n = 3;	// TODO: change the parameter n from 1 to 16 to see different image quality
	CompressImage(g_luminance_data, g_compressed_luminance_data, n);	
	CompressImageHaar(g_luminance_data, g_haar_compressed_luminance_data, 0.25f);

	writeImage();
	writeImageHaar();

	// render loop
	initWindow();
	initGL();
	renderLoop();

	return 0;
}
