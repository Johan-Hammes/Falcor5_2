#include "ecotope.h"

#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/archives/xml.hpp"
#include <sstream>
#include <fstream>
#include "imgui.h"




ecotope::ecotope() { 
	for (int j = 0; j < 6; j++)
	{
		weights[j][0] = 0;
		weights[j][1] = 0;
		weights[j][2] = 0;
		weights[j][3] = 0;
	}
}


void ecotope::loadTexture(int i) {

	//std::string filename, fullpath;
	//if (openFileDialog("Supported Formats\0*.dds;\0\0", filename))
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"dds"} };
    if (openFileDialog(filters, path))
	{
		//if (findFileInDataDirectories(filename, fullpath) == true)
		{
			switch (i) {
			case 0: albedoName = path.string(); break;
			case 1: noiseName = path.string(); break;
			case 2: displacementName = path.string(); break;
			case 3: roughnessName = path.string(); break;
			case 4: aoName = path.string(); break;
			}

			reloadTextures();
		}
	}
}

void ecotope::reloadTextures()
{
	texAlbedo = Texture::createFromFile(albedoName, true, false);
	texNoise = Texture::createFromFile(noiseName, false, false);
	texDisplacement = Texture::createFromFile(displacementName, false, false);
	texRoughness = Texture::createFromFile(roughnessName, false, false);
	texAO = Texture::createFromFile(aoName, false, false);

}

std::array<std::string, 6> tfWeights = { "Height", "Concavity", "Flatness", "Slope", "Aspect", "Texture" };
std::array<std::string, 5> tfTexHeading = { "albedo", "noise", "displacement", "roughness", "ao" };
void ecotope::GuiTextures(Gui *_pGui)
{
	ImGui::Text("albedo");
	ImGui::SameLine(140);
	if (ImGui::Selectable(albedoName.size() ? albedoName.c_str() : tfTexHeading[0].c_str())) { loadTexture(0); }
	ImGui::Text("noise");
	ImGui::SameLine(140);
	if (ImGui::Selectable(noiseName.size() ? noiseName.c_str() : tfTexHeading[1].c_str())) { loadTexture(1); }
	ImGui::Text("displacement");
	ImGui::SameLine(140);
	if (ImGui::Selectable(displacementName.size() ? displacementName.c_str() : tfTexHeading[2].c_str())) { loadTexture(2); }
	ImGui::Text("roughness");
	ImGui::SameLine(140);
	if (ImGui::Selectable(roughnessName.size() ? roughnessName.c_str() : tfTexHeading[3].c_str())) { loadTexture(3); }
	ImGui::Text("ao");
	ImGui::SameLine(140);
	if (ImGui::Selectable(aoName.size() ? aoName.c_str() : tfTexHeading[4].c_str())) { loadTexture(4); }

	ImGui::NewLine();
}

void ecotope::GuiWeights(Gui *_pGui)
{

	for (int i = 0; i < 6; i++) {
		ImGui::PushID(i);
		//ImGui::Text(tfWeights[i].c_str());
		ImGui::SliderFloat(tfWeights[i].c_str(), &weights[i][0], 0, 1, "%.2f");
		if (weights[i][0] > 0) {
			switch (i) {
			case 0:
				ImGui::DragFloat("###a", &weights[i][1], 1, 0, 1000, "bottom %3.0f m");
				ImGui::DragFloat("###b", &weights[i][2], 1, 10, 500, "top %3.0f m");
				ImGui::DragFloat("###c", &weights[i][3], 0.1f, 0.1f, 20, "smooth %.1f");
				break;
			case 1:
				ImGui::DragFloat("###a", &weights[i][1], 1, 0, 10, "disp %3.1f m");
				break;
			case 2:
				ImGui::DragFloat("###a", &weights[i][1], 1, 0, 10, "disp %3.1f m");
				break;
			case 3:
				ImGui::DragFloat("min", &weights[i][1], 0.01f, 0, 1, "%3.2f");
				ImGui::DragFloat("max", &weights[i][2], 0.01f, 0, 1, "%3.2f");
				ImGui::DragFloat("smooth", &weights[i][3], 0.1f, 0.1f, 20, "%.1f");
				break;
			case 4:
				if (ImGui::DragFloat("###a", &weights[i][1], 0.01f, -1, 1, "dX %.2f")) {
					glm::vec2 A = glm::vec2(weights[4][1], weights[4][2]);
					glm::normalize(A);
					weights[4][1] = A.x;
					weights[4][2] = A.y;
				}
				if (ImGui::DragFloat("###b", &weights[i][2], 0.01f, 1, 1, "dZ %.2f")) {
                    glm::vec2 A = glm::vec2(weights[4][1], weights[4][2]);
					glm::normalize(A);
					weights[4][1] = A.x;
					weights[4][2] = A.y;
				}
				break;
			case 5:
				ImGui::DragFloat("###c", &weights[i][1], 1, 1, 100, "%.2f uv");
				ImGui::DragFloat("###a", &weights[i][2], 0.01f, 0, 1, "%.2f scale");
				ImGui::DragFloat("###b", &weights[i][3], 0.01f, 0, 1, "%.2f offset");
				break;
			}
		}
		ImGui::PopID();

		ImGui::NewLine();
	}

}

void ecotope::addPlant()
{
	//std::string filename, fullpath;
	//if (openFileDialog("Supported Formats\0*.fbx;\0\0", filename))		//??? format
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"fbx"} };
    if (openFileDialog(filters, path))
	{
		//if (findFileInDataDirectories(filename, fullpath) == true)
		{
			_displayPlant P;
			plants.push_back(P);
		}
	}
}

void ecotope::GuiPlants(Gui *_pGui)
{
	if (ImGui::Button("add plant")) {
		addPlant();
	}

	ImGui::NewLine();
	
	for (int i = 0; i < plants.size(); i++)
	{
		ImGui::Text("tree_%d", i);
	}
}


void ecotope::renderGUI(Gui *_pGui) {
	//_pGui->pushFont(3);
	ImGui::Text(name.c_str());
	//_pGui->popFont();
	ImGui::NewLine();
	
	GuiTextures(_pGui);

	ImGui::Columns(2);
	GuiWeights(_pGui);
	ImGui::NextColumn();
	GuiPlants(_pGui);
}

void ecotope::renderPlantGUI(Gui *_gui)
{
	if (selectedPlant < plants.size())
	{
        Gui::Window plantPanel(_gui, "Plant##tfPanel", { 900, 900 }, { 100, 100 });
        {
            //_pGui->pushFont(3);
            ImGui::Text(plants[selectedPlant].shortName.c_str());
            //_pGui->popFont();
            ImGui::Text(plants[selectedPlant].name.c_str());
            ImGui::DragFloat("density", &plants[selectedPlant].density, 0, 1000, 0.1f);
            ImGui::DragFloat("scale", &plants[selectedPlant].scale, 0, 1000, 0.1f);
            ImGui::DragFloat("variance", &plants[selectedPlant].scaleVariation, 0, 1000, 0.1f);
        }
        plantPanel.release();
	}
}




void ecotopeSystem::load() {
	//td::string filename;
	//if (openFileDialog("ecosystem\0*.ecosystem\0\0", filename))
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"ecosystem"} };
    if (openFileDialog(filters, path))
	{
		std::ifstream is(path.string());
		cereal::JSONInputArchive archive(is);
		serialize(archive);

		{
//			std::ofstream os(filename + "_xml");
//			cereal::XMLOutputArchive archive(os);
//			serialize(archive);
		}
	}
}


void ecotopeSystem::save() {
	//std::string filename;
	//if (saveFileDialog("ecosystem\0*.ecosystem\0\0", filename))
    std::filesystem::path path;
    FileDialogFilterVec filters = { {"ecosystem"} };
    if (openFileDialog(filters, path))
	{
		std::ofstream os(path.string());
		cereal::JSONOutputArchive archive(os);
		serialize(archive);
	}
}


void ecotopeSystem::renderGUI(Gui *_pGui) 
{
	auto& style = ImGui::GetStyle();
	ImGuiStyle oldStyle = style;
	style.ButtonTextAlign = ImVec2(0, 0);
	style.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.7f, 0.9f, 0.7f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

	float W = ImGui::GetWindowWidth();
	ImGui::NewLine();

    
	//_pGui->pushFont(2);

	for (int i = 0; i < ecotopes.size(); i++) {

		if (i == selectedEcotope) {
			style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
			//_pGui->pushFont(3);
			{
				ImGui::NewLine();
				ImGui::SameLine(40);
				char nameEdit[1024];
				sprintf(nameEdit, "%s", ecotopes[i].name.c_str());
				if (ImGui::InputText("", nameEdit, 1024)) {
					ecotopes[i].name = nameEdit;
				}
			}
			//_pGui->popFont();
		}
		else {
			style.Colors[ImGuiCol_Text] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
			char nameEdit[1024];
			sprintf(nameEdit, "%2d - %s", i, ecotopes[i].name.c_str());
			if (ImGui::Button(nameEdit, ImVec2(W - 20, 30))) {
				selectedEcotope = i;
			}
		}
	}



	//_pGui->popFont();
	style = oldStyle;
}


void ecotopeSystem::renderSelectedGUI(Gui *_pGui)
{
	if (selectedEcotope < ecotopes.size()) {
		ecotopes[selectedEcotope].renderGUI(_pGui);
	}
}

void ecotopeSystem::renderPlantGUI(Gui *_pGui)
{
	if (selectedEcotope < ecotopes.size()) {
		ecotopes[selectedEcotope].renderPlantGUI(_pGui);
	}
}

void ecotopeSystem::addEcotope() {
	ecotope E;
	ecotopes.push_back(E);
}



void ecotopeSystem::rebuildRuntime() {
	/*
	constantBuffer = ConstantBuffer::create((Program::SharedPtr)topdownProgramForBlends, "gTopdownConstants");
	constantBuffer->setVariable("heightMask", float4((float)blends[0].useVertex, blends[0].scale, 0.2f, 0.0f));

	constantBuffer->setVariable("heightPermanence", blends[0].permanence);
	constantBuffer->setVariable("colourPermanence", blends[1].permanence);
	constantBuffer->setVariable("ecotopePermanence", blends[2].permanence);
	constantBuffer->setVariable("detailTextureSize", detailTextureSize);

	float4x4 ect[4];
	memcpy(ect, &ecotopeMapBlends, sizeof(float) * 16 * 4);	// direct copy should work
	constantBuffer->setVariableArray("ecotopeTextureMap", ect, 4);

	TerrainManager::needsRefresh = true;
	*/
}


