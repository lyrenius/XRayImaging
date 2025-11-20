#include <fitsio.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <tuple>
#include <cmath>
#include <chrono>
#include <thread>
#include <pthread.h>

#ifndef NUM_THREADS
#define NUM_THREADS 16
#endif

using std::max;
using std::min;

constexpr int STEP = 5;
constexpr float LIMIT = 6.0;
constexpr float threshold = 8.0;
constexpr int SMALL_COUNT_LIMIT = 9;

constexpr int ITERATION_COUNT = 10;


constexpr int LEN = 512;
constexpr int PSF_SIZE = 15;

constexpr float bkg_rate = 1e-5;
constexpr int TIME = 1000;

#define CHECK_STATUS(status)                         \
  do {                                               \
    if (status) {                                    \
      fits_report_error(stderr, status);             \
      std::exit(EXIT_FAILURE);                       \
    }                                                \
  } while (0)

int count[LEN][LEN];
int sum_count[LEN][LEN];
float ratio[LEN][LEN];
float Rval[LEN][LEN];
std::vector<int> source_x;
std::vector<int> source_y;
std::vector<double> source_R;

// Function to pin the thread
void set_cpu_affinity(std::thread& t, int cpu_id) {
    // 1. Define the CPU set (the mask)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset); // Only allow running on cpu_id

    // 2. Get the native handle (pthread_t)
    pthread_t native_handle = t.native_handle();

    // 3. Call the OS affinity function
    pthread_setaffinity_np(
        native_handle,
        sizeof(cpu_set_t),
        &cpuset
    );
}

inline float square_sum(float x,float y)
{
    return x * x + y * y;
}

void calc(int x, int y)
{
    int min_x = max(0, x - PSF_SIZE / 2);
    int max_x = min(LEN - 1, x + PSF_SIZE / 2);
    int min_y = max(0, y - PSF_SIZE / 2);
    int max_y = min(LEN - 1, y + PSF_SIZE / 2);

    if(sum_count[max_x][max_y]
       - (min_x > 0 ? sum_count[min_x - 1][max_y] : 0)
       - (min_y > 0 ? sum_count[max_x][min_y - 1] : 0)
       + (min_x > 0 && min_y > 0 ? sum_count[min_x - 1][min_y - 1] : 0)
       < SMALL_COUNT_LIMIT) {
        ratio[x][y] = -1e9;
        Rval[x][y] = 0;
        return;
    }

    constexpr float center_x = 256;
    constexpr float center_y = 256;

    float dx = center_x - x - 0.5;
    float dy = center_y - y - 0.5;
    float distance = sqrt(square_sum(dx,dy));
    
    constexpr float max_distance = 362.039; //256*sqrt(2)
    float eccentricity = 0.9 * (distance / max_distance);

    float sigma_minor = 0.5 + (distance / max_distance) * 2.5;
    float sigma_major = sigma_minor / sqrt(1 - eccentricity * eccentricity);

    float angle = atan2(dy, dx);
    float cos_angle = cos(angle);
    float sin_angle = sin(angle);

    float normalization_factor = 1 / (2 * M_PI * sigma_major * sigma_minor);

    std::vector<std::pair<float,int>> PSF_list;

    PSF_list.clear();

    for(int i = max(0, x - PSF_SIZE / 2); i < min(LEN, x + PSF_SIZE / 2 + 1); i++) {
        for(int j = max(0, y - PSF_SIZE / 2); j < min(LEN, y + PSF_SIZE / 2 + 1); j++) {
            if(count[i][j]) {
                int delta_x = i - x;
                int delta_y = j - y;
                float exponent = square_sum( (delta_x * cos_angle + delta_y * sin_angle) / sigma_major,
                                               (delta_x * sin_angle - delta_y * cos_angle) / sigma_minor ) / 2;
                float psf_value = normalization_factor * exp(-exponent);
                PSF_list.emplace_back(psf_value, count[i][j]);
            }
        }
    }

    float R = 0.05;

    for(int iter = 0; iter < ITERATION_COUNT; iter++) {
        float tmp_R = 0;
        for(auto [s, c] : PSF_list) {
            tmp_R += c * (R * s) / ((R * s + bkg_rate) * TIME);
        }
        R = tmp_R;
    }

    Rval[x][y] = R;
    
    float res = 0;
    for(auto [s, c] : PSF_list) {
        res += c * log((R * s + bkg_rate) / bkg_rate);
    }

    ratio[x][y] = res - TIME * R;
}

void calc_sum()
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    for(int i = 0; i < LEN; i++) {
        for(int j = 0; j < LEN; j++) {
            sum_count[i][j] = count[i][j];
            if(i > 0) sum_count[i][j] += sum_count[i - 1][j];
            if(j > 0) sum_count[i][j] += sum_count[i][j - 1];
            if(i > 0 && j > 0) sum_count[i][j] -= sum_count[i - 1][j - 1];
        }
    }
    
    auto EndTime = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> Elapsed = EndTime - StartTime;

    std::cerr << "Calc sum time: " << Elapsed.count() * 1000.0 << " ms" << std::endl;
}

void preworker(int id)
{
    for(int x = 0; x < LEN / STEP; x++) {
        for(int y = 0; y < LEN / STEP; y++) {
            if((x * LEN + y) % NUM_THREADS == id) {
                calc(x * STEP, y * STEP);
            }
        }
    }
}

void prework()
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;

    for(int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(preworker, i);
        set_cpu_affinity(threads.back(), i);
    }

    for(auto& th : threads) {
        th.join();
    }

    auto EndTime = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> Elapsed = EndTime - StartTime;

    std::cerr << "Prework time: " << Elapsed.count() * 1000.0 << " ms" << std::endl;
}

void work()
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    for(int x = 0; x < LEN; x += STEP) {
        for(int y = 0; y < LEN; y += STEP) {
            if(ratio[x][y] >= LIMIT) {
                for(int dx = -STEP + 1; dx < STEP; dx++) {
                    for(int dy = -STEP + 1; dy < STEP; dy++) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if(nx >= 0 && nx < LEN && ny >= 0 && ny < LEN) {
                            calc(nx, ny);
                        }
                    }
                }
            }
        }
    }

    auto EndTime = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> Elapsed = EndTime - StartTime;

    std::cerr << "Work time: " << Elapsed.count() * 1000.0 << " ms" << std::endl;
}

void detection()
{
    auto StartTime = std::chrono::high_resolution_clock::now();
    
    for(int x = 10; x < 500; x++) {
        for(int y = 10; y < 500; y++) {
            if(ratio[x][y] >= threshold) {
                bool ok = 1;
                for(int i = x - 10; i <= x + 10; i++) {
                    for(int j = y - 10; j <= y + 10; j++) {
                        if(ratio[i][j] > ratio[x][y]) {
                            ok = 0;
                            break;
                        }
                    }
                }
                if(ok) {
                    source_x.push_back(x);
                    source_y.push_back(y);
                    source_R.push_back(Rval[x][y]);
                }
            }
        }
    }

    auto EndTime = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> Elapsed = EndTime - StartTime;

    std::cerr << "Detection time: " << Elapsed.count() * 1000.0 << " ms" << std::endl;
}

void read_data()
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    fitsfile *fptr = nullptr;
    int status = 0;
    int hdutype = 0;
    long nrows = 0;
    int col_x = 0, col_y = 0;
    int anynul = 0;

    const char* submit_dir = std::getenv("SLURM_SUBMIT_DIR");

    std::string mock_file = std::string(submit_dir) + "/data/mock_data.fits";
    fits_open_file(&fptr, mock_file.c_str(), READONLY, &status);
    CHECK_STATUS(status);

    // Python hdu[1] -> CFITSIO HDU
    fits_movabs_hdu(fptr, 2, &hdutype, &status);
    CHECK_STATUS(status);

    fits_get_num_rows(fptr, &nrows, &status);
    CHECK_STATUS(status);

    fits_get_colnum(fptr, CASEINSEN, const_cast<char *>("x"), &col_x, &status);
    CHECK_STATUS(status);
    fits_get_colnum(fptr, CASEINSEN, const_cast<char *>("y"), &col_y, &status);
    CHECK_STATUS(status);

    std::vector<int> x(nrows), y(nrows);

    fits_read_col(fptr, TINT, col_x, 1, 1, nrows, nullptr,
                    x.data(), &anynul, &status);
    CHECK_STATUS(status);
    fits_read_col(fptr, TINT, col_y, 1, 1, nrows, nullptr,
                    y.data(), &anynul, &status);
    CHECK_STATUS(status);

    fits_close_file(fptr, &status);
    CHECK_STATUS(status);

    for(int i = 0; i < nrows; i++) {
        count[x[i]][y[i]]++;
    }

    auto EndTime = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> Elapsed = EndTime - StartTime;

    std::cerr << "Read time: " << Elapsed.count() * 1000.0 << " ms" << std::endl;
}

void write_result()
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    fitsfile *fptr = nullptr;
    int status = 0;

    // "!" to overwrite existing file
    fits_create_file(&fptr, "!data/detection_info.fits", &status);
    CHECK_STATUS(status);

    // primary HDU
    fits_create_img(fptr, 8, 0, nullptr, &status);  // BITPIX=8
    CHECK_STATUS(status);

    fits_write_comment(fptr,
                        const_cast<char *>("This file storages the info of detected sources"),
                        &status);
    CHECK_STATUS(status);

    // binary table
    char *ttype[] = {
        const_cast<char *>("x"),
        const_cast<char *>("y"),
        const_cast<char *>("countrate")
    };

    // In Python you used 'I', 'I', 'D'. If you want exact layout, use "1I","1I","1D"
    // Here we keep the same: x,y as unsigned 16-bit int, R as float64.
    char *tform[] = {
        const_cast<char *>("1I"),
        const_cast<char *>("1I"),
        const_cast<char *>("1D")
    };

    char *tunit[] = {
        const_cast<char *>(""),
        const_cast<char *>(""),
        const_cast<char *>("")
    };

    int nrows = source_R.size();
    
    fits_create_tbl(fptr, BINARY_TBL, nrows, 3,
                    ttype, tform, tunit,
                    const_cast<char *>("DETECTION_INFO"), &status);
    CHECK_STATUS(status);

    fits_write_col(fptr, TINT, 1, 1, 1, nrows,
                    source_x.data(), &status);
    CHECK_STATUS(status);

    fits_write_col(fptr, TINT, 2, 1, 1, nrows,
                    source_y.data(), &status);
    CHECK_STATUS(status);

    fits_write_col(fptr, TDOUBLE, 3, 1, 1, nrows,
                    source_R.data(), &status);
    CHECK_STATUS(status);

    fits_close_file(fptr, &status);
    CHECK_STATUS(status);

    auto EndTime = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> Elapsed = EndTime - StartTime;

    std::cerr << "Write time: " << Elapsed.count() * 1000.0 << " ms" << std::endl;
}

void print_grid()
{
    std::ofstream fout("data/detected_ratio.txt");

    for(int i = 0; i < LEN; i++) {
        for(int j = 0; j < LEN; j++) {
            fout << ratio[i][j] << " ";
        }
        fout << std::endl;
    }
}

int main()
{
    std::cerr << "Using " << NUM_THREADS << " threads." << std::endl;
    read_data();
    calc_sum();
    prework();
    work();
    detection();
    write_result();
    // print_grid();
    return 0;
}