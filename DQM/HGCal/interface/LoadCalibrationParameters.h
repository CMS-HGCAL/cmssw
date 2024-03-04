#ifndef __LoadCalibrationParameters__
#define __LoadCalibrationParameters__

#include <string>
#include <fstream>
#include <iostream>
#include <vector>

#include "TString.h"

class CalibrationParameterLoader {
public:
    CalibrationParameterLoader();
    void loadParameters();

    std::map<int, double> map_pedestals;
    std::map<int, std::vector<double> > map_cm_parameters;

private:
    TString csv_file_name;
}; // CalibrationParameterLoader

CalibrationParameterLoader::CalibrationParameterLoader() {
    csv_file_name = "./meta_conditions/calibration_parameters.csv";
}

void CalibrationParameterLoader::loadParameters() {
    printf("[INFO] CalibrationParameterLoader: Load calibration parameters: %s\n", csv_file_name.Data());

    std::string line;
    std::ifstream loaded_csv_file(csv_file_name.Data());

    if(loaded_csv_file.is_open()) {
        while(getline(loaded_csv_file, line)) {
            // skip comments
            if(line.find("#")!=std::string::npos) continue;

            std::size_t found_1st_index = line.find(",");
            std::size_t found_2nd_index = line.find(",", found_1st_index+1, 1);
            std::size_t found_3rd_index = line.find(",", found_2nd_index+1, 1);
            std::size_t found_4th_index = line.find(",", found_3rd_index+1, 1);

            int eleId   = std::stoi( line.substr(0,found_1st_index) );
            double pedestal  = std::stod( line.substr(found_1st_index+1, found_2nd_index) );
            double slope     = std::stod( line.substr(found_2nd_index+1, found_3rd_index) );
            double intercept = std::stod( line.substr(found_3rd_index+1, found_4th_index) );

            std::vector<double> v = {slope, intercept};
            map_pedestals[eleId] = pedestal;
            map_cm_parameters[eleId] = v;

            printf("eleId = %d, pedestal = %.3f, slope = %.3f, intercept = %6.3f\n",
                    eleId, map_pedestals[eleId], map_cm_parameters[eleId][0], map_cm_parameters[eleId][1] );
        }
        loaded_csv_file.close();
    } else {
        std::cout << "[ERROR] unable to open " << csv_file_name.Data() << std::endl;
    }
}

#endif // __LoadCalibrationParameters__
