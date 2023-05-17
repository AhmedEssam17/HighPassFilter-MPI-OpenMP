#include <iostream>
#include <math.h>
#include <stdlib.h>
#include<string.h>
#include<msclr\marshal_cppstd.h>
#include <ctime>// include this header 
#include "mpi.h"
#include "omp.h"
#include <exception>

#pragma once

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
using namespace System;
using namespace std;
using namespace msclr::interop;

int kernel[3][3] = { {0, -1, 0}, {-1, 4, -1}, {0, -1, 0} };

int* inputImage(int* w, int* h, System::String^ imagePath)
{
    int* input = nullptr;
    int OriginalImageWidth, OriginalImageHeight;

    // Read Image and save it to local arrays
    try
    {
        System::Drawing::Bitmap^ BM = gcnew System::Drawing::Bitmap(imagePath);
        OriginalImageWidth = BM->Width;
        OriginalImageHeight = BM->Height;
        *w = OriginalImageWidth;
        *h = OriginalImageHeight;

        int size = OriginalImageWidth * OriginalImageHeight;
        input = new int[size];

        for (int i = 0; i < OriginalImageHeight; i++)
        {
            for (int j = 0; j < OriginalImageWidth; j++)
            {
                System::Drawing::Color c = BM->GetPixel(j, i);
                int gray = (c.R + c.B + c.G) / 3;
                input[i * OriginalImageWidth + j] = gray;
            }
        }
    }
    catch (Exception^ ex)
    {
        // Handle exceptions when loading the image
        cout << "Error loading the image: " << marshal_as<std::string>(ex->Message) << endl;
    }

    return input;
}


void createImage(int* image, int width, int height, int index)
{
    try
    {
        System::Drawing::Bitmap^ MyNewImage = gcnew System::Drawing::Bitmap(width, height);

        for (int i = 0; i < MyNewImage->Height; i++)
        {
            for (int j = 0; j < MyNewImage->Width; j++)
            {
                if (image[i * width + j] < 0)
                {
                    image[i * width + j] = 0;
                }
                if (image[i * width + j] > 255)
                {
                    image[i * width + j] = 255;
                }
                System::Drawing::Color c = System::Drawing::Color::FromArgb(image[i * MyNewImage->Width + j], image[i * MyNewImage->Width + j], image[i * MyNewImage->Width + j]);
                MyNewImage->SetPixel(j, i, c);
            }
        }
        MyNewImage->Save("..//Data//Output//outputRes" + index + ".png");
        cout << "Result image saved: " << index << endl;
    }
    catch (Exception^ ex)
    {
        // Handle exceptions when saving the image
        cout << "Error saving the image: " << marshal_as<std::string>(ex->Message) << endl;
    }
}


/**================================================================
 * @Fn				-MPI
 * @brief 			-Reads Input Images, distributed rows among available processes,
 *                   Apply HighPassFilter Algorithm, then gather all results and save
 *                   the Output Image.
 * @param [in]	 	-NONE
 * @retval 			-NONE
 * Note				-To run this application just navigate to debug folder "HighPassFilter - MPI\Debug",
 *                   open Git Bash window and run "mpiexec "HPC_ProjectTemplate.exe"".
                     Input Images are defined in a char pointer array located in
                     "..//Data//Input//" path, saved output images are located in
                     "..//Data//Output//" path.
 */
int main()
{
    //-----------------------------------------------
    //1) Initialize timer variables
    //-----------------------------------------------
    int start_s, stop_s, TotalTime = 0;

    //-----------------------------------------------
    //2) Initialize MPI
    //-----------------------------------------------
    MPI_Init(NULL, NULL);

    //-----------------------------------------------
    //3) Get Rank of each Process & Num of Processes
    //-----------------------------------------------
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    //-----------------------------------------------
    //4) Input Images to be read and processed
    //-----------------------------------------------
    char* image[7] =
    {
        "..//Data//Input//1.png",
        "..//Data//Input//2.jpg",
        "..//Data//Input//3.png",
        "..//Data//Input//4.jpg",
        "..//Data//Input//5.jpg",
        "..//Data//Input//6.png",
        "..//Data//Input//7.png",
    };

    int imageWidth;
    int imageHeight;

    //-----------------------------------------------
    //5) Loop for 7 different Input Images
    //-----------------------------------------------
    for (int i = 1; i < 8; i++) {
        if (rank == 0)
            cout << "\nProcessing Image " << i << " Using MPI ..." << endl;

        //---------------------------------------------------
        //6) Input an Image and store its data in imageData
        //---------------------------------------------------
        int* imageData = NULL;
        System::String^ imagePath;
        std::string img;
        img = image[i - 1];
        imagePath = marshal_as<System::String^>(img);
        imageData = inputImage(&imageWidth, &imageHeight, imagePath);

        start_s = clock();

        //------------------------------------------------------------
        //7) Calculate the size of each sub-image accross imageHeight
        //------------------------------------------------------------
        int processRow = imageHeight / size;
        int remainder = imageHeight % size;
        int localHeight = processRow;

        //------------------------------------------------------------
        //8) Add the remainder height to last Process
        //------------------------------------------------------------
        if (rank == size - 1) {
            localHeight += remainder - 2;
        }

        //------------------------------------------------------------
        //9) Adjust start and end boundaries for each process
        //------------------------------------------------------------
        int start = rank * processRow + 1;
        int end = start + localHeight;

        int* localBuffer = new int[localHeight * imageWidth];

        //------------------------------------------------------------
        //10) Apply HighPassFilter Algorithm to each processData 
        //   and store filtered data in localBufer
        //------------------------------------------------------------
        int kernel[3][3] = { {0, -1, 0}, {-1, 4, -1}, {0, -1, 0} };

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
                localBuffer[(y - start) * imageWidth + x] = (int)max(0, min(255, sum));
            }
        }

        //------------------------------------------------------------
        //11) Gather Results from each process
        //------------------------------------------------------------
        int gatherCount = localHeight * imageWidth;

        int* gatheredBuffer = nullptr;
        int* recvCounts = nullptr;
        int* displs = nullptr;

        if (rank == 0) {
            recvCounts = new int[size];
            displs = new int[size];
            gatheredBuffer = new int[imageWidth * imageHeight];

            //------------------------------------------------------------
            //12) Calculate receive counts and displacements
            //------------------------------------------------------------
            for (int r = 0; r < size; r++) {
                int rows = processRow;
                if (r == size - 1) {
                    rows += remainder;
                }
                recvCounts[r] = rows * imageWidth;
                displs[r] = r * processRow * imageWidth;
            }
        }

        //------------------------------------------------------------
        //13) Gather filtered localBuffer into gatheredBuffer
        //------------------------------------------------------------
        MPI_Gatherv(localBuffer, gatherCount, MPI_INT, gatheredBuffer,
            recvCounts, displs, MPI_INT, 0, MPI_COMM_WORLD);

        //------------------------------------------------------------
        //14) Save the Output Image & Calculate total time
        //------------------------------------------------------------
        if (rank == 0) {
            stop_s = clock();
            TotalTime += (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;
            createImage(gatheredBuffer, imageWidth, imageHeight, i);
            cout << "Time: " << TotalTime << endl;

            //------------------------------------------------------------
            //15) Free Gathering allocated memory
            //------------------------------------------------------------
            delete[] gatheredBuffer;
            delete[] recvCounts;
            delete[] displs;
        }

        //------------------------------------------------------------
        //16) Free image-related allocated memory
        //------------------------------------------------------------
        delete[] imageData;
        delete[] localBuffer;
    }

    //------------------------------------------------------------
    //17) Finialize MPI
    //------------------------------------------------------------
    MPI_Finalize();

    return 0;
}


