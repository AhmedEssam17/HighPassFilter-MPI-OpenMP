#include <iostream>
#include <math.h>
#include <stdlib.h>
#include<string.h>
#include "omp.h"
#include<msclr\marshal_cppstd.h>
#include <ctime>// include this header 
#pragma once

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
using namespace std;
using namespace msclr::interop;

int* inputImage(int* w, int* h, System::String^ imagePath) //put the size of image in w & h
{
	int* input;


	int OriginalImageWidth, OriginalImageHeight;

	//*********************************************************Read Image and save it to local arrayss*************************	
	//Read Image and save it to local arrayss

	System::Drawing::Bitmap BM(imagePath);

	OriginalImageWidth = BM.Width;
	OriginalImageHeight = BM.Height;
	*w = BM.Width;
	*h = BM.Height;
	int *Red = new int[BM.Height * BM.Width];
	int *Green = new int[BM.Height * BM.Width];
	int *Blue = new int[BM.Height * BM.Width];
	input = new int[BM.Height*BM.Width];
	for (int i = 0; i < BM.Height; i++)
	{
		for (int j = 0; j < BM.Width; j++)
		{
			System::Drawing::Color c = BM.GetPixel(j, i);

			Red[i * BM.Width + j] = c.R;
			Blue[i * BM.Width + j] = c.B;
			Green[i * BM.Width + j] = c.G;

			input[i*BM.Width + j] = ((c.R + c.B + c.G) / 3); //gray scale value equals the average of RGB values

		}

	}
	return input;
}


void createImage(int* image, int width, int height, int index)
{
	System::Drawing::Bitmap MyNewImage(width, height);


	for (int i = 0; i < MyNewImage.Height; i++)
	{
		for (int j = 0; j < MyNewImage.Width; j++)
		{
			//i * OriginalImageWidth + j
			if (image[i*width + j] < 0)
			{
				image[i*width + j] = 0;
			}
			if (image[i*width + j] > 255)
			{
				image[i*width + j] = 255;
			}
			System::Drawing::Color c = System::Drawing::Color::FromArgb(image[i*MyNewImage.Width + j], image[i*MyNewImage.Width + j], image[i*MyNewImage.Width + j]);
			MyNewImage.SetPixel(j, i, c);
		}
	}
	MyNewImage.Save("..//Data//Output//outputRes" + index + ".png");
	cout << "result Image Saved " << index << endl;
}


void OpenMp() {
	int start_s = 0;
	int stop_s = 0;
	int TotalTime = 0;
	int nthreads = 8;
	int tid;

	char* image[7] = {
		"..//Data//Input//1.png",
		"..//Data//Input//2.jpg",
		"..//Data//Input//3.png",
		"..//Data//Input//4.jpg",
		"..//Data//Input//5.jpg",
		"..//Data//Input//6.png",
		"..//Data//Input//7.png",
	};

	int kernel[3][3] = { {0, -1, 0}, {-1, 4, -1}, {0, -1, 0} };

	for (int i = 0; i < 7; i++) {
		int imageWidth, imageHeight;
		int* imageData = nullptr;

		System::String^ imagePath;
		std::string img = image[i];
		imagePath = marshal_as<System::String^>(img);
		imageData = inputImage(&imageWidth, &imageHeight, imagePath);

		int* filteredImage = new int[imageWidth * imageHeight];

		// Parallel processing
#pragma omp parallel private(tid) shared(imageData, filteredImage) num_threads(nthreads)
		{
			tid = omp_get_thread_num();

			int processRow = imageHeight / nthreads;
			int remainder = imageHeight % nthreads;
			int localHeight = processRow;

			if (tid == nthreads - 1)
				localHeight += remainder - 2;

			int start = tid * processRow + 1;
			int end = start + localHeight;

			for (int y = start; y < end; y++) {
				for (int x = 1; x < imageWidth - 1; x++) {
					int sum = 0;
					for (int ky = -1; ky <= 1; ky++) {
						for (int kx = -1; kx <= 1; kx++) {
							int px = x + kx;
							int py = y + ky;
							if (px >= 0 && px < imageWidth && py >= 0 && py < imageHeight) {
								sum += imageData[py * imageWidth + px] * kernel[ky + 1][kx + 1];
							}
						}
					}
					filteredImage[(y)*imageWidth + x] = (int)max(0, min(255, sum));
				}
			}
		}
		createImage(filteredImage, imageWidth, imageHeight, i);

		stop_s = clock();
		TotalTime += (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;
		cout << "Time: " << TotalTime << endl;

		delete[] imageData;
		delete[] filteredImage;
	}	
}

int main()
{
	OpenMp();

	return 0;

}



