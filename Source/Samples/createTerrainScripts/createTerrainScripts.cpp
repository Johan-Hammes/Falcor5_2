// createTerrainScripts.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include <direct.h>
#include <string>



std::string path = "F:/terrains/";
std::string name = "sonoma";
std::string proj = "\"+proj=tmerc +lat_0=38.161 +lon_0=-122.46 +k_0=1 + x_0=0 + y_0=0 +ellps=GRS80 +units=m\"";
float size = 32768.f;
std::string elevInputFiles = "";
float rootheight[2048][2048];

void addTile(int _lod, int _y, int _x, FILE* _file, FILE* _list)
{
    int grid = pow(2, _lod);
    int halfgrid = grid / 2;
    float origin = -(size * 0.5f);
    float block = size / grid;
    float total = block * (256.f / 248.f);
    float edge = (total - block) * 0.5f;

    // minmax
    /*
    float min = 100000000;
    float max = -100000000;
    int minmaxgrid = 2048 / grid;
    int mmY = _y * minmaxgrid;
    int mmX = _x * minmaxgrid;
    for (int y = 0; y < minmaxgrid; y++) {
        for (int x = 0; x < minmaxgrid; x++) {
            max = __max(max, rootheight[mmY + y][mmX + x]);
            min = __min(min, rootheight[mmY + y][mmX + x]);
        }
    }
    //then padd a little to make yp for lack of presision
    min = __max(0, min);
    //max = __min(0, max);
    max += 10;
    min -= 10;
    */

    float xstart = origin + (_x * block) - edge;
    float ystart = origin + (_y * block) - edge;
    fprintf(_file, "gdalwarp -t_srs %s ", proj.c_str());
    fprintf(_file, "-te %f %f %f %f ", xstart, -(ystart + total), xstart + total, -ystart);     // remember to flip Y
    fprintf(_file, "-ts 1024 1024 -r cubicspline -multi -overwrite -ot Float32 ");      //-srcnodata 0.0 
    fprintf(_file, "%s ", elevInputFiles.c_str());
    fprintf(_file, "../_temp/hgt_%d_%d_%d.tiff \n\n", _lod, _y, _x);    // tiff for now

    fprintf(_file, "gdal_translate -ot Float32 ../_temp/hgt_%d_%d_%d.tiff ../_temp/hgt_%d_%d_%d.bil \n\n\n", _lod, _y, _x, _lod, _y, _x);

    fprintf(_list, "%d %d %d %f %f %f hgt_%d_%d_%d\n", _lod, _y, _x, xstart, ystart, total, _lod, _y, _x);

    if (_lod == 0) {
        //fprintf(_file, "gdal_translate  -ot UINT16 -scale %f %f 0 65536  ../_temp/hgt_%d_%d_%d.tiff ../../elevation/hgt_%d_%d_%d.bil \n\n\n", min, max, _lod, _y, _x, _lod, _y, _x);

        //fprintf(_list, "%d %d %d 1024 %f %f %f %f %f elevation/hgt_%d_%d_%d.bil\n", _lod, _y, _x, xstart, ystart, total, min, max - min, _lod, _y, _x);
    }
    else {
        //fprintf(_file, "gdal_translate -of JP2OpenJPEG -ot UINT16 -scale %f %f 0 65536  ../_temp/hgt_%d_%d_%d.tiff ../../elevation/hgt_%d_%d_%d.jp2 \n\n\n", min, max, _lod, _y, _x, _lod, _y, _x);

        //fprintf(_list, "%d %d %d 1024 %f %f %f %f %f elevation/hgt_%d_%d_%d.jp2\n", _lod, _y, _x, xstart, ystart, total, min, max - min, _lod, _y, _x);
    }



    
}


int main()
{
    std::cout << "Create scripts\n";


    int step = 2;

    switch (step)
    {
    case 0:
    {
        _chdir((path + name + "/gis/photos").c_str());
        system("dir /B > files.txt");

        _chdir((path + name + "/gis/elevation").c_str());
        system("dir /B > files.txt");

        printf("Then go and edit files.txt to contain the correct files and layer order\n");
    }
        break;

    case 1:
    {
        // the root files INDEX and
        float half = size * 0.5f;
        FILE* file, *input;
        char filename[256];
        int numread = 1;
        fopen_s(&input, (path + name + +"/gis/photos/files.txt").c_str(), "r");
        fopen_s(&file, (path + name + +"/gis/photos/_gdal_index.bat").c_str(), "w");
        if (file) {
            fprintf(file, "gdalwarp -t_srs %s -te -%f -%f %f %f -ts 16384 16384 -r cubicspline -multi -overwrite -ot byte -of jpeg ", proj.c_str(), half, half, half, half);
            while (numread > 0)
            {
                numread = fscanf_s(input, "%s", filename, 256);
                if (numread > 0) {
                    fprintf(file, "%s ", filename);
                }
            }
            fprintf(file, " ../_export/photo_index.jpg");

            fclose(file);
            fclose(input);
        }

        float block = size / 16.f;
        float total = block * (3200.f / 2500.f);        //(3200.f / 2500.f) is an accident leftover from sonoma just use it
        float edge = (total - block) * 0.5f;
        fopen_s(&file, (path + name + +"/gis/photos/_gdal_tiles.bat").c_str(), "w");
        if (file) {
            for (int y = 7; y < 9; y++)
            {
                float ystart = ((7 - y) * block) - edge;
                for (int x = 7; x < 9; x++)
                {
                    float xstart = ((x - 8) * block) - edge;
                    fprintf(file, "gdalwarp -t_srs %s -te %f %f %f %f -ts 8192 8192 -r cubicspline -multi -overwrite -ot byte ", proj.c_str(), xstart, ystart, xstart + total, ystart + total);

                    numread = 1;
                    fopen_s(&input, (path + name + +"/gis/photos/files.txt").c_str(), "r");
                    while (numread > 0)
                    {
                        numread = fscanf_s(input, "%s", filename, 256);
                        if (numread > 0) {
                            fprintf(file, "%s ", filename);
                        }
                    }
                    fclose(input);

                    std::string out = std::string(" ../_export/") + char(65 + x) + "_" + std::to_string(y) + "_image.tiff";
                    fprintf(file, out.c_str());

                    fprintf(file, "\n\n");

                    // dit kort ook gigapixel meestal, los eers
                    //std::string dds = path + "/Compressonator/CompressonatorCLI -fd BC7 -Quality 0.01 " + path + name + "/overlay/" + char(65 + x) + "_" + std::to_string(y) + "_image.dds";

                    fprintf(file, "\n\n\n");
                }
            }
            fclose(file);
        }

        // root elevation BIL for lod processing later
        numread = 1;
        fopen_s(&input, (path + name + +"/gis/elevation/files.txt").c_str(), "r");
        fopen_s(&file, (path + name + +"/gis/elevation/_gdal_root2048.bat").c_str(), "w");
        if (file) {
            fprintf(file, "gdalwarp -t_srs %s -te -%f -%f %f %f -ts 2048 2048 -r cubicspline -multi -overwrite -ot Float32  ", proj.c_str(), half, half, half, half);
            while (numread > 0)
            {
                numread = fscanf_s(input, "%s", filename, 256);
                if (numread > 0) {
                    fprintf(file, "%s ", filename);
                }
            }
            fprintf(file, " ../_export/root2048.bil");

            fclose(file);
            fclose(input);
        }


        fopen_s(&file, (path + name + +"/gis/elevation/_gdal_tiles.bat").c_str(), "w");
        if (file) {
            for (int y = 7; y < 9; y++)
            {
                float ystart = ((7 - y) * block) - edge;
                for (int x = 7; x < 9; x++)
                {
                    float xstart = ((x - 8) * block) - edge;
                    fprintf(file, "gdalwarp -t_srs %s -te %f %f %f %f -ts 3200 3200 -r cubicspline -multi -overwrite -ot Float32 ", proj.c_str(), xstart, ystart, xstart + total, ystart + total);

                    numread = 1;
                    fopen_s(&input, (path + name + +"/gis/elevation/files.txt").c_str(), "r");
                    while (numread > 0)
                    {
                        numread = fscanf_s(input, "%s", filename, 256);
                        if (numread > 0) {
                            fprintf(file, "%s ", filename);
                        }
                    }
                    fclose(input);

                    std::string out = std::string(" ../_export/") + char(65 + x) + "_" + std::to_string(y) + "_height.tiff";
                    fprintf(file, out.c_str());

                    fprintf(file, "\n\n");
                    fprintf(file, "gdal_translate -of XYZ ../_export/%c_%d_height.tiff ../_export/%c_%d_height.asc \n", char(65 + x), y, char(65 + x), y);
                    

                    fprintf(file, "\n\n\n");
                }
            }
            fclose(file);
        }
    }
        break;




    case 2:
    {
        unsigned char lod4[16][16];
        memset(lod4, 0, 16 * 16);
        unsigned char lod6[64][64];
        memset(lod6, 0, 64 * 64);
        unsigned char lod8[256][256];
        memset(lod8, 0, 256 * 256);
        FILE* lodFile;
        fopen_s(&lodFile, (path + name + +"/gis/elevation/lod4.bil").c_str(), "rb");
        if (lodFile) {
            fread(lod4, 1, 16 * 16, lodFile);
            fclose(lodFile);
        }
        fopen_s(&lodFile, (path + name + +"/gis/elevation/lod6.bil").c_str(), "rb");
        if (lodFile) {
            fread(lod6, 1, 64 * 64, lodFile);
            fclose(lodFile);
        }
        fopen_s(&lodFile, (path + name + +"/gis/elevation/lod8.bil").c_str(), "rb");
        if (lodFile) {
            fread(lod8, 1, 256 * 256, lodFile);
            fclose(lodFile);
        }

        fopen_s(&lodFile, (path + name + +"/gis/_export/root2048.bil").c_str(), "rb");
        if (lodFile) {
            fread(rootheight, sizeof(float), 2048 * 2048, lodFile);
            fclose(lodFile);
        }

        

        FILE* input;
        fopen_s(&input, (path + name + +"/gis/elevation/files.txt").c_str(), "r");
        int numread;
        do
        {
            char filename[256];
            memset(filename, 0, 256);
            numread = fscanf_s(input, "%s", filename, 256);
            elevInputFiles += std::string(filename) + " ";
        } while (numread > 0);
        printf("%s", elevInputFiles.c_str());

        FILE* file, *list;
        fopen_s(&list, (path + name + +"/gis/_temp/filenames.txt").c_str(), "w");
        int lod, y, x, grid;
        fopen_s(&file, (path + name + +"/gis/elevation/_gdal_jp2.bat").c_str(), "w");
        if (file) {

            addTile(0, 0, 0, file, list);

            lod = 2;
            grid = pow(2, lod);
            for (y = 0; y < 3; y++) {
                for (x = 0; x < 3; x++) {
                    addTile(lod, y, x, file, list);
                }
            }

            lod = 4;
            grid = pow(2, lod);
            for (y = 0; y < grid; y++) {
                for (x = 0; x < grid; x++) {
                    if (lod4[y][x] > 128) {
                        addTile(lod, y, x, file, list);
                    }
                }
            }

            // this gets us down to 0.6 meters probably good enough 512x512 tile
            lod = 6;
            grid = pow(2, lod);
            for (y = 0; y < grid; y++) {
                for (x = 0; x < grid; x++) {
                    if (lod6[y][x] > 128) {
                        addTile(lod, y, x, file, list);
                    }
                }
            }

            /*
            lod = 8;
            grid = pow(2, lod);
            for (y = 0; y < grid; y++) {
                for (x = 0; x < grid; x++) {
                    if (lod8[y][x] > 128) {
                        addTile(lod, y, x, file, list);
                    }
                }
            }
            */

            fclose(file);
            fclose(list);
        }
    }
        break;
    }
    
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
