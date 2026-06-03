// MAINLOOP
// Jeffrey Ock
// 05/20/26
// DATASET:
// https://www.kaggle.com/datasets/burakkahveci/brain-organoids-segmentation-dataset

// TODO
// - impl view_zstack.h


//---------- HEADERS ----------//
#include "view_zstack.h"
#include <iostream>
#include <string>

// FILE PATHS
std::string img_path = "C:/Users/jeffo/Coding/organoid_z_segmentation/dataset/Segmentation/BO-WoAugmentation/img";
std::string seg_path = "C:/Users/jeffo/Coding/organoid_z_segmentation/dataset/Segmentation/BO-WoAugmentation/seg";
std::string fullstack_path = "C:/Users/jeffo/Coding/organoid_z_segmentation/dataset/fullstacks";

int main()
{
	// load
	//ZStackCollection collection = load_zstacks(img_path);
	// view first stack
	//view_zstack(collection, 5);

	ZStackSingleCollection collection = load_zstacks_single(fullstack_path);
	view_zstack_single(collection, 2);

	return 0;
}
