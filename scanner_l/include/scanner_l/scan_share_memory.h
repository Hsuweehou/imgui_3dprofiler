#ifndef SCAN_SHARE_MEMORY_H
#define SCAN_SHARE_MEMORY_H

#pragma once

#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <opencv2/opencv.hpp>
#include <io.h>
#include <winsock2.h>
#include <windows.h>
#include "glog/logging.h"

HANDLE shared_file_handler_gray = NULL;
HANDLE shared_file_handler_pc = NULL;
HANDLE hFile_gray = NULL;
HANDLE hFile_pc = NULL;

void readMemory_gray(const std::string obj_name,const std::string file_name, size_t total_size) {
    //char *shared_object_name = "Local\\object_name_sean";
    //TCHAR shared_object_name[30] = TEXT("Local\\object_name_sean");
    //TCHAR shared_file_name[30] = TEXT("file_name_sean");

    HANDLE shared_file_handler = OpenFileMapping(FILE_MAP_READ, FALSE, obj_name.c_str());
    if (shared_file_handler == NULL) {
        std::cerr << "OpenFileMapping failed with error: " << GetLastError()
                  << std::endl;
      }
    // open shared memory file

    if (shared_file_handler) {

        void* pBuf = MapViewOfFile(
            shared_file_handler,
            FILE_MAP_READ,
            0,
            0,
            total_size);
        if (pBuf == NULL) {
            std::cerr << "MapViewOfFile failed with error: " << GetLastError()
                << std::endl;
            CloseHandle(shared_file_handler);
        }
        // copy shared data from memory
        LOG(INFO) << "read shared data: \n";

        char* ptr = static_cast<char*>(pBuf);
        // --- READ HEADER ---
        int width, height, channels, mat_type;

        memcpy(&width, ptr, sizeof(int));
        ptr += sizeof(int);
        memcpy(&height, ptr, sizeof(int));
        ptr += sizeof(int);
        memcpy(&channels, ptr, sizeof(int));
        ptr += sizeof(int);
        memcpy(&mat_type, ptr, sizeof(int));  // Read the full type
        ptr += sizeof(int);

        // --- CORRECTLY CREATE MAT AND COPY DATA ---
        cv::Mat normalized_img(height, width, CV_8UC1);
        memcpy(normalized_img.data, ptr, height * width * channels);
        // Calculate the expected data size from the metadata we just read
        size_t data_size = normalized_img.total() * normalized_img.elemSize();

        // Sanity check
        if (data_size + (4 * sizeof(int)) != total_size) {
            std::cerr << "Error: Mismatch in expected vs actual buffer size."
                << std::endl;

        UnmapViewOfFile(pBuf);
        CloseHandle(shared_file_handler);
        }
        std::vector<int> compression_params = { cv::IMWRITE_TIFF_COMPRESSION, 1 };
        cv::imwrite(file_name, normalized_img, compression_params);

        UnmapViewOfFile(pBuf);
        CloseHandle(shared_file_handler);
    }
    else
        LOG(ERROR) << "open mapping file error\n";
}

int writeMemory_gray(cv::Mat normalized_img, const std::string obj_name, const std::string file_name, uint64& mem_size) {
    // define shared data
    //TCHAR shared_object_name[30] = TEXT("Local\\object_name_sean");
    //TCHAR shared_file_name[30] = TEXT("file_name_sean");

    //cv::Mat normalized_img = imread(imgDir, cv::IMREAD_GRAYSCALE);

    LOG(INFO) << "Img type: Gray\n";
      // --- METADATA TO WRITE ---
      int width = normalized_img.cols;
      int height = normalized_img.rows;
      int channels = normalized_img.channels();
      int mat_type = normalized_img.type();  // CRUCIAL: Get the exact matrix type

      // --- ROBUST SIZE CALCULATION ---
      size_t data_size = normalized_img.total() * normalized_img.elemSize();
      // Header: width, height, channels, mat_type
      size_t header_size = 4 * sizeof(int);
      size_t buffer_size = data_size + header_size;

      // (Assuming mem_file_handle_... is a member variable)
      HANDLE& shared_file_handler = shared_file_handler_gray;

      shared_file_handler = CreateFileMapping(
          INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
          static_cast<DWORD>(buffer_size), obj_name.c_str());

      if (!shared_file_handler) {
        std::cerr << "CreateFileMapping failed with error: " << GetLastError()
                  << std::endl;
        return 0;
      }

      // --- EFFICIENT WRITING ---
      // No need for a temporary buffer, write directly to shared memory
      LPVOID lp_base = MapViewOfFile(shared_file_handler, FILE_MAP_ALL_ACCESS,
                                     0, 0, buffer_size);
      if (lp_base == NULL) {
        std::cerr << "MapViewOfFile failed with error: " << GetLastError()
                  << std::endl;
        CloseHandle(shared_file_handler);
        return 0;
      }

      // Use a char* for safe pointer arithmetic
      char* ptr = static_cast<char*>(lp_base);

      // Write header information
      memcpy(ptr, &width, sizeof(int));
      ptr += sizeof(int);
      memcpy(ptr, &height, sizeof(int));
      ptr += sizeof(int);
      memcpy(ptr, &channels, sizeof(int));
      ptr += sizeof(int);
      memcpy(ptr, &mat_type, sizeof(int));  // Write the full type
      ptr += sizeof(int);

      // Write the actual pixel data
      memcpy(ptr, normalized_img.data, data_size);
      mem_size = buffer_size;
      FlushViewOfFile(lp_base, buffer_size);
      UnmapViewOfFile(lp_base);

      return 0;
}

void readMemory_pc(const std::string obj_name, const std::string file_name, size_t total_size) {
    //char *shared_object_name = "Local\\object_name_sean";
    //TCHAR shared_object_name[30] = TEXT("Local\\object_name_scan1");
    //TCHAR shared_file_name[30] = TEXT("file_name_sean");

    HANDLE shared_file_handler = OpenFileMapping(FILE_MAP_READ, FALSE, obj_name.c_str());
    if (shared_file_handler == NULL) {
        std::cerr << "OpenFileMapping failed with error: " << GetLastError()
                  << std::endl;
      }
    // open shared memory file

    if (shared_file_handler) {

        void* pBuf = MapViewOfFile(
            shared_file_handler,
            FILE_MAP_READ,
            0,
            0,
            total_size);

        if (pBuf == NULL) {
            std::cerr << "MapViewOfFile failed with error: " << GetLastError()
                << std::endl;
            CloseHandle(shared_file_handler);
        }

        // copy shared data from memory
        LOG(INFO) << "read shared data: \n";

        char* ptr = static_cast<char*>(pBuf);
        // --- READ HEADER ---
        int width, height, channels, mat_type;

        memcpy(&width, ptr, sizeof(int));
        ptr += sizeof(int);
        memcpy(&height, ptr, sizeof(int));
        ptr += sizeof(int);
        memcpy(&channels, ptr, sizeof(int));
        ptr += sizeof(int);
        memcpy(&mat_type, ptr, sizeof(int));  // Read the full type
        ptr += sizeof(int);

        // --- CORRECTLY CREATE MAT AND COPY DATA ---
        cv::Mat normalized_img(height, width, mat_type);
        memcpy(normalized_img.data, ptr, sizeof(float) * height * width * channels);
        // Calculate the expected data size from the metadata we just read
        size_t data_size = normalized_img.total() * normalized_img.elemSize();

        // Sanity check
        LOG(INFO) << "data_size: " << data_size;
        LOG(INFO) << "(4 * sizeof(int)): " << (4 * sizeof(int));
        LOG(INFO) << "total_size： " << total_size;
        if (data_size + (4 * sizeof(int)) != total_size) {
            std::cerr << "Error: Mismatch in expected vs actual buffer size."
                << std::endl;

        UnmapViewOfFile(pBuf);
        CloseHandle(shared_file_handler);
        }
        std::vector<int> compression_params = { cv::IMWRITE_TIFF_COMPRESSION, 1 };
        cv::imwrite(file_name, normalized_img, compression_params);
        UnmapViewOfFile(pBuf);
        CloseHandle(shared_file_handler);
    }
    else
        LOG(ERROR) << "open mapping file error\n";
}

int writeMemory_pc(cv::Mat img, const std::string obj_name, const std::string file_name, uint64& mem_size) {
    // define shared data
    //TCHAR shared_object_name[30] = TEXT("Local\\object_name_scan1");
    //TCHAR shared_file_name[30] = TEXT("file_name_sean");

    LOG(INFO) << "Img type: pointcloud\n";
    //cv::Mat img = cv::imread(imgDir, cv::IMREAD_ANYDEPTH | cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        std::cerr << "Failed to read TIFF file\n";
        return -1;
    }

    int width = img.cols;
    int height = img.rows;
    int channels = img.channels();
    int mat_type =
          img.type();  // CRUCIAL: Get the exact matrix type

      // --- ROBUST SIZE CALCULATION ---
      size_t data_size = img.total() * img.elemSize();
      // Header: width, height, channels, mat_type
      size_t header_size = 4 * sizeof(int);
      size_t buffer_size = data_size + header_size;

    LOG(INFO) << "share buffer \n";

      // (Assuming mem_file_handle_... is a member variable)
      HANDLE& shared_file_handler = shared_file_handler_pc;

      shared_file_handler = CreateFileMapping(
          INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
          static_cast<DWORD>(buffer_size), obj_name.c_str());

      if (shared_file_handler) {

      // --- EFFICIENT WRITING ---
      // No need for a temporary buffer, write directly to shared memory
      LPVOID lp_base = MapViewOfFile(shared_file_handler, FILE_MAP_ALL_ACCESS,
                                     0, 0, buffer_size);
      if (lp_base == NULL) {
        std::cerr << "MapViewOfFile failed with error: " << GetLastError()
                  << std::endl;
        CloseHandle(shared_file_handler);
        return 0;
      }

      // Use a char* for safe pointer arithmetic
      char* ptr = static_cast<char*>(lp_base);

      // Write header information
      memcpy(ptr, &width, sizeof(int));
      ptr += sizeof(int);
      memcpy(ptr, &height, sizeof(int));
      ptr += sizeof(int);
      memcpy(ptr, &channels, sizeof(int));
      ptr += sizeof(int);
      memcpy(ptr, &mat_type, sizeof(int));  // Write the full type
      ptr += sizeof(int);

      // Write the actual pixel data
      memcpy(ptr, img.data, data_size);

      FlushViewOfFile(lp_base, buffer_size);
      UnmapViewOfFile(lp_base);
        // process wait here for other task to read data
        LOG(INFO) << "already write to shared memory, wait ...\n";
        mem_size = buffer_size;
        // CloseHandle(hFile);
        //SetFilePointer(hFile, 0, FILE_BEGIN);
        //SetEndOfFile(hFile);
        LOG(INFO) << "shared memory closed\n";
    }
    else
        LOG(ERROR) << "create mapping file error\n";
    return 0;
}


#endif