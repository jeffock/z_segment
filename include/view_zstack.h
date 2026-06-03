// view_zstack.h
// Jeffrey Ock
// Z-Stack loading, cleaning, and 3D viewing utilities
// Dependencies: OpenCV

#pragma once

#include <opencv2/opencv.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <algorithm>

namespace fs = std::filesystem;

// -------------------------------------------------------
// STRUCTS
// -------------------------------------------------------

struct ZStack {
    std::string name;
    std::vector<cv::Mat> slices;
    int width  = 0;
    int height = 0;
    int depth  = 0;
};

struct ZStackCollection {
    std::vector<ZStack> stacks;
    int count = 0;

    // Access by index
    ZStack& operator[](int i) { return stacks[i]; }
    const ZStack& operator[](int i) const { return stacks[i]; }
};


// -------------------------------------------------------
// 1. LOAD Z-STACKS FROM DIRECTORY
// -------------------------------------------------------
// Expects files named like: <group_id>_<slice_index>.png
// e.g. organoid_01_000.png, organoid_01_001.png, organoid_02_000.png
// Returns a ZStackCollection where each ZStack is one group.

inline ZStackCollection load_zstacks(const std::string& dir_path)
{
    ZStackCollection collection;

    // Valid Directory checking
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        std::cerr << "[load_zstacks] ERROR: Invalid directory: " << dir_path << "\n";
        return collection;
    }

    // Group files by prefix (everything before the last underscore + number)
    std::map<std::string, std::vector<fs::path>> groups;
    std::regex slice_pattern(R"(^(.+)_(\d+)\.(tif|tiff)$)");

    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (!entry.is_regular_file()) continue;

        std::string filename = entry.path().filename().string();
        std::smatch match;

        if (std::regex_match(filename, match, slice_pattern)) {
            std::string group_id = match[1].str();
            groups[group_id].push_back(entry.path());
        }
    }

    if (groups.empty()) {
        std::cerr << "[load_zstacks] WARNING: No matching files found in: " << dir_path << "\n";
        std::cerr << "  Files should be named: <group>_<slice_index>.png\n";
        std::cerr << "  Run clean_directory() first if files are not yet renamed.\n";
        return collection;
    }

    // Build a ZStack per group
    for (auto& [group_id, paths] : groups) {
        // Sort paths by filename to ensure correct slice order
        std::sort(paths.begin(), paths.end(), [](const fs::path& a, const fs::path& b) {
            return a.filename().string() < b.filename().string();
        });

        ZStack zstack;
        zstack.name = group_id;

        for (const auto& p : paths) {
            cv::Mat slice = cv::imread(p.string(), cv::IMREAD_GRAYSCALE);
            if (slice.empty()) {
                std::cerr << "[load_zstacks] WARNING: Could not read: " << p << "\n";
                continue;
            }
            zstack.slices.push_back(slice);
        }

        if (!zstack.slices.empty()) {
            zstack.width  = zstack.slices[0].cols;
            zstack.height = zstack.slices[0].rows;
            zstack.depth  = static_cast<int>(zstack.slices.size());
            collection.stacks.push_back(zstack);
        }
    }

    collection.count = static_cast<int>(collection.stacks.size());
    std::cout << "[load_zstacks] Loaded " << collection.count << " z-stack(s) from: " << dir_path << "\n";
    
    // Print example group to verify correct loading
    if (!collection.stacks.empty()) {
        const ZStack& example = collection.stacks[0];
        std::cout << "[load_zstacks] Example group:\n";
        std::cout << "  Name:   " << example.name << "\n";
        std::cout << "  Size:   " << example.width << "x" << example.height << "\n";
        std::cout << "  Depth:  " << example.depth << " slices\n";
        std::cout << "  Slices:\n";
        int preview = std::min(example.depth, 5);
        for (int i = 0; i < preview; i++) {
            std::cout << "    [" << i << "] " << example.width << "x" << example.height
                      << " channels=" << example.slices[i].channels() << "\n";
        }
        if (example.depth > 5)
            std::cout << "    ... and " << example.depth - 5 << " more slices\n";
    }

    return collection;
}

// -------------------------------------------------------
// 3. VIEW Z-STACK (3D MAX INTENSITY PROJECTION)
// -------------------------------------------------------
// Renders a chosen z-stack from the collection as:
//   - XY projection (top-down)
//   - XZ projection (side view)
//   - YZ projection (front view)
// Also allows interactive slice-by-slice browsing with arrow keys.
//
// Controls:
//   LEFT/RIGHT arrow : browse slices
//   Q or ESC         : quit

inline void view_zstack(const ZStackCollection& collection, int stack_index)
{
    if (stack_index < 0 || stack_index >= collection.count) {
        std::cerr << "[view_zstack] ERROR: stack_index " << stack_index
                  << " out of range (0-" << collection.count - 1 << ")\n";
        return;
    }

    const ZStack& zs = collection.stacks[stack_index];

    if (zs.slices.empty()) {
        std::cerr << "[view_zstack] ERROR: Z-stack '" << zs.name << "' has no slices.\n";
        return;
    }

    int W = zs.width;
    int H = zs.height;
    int D = zs.depth;

    std::cout << "[view_zstack] Viewing '" << zs.name << "' ("
              << W << "x" << H << "x" << D << ")\n";
    std::cout << "  Controls: A/D or LEFT/RIGHT = browse slices | Q/ESC = quit\n";

    // --- Normalize a slice to 8-bit for display ---
    auto to_8bit = [](const cv::Mat& src) -> cv::Mat {
        cv::Mat dst;
        if (src.depth() == CV_8U) {
            dst = src.clone();
        } else {
            // 16-bit or float — normalize to 0-255
            cv::normalize(src, dst, 0, 255, cv::NORM_MINMAX);
            dst.convertTo(dst, CV_8U);
        }
        return dst;
    };

    // --- Compute Max Intensity Projections ---

    // XY projection (collapse Z axis) — use MIN
    cv::Mat proj_xy = cv::Mat::ones(H, W, CV_32F) * 1e9f;
    for (const auto& slice : zs.slices) {
        cv::Mat f; slice.convertTo(f, CV_32F);
        cv::min(proj_xy, f, proj_xy);
    }

    // XZ projection (collapse Y axis) — use MIN for inverted contrast images
    cv::Mat proj_xz = cv::Mat::ones(D, W, CV_32F) * 1e9f;
    for (int z = 0; z < D; z++) {
        cv::Mat f; zs.slices[z].convertTo(f, CV_32F);
        for (int x = 0; x < W; x++) {
            float min_val = 1e9f;
            for (int y = 0; y < H; y++) {
                float val = f.at<float>(y, x);
                if (val < min_val) min_val = val;
            }
            proj_xz.at<float>(z, x) = min_val;
        }
    }

    // YZ projection (collapse X axis) — use MIN for inverted contrast images
    cv::Mat proj_yz = cv::Mat::ones(H, D, CV_32F) * 1e9f;
    for (int z = 0; z < D; z++) {
        cv::Mat f; zs.slices[z].convertTo(f, CV_32F);
        for (int y = 0; y < H; y++) {
            float min_val = 1e9f;
            for (int x = 0; x < W; x++) {
                float val = f.at<float>(y, x);
                if (val < min_val) min_val = val;
            }
            proj_yz.at<float>(y, z) = min_val;
        }
    }

    // Normalize all projections to 8-bit
    cv::Mat proj_xy_8, proj_xz_8, proj_yz_8;
    cv::normalize(proj_xy, proj_xy_8, 0, 255, cv::NORM_MINMAX); proj_xy_8.convertTo(proj_xy_8, CV_8U);
    cv::normalize(proj_xz, proj_xz_8, 0, 255, cv::NORM_MINMAX); proj_xz_8.convertTo(proj_xz_8, CV_8U);
    cv::normalize(proj_yz, proj_yz_8, 0, 255, cv::NORM_MINMAX); proj_yz_8.convertTo(proj_yz_8, CV_8U);

    // Scale XZ and YZ to match XY size
    cv::Mat proj_xz_resized, proj_yz_resized;
    cv::resize(proj_xz_8, proj_xz_resized, cv::Size(W, H));
    cv::resize(proj_yz_8, proj_yz_resized, cv::Size(W, H));

    // Apply colormap
    cv::Mat proj_xy_color, proj_xz_color, proj_yz_color;
    cv::applyColorMap(proj_xy_8,       proj_xy_color,  cv::COLORMAP_INFERNO);
    cv::applyColorMap(proj_xz_resized, proj_xz_color,  cv::COLORMAP_INFERNO);
    cv::applyColorMap(proj_yz_resized, proj_yz_color,  cv::COLORMAP_INFERNO);

    // Add labels
    auto add_label = [](cv::Mat& img, const std::string& label) {
        cv::putText(img, label, cv::Point(8, 22),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
    };
    add_label(proj_xy_color, "XY (top-down)");
    add_label(proj_xz_color, "XZ (side)");
    add_label(proj_yz_color, "YZ (front)");

    // Tile: [XY | XZ] / [YZ | YZ]
    cv::Mat top_row, bottom_row, mip_view;
    cv::hconcat(proj_xy_color, proj_xz_color, top_row);
    cv::hconcat(proj_yz_color, proj_yz_color, bottom_row);
    cv::vconcat(top_row, bottom_row, mip_view);

    // --- Interactive slice browser ---
    int current_slice = 0;
    const std::string mip_window   = "3D MIP - " + zs.name;
    const std::string slice_window = "Slice Browser - " + zs.name;

    cv::namedWindow(mip_window,   cv::WINDOW_NORMAL);
    cv::namedWindow(slice_window, cv::WINDOW_NORMAL);
    cv::imshow(mip_window, mip_view);

    while (true) {
        // Normalize current slice to 8-bit
        cv::Mat slice_8 = to_8bit(zs.slices[current_slice]);

        cv::Mat slice_color;
        cv::applyColorMap(slice_8, slice_color, cv::COLORMAP_INFERNO);

        std::string info = "Slice " + std::to_string(current_slice + 1)
                         + " / " + std::to_string(D)
                         + "  (A/D or arrow keys to navigate, Q to quit)";
        cv::putText(slice_color, info, cv::Point(8, 22),
                    cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 255), 1);

        cv::imshow(slice_window, slice_color);

        int key = cv::waitKey(0) & 0xFF;  // mask to 8 bits for Windows compatibility

        if (key == 'q' || key == 27)  break;                                            // Q or ESC
        if (key == 'd' || key == 39)  current_slice = std::min(current_slice + 1, D - 1); // D or RIGHT
        if (key == 'a' || key == 37)  current_slice = std::max(current_slice - 1, 0);     // A or LEFT
    }

    cv::destroyAllWindows();
}

// -------------------------------------------------------
// STRUCTS (SINGLE-FILE Z-STACK)
// -------------------------------------------------------

struct ZStackSingle {
    std::string name;
    std::vector<cv::Mat> slices;
    int width  = 0;
    int height = 0;
    int depth  = 0;
};

struct ZStackSingleCollection {
    std::vector<ZStackSingle> stacks;
    int count = 0;

    ZStackSingle& operator[](int i) { return stacks[i]; }
    const ZStackSingle& operator[](int i) const { return stacks[i]; }
};


// -------------------------------------------------------
// LOAD SINGLE-FILE Z-STACKS FROM DIRECTORY
// -------------------------------------------------------
// Each .tif file in the directory is a multi-page tif
// where each page is one Z slice.

inline ZStackSingleCollection load_zstacks_single(const std::string& dir_path)
{
    ZStackSingleCollection collection;

    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        std::cerr << "[load_zstacks_single] ERROR: Invalid directory: " << dir_path << "\n";
        return collection;
    }

    std::vector<fs::path> tif_files;
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".tif" || ext == ".tiff")
            tif_files.push_back(entry.path());
    }

    std::sort(tif_files.begin(), tif_files.end());

    if (tif_files.empty()) {
        std::cerr << "[load_zstacks_single] WARNING: No .tif files found in: " << dir_path << "\n";
        return collection;
    }

    for (const auto& path : tif_files) {
        std::vector<cv::Mat> pages;
        cv::imreadmulti(path.string(), pages, cv::IMREAD_GRAYSCALE);

        if (pages.empty()) {
            std::cerr << "[load_zstacks_single] WARNING: Could not read or no pages in: " << path << "\n";
            continue;
        }

        ZStackSingle zstack;
        zstack.name   = path.stem().string();
        zstack.slices = pages;
        zstack.width  = pages[0].cols;
        zstack.height = pages[0].rows;
        zstack.depth  = static_cast<int>(pages.size());

        collection.stacks.push_back(zstack);
    }

    collection.count = static_cast<int>(collection.stacks.size());
    std::cout << "[load_zstacks_single] Loaded " << collection.count << " z-stack(s) from: " << dir_path << "\n";

    // Print example
    if (!collection.stacks.empty()) {
        const ZStackSingle& example = collection.stacks[0];
        std::cout << "[load_zstacks_single] Example:\n";
        std::cout << "  Name:  " << example.name << "\n";
        std::cout << "  Size:  " << example.width << "x" << example.height << "\n";
        std::cout << "  Depth: " << example.depth << " slices\n";
        int preview = std::min(example.depth, 5);
        for (int i = 0; i < preview; i++) {
            std::cout << "    [" << i << "] " << example.width << "x" << example.height
                      << " channels=" << example.slices[i].channels() << "\n";
        }
        if (example.depth > 5)
            std::cout << "    ... and " << example.depth - 5 << " more slices\n";
    }

    return collection;
}


// -------------------------------------------------------
// VIEW SINGLE-FILE Z-STACK
// -------------------------------------------------------

inline void view_zstack_single(const ZStackSingleCollection& collection, int stack_index)
{
    if (stack_index < 0 || stack_index >= collection.count) {
        std::cerr << "[view_zstack_single] ERROR: stack_index " << stack_index
                  << " out of range (0-" << collection.count - 1 << ")\n";
        return;
    }

    const ZStackSingle& zs = collection.stacks[stack_index];

    if (zs.slices.empty()) {
        std::cerr << "[view_zstack_single] ERROR: Z-stack '" << zs.name << "' has no slices.\n";
        return;
    }

    int W = zs.width;
    int H = zs.height;
    int D = zs.depth;

    std::cout << "[view_zstack_single] Viewing '" << zs.name << "' ("
              << W << "x" << H << "x" << D << ")\n";
    std::cout << "  Controls: A/D = browse slices | Q/ESC = quit\n";

    auto to_8bit = [](const cv::Mat& src) -> cv::Mat {
        cv::Mat dst;
        if (src.depth() == CV_8U) {
            dst = src.clone();
        } else {
            cv::normalize(src, dst, 0, 255, cv::NORM_MINMAX);
            dst.convertTo(dst, CV_8U);
        }
        return dst;
    };

    // XY min projection
    cv::Mat proj_xy = cv::Mat::ones(H, W, CV_32F) * 1e9f;
    for (const auto& slice : zs.slices) {
        cv::Mat f; slice.convertTo(f, CV_32F);
        cv::min(proj_xy, f, proj_xy);
    }

    // XZ min projection
    cv::Mat proj_xz = cv::Mat::ones(D, W, CV_32F) * 1e9f;
    for (int z = 0; z < D; z++) {
        cv::Mat f; zs.slices[z].convertTo(f, CV_32F);
        for (int x = 0; x < W; x++) {
            float min_val = 1e9f;
            for (int y = 0; y < H; y++) {
                float val = f.at<float>(y, x);
                if (val < min_val) min_val = val;
            }
            proj_xz.at<float>(z, x) = min_val;
        }
    }

    // YZ min projection
    cv::Mat proj_yz = cv::Mat::ones(H, D, CV_32F) * 1e9f;
    for (int z = 0; z < D; z++) {
        cv::Mat f; zs.slices[z].convertTo(f, CV_32F);
        for (int y = 0; y < H; y++) {
            float min_val = 1e9f;
            for (int x = 0; x < W; x++) {
                float val = f.at<float>(y, x);
                if (val < min_val) min_val = val;
            }
            proj_yz.at<float>(y, z) = min_val;
        }
    }

    // Normalize to 8-bit
    cv::Mat proj_xy_8, proj_xz_8, proj_yz_8;
    cv::normalize(proj_xy, proj_xy_8, 0, 255, cv::NORM_MINMAX); proj_xy_8.convertTo(proj_xy_8, CV_8U);
    cv::normalize(proj_xz, proj_xz_8, 0, 255, cv::NORM_MINMAX); proj_xz_8.convertTo(proj_xz_8, CV_8U);
    cv::normalize(proj_yz, proj_yz_8, 0, 255, cv::NORM_MINMAX); proj_yz_8.convertTo(proj_yz_8, CV_8U);

    // Resize XZ and YZ to match XY
    cv::Mat proj_xz_resized, proj_yz_resized;
    cv::resize(proj_xz_8, proj_xz_resized, cv::Size(W, H));
    cv::resize(proj_yz_8, proj_yz_resized, cv::Size(W, H));

    // Colormap
    cv::Mat proj_xy_color, proj_xz_color, proj_yz_color;
    cv::applyColorMap(proj_xy_8,        proj_xy_color, cv::COLORMAP_INFERNO);
    cv::applyColorMap(proj_xz_resized,  proj_xz_color, cv::COLORMAP_INFERNO);
    cv::applyColorMap(proj_yz_resized,  proj_yz_color, cv::COLORMAP_INFERNO);

    auto add_label = [](cv::Mat& img, const std::string& label) {
        cv::putText(img, label, cv::Point(8, 22),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
    };
    add_label(proj_xy_color, "XY (top-down)");
    add_label(proj_xz_color, "XZ (side)");
    add_label(proj_yz_color, "YZ (front)");

    cv::Mat top_row, bottom_row, mip_view;
    cv::hconcat(proj_xy_color, proj_xz_color, top_row);
    cv::hconcat(proj_yz_color, proj_yz_color, bottom_row);
    cv::vconcat(top_row, bottom_row, mip_view);

    int current_slice = 0;
    const std::string mip_window   = "3D MIP - " + zs.name;
    const std::string slice_window = "Slice Browser - " + zs.name;

    cv::namedWindow(mip_window,   cv::WINDOW_NORMAL);
    cv::namedWindow(slice_window, cv::WINDOW_NORMAL);
    cv::imshow(mip_window, mip_view);

    while (true) {
        cv::Mat slice_8 = to_8bit(zs.slices[current_slice]);
        cv::Mat slice_color;
        cv::applyColorMap(slice_8, slice_color, cv::COLORMAP_INFERNO);

        std::string info = "Slice " + std::to_string(current_slice + 1)
                         + " / " + std::to_string(D)
                         + "  (A/D to navigate, Q to quit)";
        cv::putText(slice_color, info, cv::Point(8, 22),
                    cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 255), 1);

        cv::imshow(slice_window, slice_color);

        int key = cv::waitKey(0) & 0xFF;
        if (key == 'q' || key == 27)  break;
        if (key == 'd' || key == 39)  current_slice = std::min(current_slice + 1, D - 1);
        if (key == 'a' || key == 37)  current_slice = std::max(current_slice - 1, 0);
    }

    cv::destroyAllWindows();
}