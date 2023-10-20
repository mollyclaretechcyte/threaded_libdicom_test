#include <stdlib.h>
extern "C"
{
#include <dicom/dicom.h>
}

#include <future>
#include <iomanip>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <vector>
#include <string>

void print_dicom_error_and_clear(DcmError* error, std::string method) {
    dcm_error_log(error);
    const char* sum = dcm_error_get_summary(error);
    const char* messsage = dcm_error_get_message(error);
    printf("ERRORED ON %s %s %s\n", method.c_str(), sum, messsage);
    dcm_error_clear(&error);
}

DcmFilehandle* open_file(std::string file) {
    printf("OPENING FILE\n");
    DcmFilehandle* ptr;
    DcmError *error = NULL;
    ptr = dcm_filehandle_create_from_file(&error, file.c_str());
    if (ptr == NULL) {
        print_dicom_error_and_clear(error, "open");
        return NULL;
    }

    dcm_error_clear(&error);
    return ptr;
}

bool close_file(DcmFilehandle* handle) {
    printf("Closing file.\n");
    dcm_filehandle_destroy(handle);
    printf("File closed.\n");
    return true;
}

bool get_crop(DcmFilehandle * dicomFile, int frameNumber) {
    printf("GETTING CROP\n");
    DcmError *error = NULL;
    DcmFrame* frame = dcm_filehandle_read_frame(&error, dicomFile, frameNumber);
    if (!frame) {
        print_dicom_error_and_clear(error, "crop");
        return false;
    }

    dcm_error_clear(&error);
    const char* dicomFramePixels = dcm_frame_get_value(frame);
    printf("DESTROYING FRAME\n");
    dcm_frame_destroy(frame);
    return true;
}

bool get_frames_threaded(std::string input_file) {
    bool any_failures = false;
    int numFrames = 2;

    std::vector<std::future<DcmFilehandle*>> open_file_futures;
    DcmFilehandle * handle = open_file(input_file);
    if (!handle) {
        return false;
    }
    std::vector<std::future<bool>> get_crop_futures;
    for (int i = 1; i <= numFrames; ++i) {
        get_crop_futures.emplace_back(std::async(get_crop, handle, i));
    }

    for (auto& f : get_crop_futures) {
        auto result = f.get();
        if (!result) {
            any_failures = true;
        }
    }
    if (!close_file(handle)) {
        any_failures = true;
    }

    printf("done!\n");

    return !any_failures;
}

bool get_frames_single_thread(std::string input_file) {

    DcmFilehandle *dicomFile = open_file(input_file);
    if (!dicomFile) {
        return false;
    }

    bool cropSuccess = get_crop(dicomFile, 1);
    if (!cropSuccess) {
        return false;
    }

    bool crop2Success = get_crop(dicomFile, 2);
    if (!crop2Success) {
        return false;
    }

    return close_file(dicomFile);
}

int main() {
    std::string file_name = "/tmp/test_data/DCM_0.dcm";
    if (!get_frames_single_thread(file_name)) {
        printf("Getting frames with a single thread failed\n");
    }
    if (!get_frames_threaded(file_name)) {
        printf("Getting frames asynchronously failed\n");
    }
    return 0;
}