// CreateTerrain.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include <filesystem>
#include <direct.h>

int main()
{
    std::cout << "Hello World!\n";

    std::string path = "F:/terrains/";
    std::string name = "switserland_Steg";
    float size = 40000.f;
    float latt = 47.27f;
    float lon = 9.07f;


    _chdir( path.c_str() );
    system(("mkdir " + name).c_str());
    _chdir((path + name).c_str());

    FILE* file;
    fopen_s(&file, (path + name + "/" + name + ".root").c_str(), "w");
    if (file) {
        fprintf(file, "%s \n", name.c_str());
        fprintf(file, "%f \n", size);
        fprintf(file, "\"+proj=tmerc +lat_0=%1.2f +lon_0=%1.2f +k_0=1 +x_0=0 +y_0=0 +ellps=GRS80 +units=m\"\n", latt, lon);
        fprintf(file, "ecosystems/%s.ecotopes \n", name.c_str());
        fclose(file);
    }


    system("mkdir elevation");
    system("mkdir bake");
    system("mkdir ecosystem");
    system("mkdir terrafectors");
    system("mkdir _export");
    system("mkdir overlay");
    system("mkdir gis");
    system("mkdir roads");

    {
        _chdir((path + name + "/terrafectors").c_str());
        system("mkdir 10_bakeOnlyBottom");
        system("mkdir 20_base");
        system("mkdir 30_roads");
        system("mkdir 40_bakeOnlyTop");
        system("mkdir 50_top");
        system("mkdir 60_overlay");
    }

    
    {
        _chdir((path + name + "/_export").c_str());
        system("mkdir bridges");
        system("mkdir roads");
        system("mkdir scene");
        system("mkdir tiles");
    }

    
    {
        _chdir((path + name + "/gis").c_str());
        system("mkdir elevation");
        system("mkdir photos");
        system("mkdir _export");
        system("mkdir _temp");
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
