//  ================================================================
//  Created by Gregory Kramida on 6/28/18.
//  Copyright (c) 2018-2025 Gregory Kramida
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at

//  http://www.apache.org/licenses/LICENSE-2.0

//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//  ================================================================

//stdlib
#include <map>

//VTK
#include <vtkObjectFactory.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>


//local
#include "ITMScene3DSliceVisualizerInteractorStyle.h"
#include "../FileIO/ITMDynamicFusionLogger.h"

using namespace ITMLib;

// Can't use vtkStandardNewMacro on a templated class.
template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>*
ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>::New() {
	ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>* result = new ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>();
	result->InitializeObjectBase();
	return result;
};


template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>::
ITMScene3DSliceVisualizerInteractorStyle() : bindingMap(createBindingMap()) {

}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
void ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>::OnKeyPress() {
	// Get the keypress
	vtkRenderWindowInteractor* rwi = this->Interactor;
	std::string key = rwi->GetKeySym();
	bool shiftPressed = static_cast<bool>(rwi->GetShiftKey());
	bool altPressed = static_cast<bool>(rwi->GetAltKey());
	bool ctrlPressed = static_cast<bool>(rwi->GetControlKey());
	std::string hashKey = std::string((ctrlPressed ? "Ctrl+" : "")) + (altPressed ? "Alt+" : "") +
	                      (shiftPressed ? "Shift+" : "") + key;

	auto item = this->bindingMap.find(hashKey);
	if (item != this->bindingMap.end()) {
		(this->*(item->second.function))();
	} else {
		std::cout << "User pressed '" << hashKey << "', which has no active binding." << std::endl;
	}

	//forward events
	vtkInteractorStyle::OnKeyPress();
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
void ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>::SetParent(
		ITMScene3DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>* parent) {
	this->parent = parent;
	this->keyBindingLayerIndex = this->parent->window->GetLayerCount();
	this->parent->window->AddLayer();

	int yPosition = 1000;
	int yOffset = 20;
	for (auto const& pair : this->bindingMap) {
		vtkSmartPointer<vtkTextActor> textActor = vtkSmartPointer<vtkTextActor>::New();
		textActor->SetInput((pair.first + " : " + pair.second.description).c_str());
		textActor->SetPosition(10, yPosition);
		yPosition -= yOffset;
		textActor->GetTextProperty()->SetFontFamilyToCourier();
		textActor->GetTextProperty()->SetFontSize(14);
		textActor->GetTextProperty()->SetColor(0.0, 0.0, 0.0);
		this->parent->window->AddActor2DToLayer(textActor, this->keyBindingLayerIndex);
	}
	this->parent->window->HideLayer(keyBindingLayerIndex);
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
std::map<std::string, typename ITMLib::ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>::UserAction>
ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>::createBindingMap() {
	std::map<std::string, UserAction> map;
	// CREATE NEW KEY BINDINGS HERE!
	// Example keys: "F1","Ctrl+a", "Alt+b", "Ctrl+Alt+k", "Alt+Shift+b", "Ctrl+Shift+c", "Ctrl+Alt+Shift+grave"
	map.insert(std::make_pair(
			"F7",
			UserAction{"Test action, prints \"MIAU!\" to stdout",
			           &ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>::PrintMIAU}));
	map.insert(std::make_pair(
			"grave",
			UserAction{"Toggle key-binding overlay",
			           &ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>::ToggleKeyBindingOverlay}));
	map.insert(std::make_pair(
			"q",
			UserAction{"Request shutdown of the whole application",
			           &ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>::RequestShutdown}));
	return map;
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
void ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>::PrintMIAU() {
	debug_print("MIAU!");
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
void ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>::ToggleKeyBindingOverlay() {
	if (this->bindingLayerShowing) {
		this->parent->window->HideLayer(this->keyBindingLayerIndex);
		this->bindingLayerShowing = false;
	} else {
		this->parent->window->ShowLayer(this->keyBindingLayerIndex);
		this->bindingLayerShowing = true;
	}
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
void ITMScene3DSliceVisualizerInteractorStyle<TVoxelCanonical, TVoxelLive, TIndex>::RequestShutdown() {
	ITMDynamicFusionLogger::Instance().RequestAppShutdown();
}
