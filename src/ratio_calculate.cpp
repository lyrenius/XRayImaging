#include <fitsio.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <tuple>
#include <cmath>

using std::max;
using std::min;

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
float ratio[LEN][LEN];
float Rval[LEN][LEN];
std::vector<std::pair<float,int>> PSF_list;
std::vector<int> source_x;
std::vector<int> source_y;
std::vector<double> source_R;

inline float square_sum(float x,float y)
{
    return x*x + y*y;
}

float PSF_frac_calc(float x,float y,float delta_x, float delta_y)
{
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

    float exponent = square_sum( (delta_x * cos_angle + delta_y * sin_angle) / sigma_major,
                            (delta_x * sin_angle - delta_y * cos_angle) / sigma_minor ) / 2;

    return normalization_factor * exp(-exponent);
}

void work()
{
    constexpr int ITERATION_COUNT = 10;

    for(int x = 0; x < LEN; x++) {
        for(int y = 0; y < LEN; y++) {
            PSF_list.clear();

            for(int i = max(0, x - PSF_SIZE / 2); i < min(LEN, x + PSF_SIZE / 2 + 1); i++) {
                for(int j = max(0, y - PSF_SIZE / 2); j < min(LEN, y + PSF_SIZE / 2 + 1); j++) {
                    if(count[i][j]) {
                        PSF_list.emplace_back(PSF_frac_calc(x, y, i - x, j - y), count[i][j]);
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
    }
}

void detection()
{
    constexpr float threshold = 8.0;

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
}

void read_data()
{
    fitsfile *fptr = nullptr;
    int status = 0;
    int hdutype = 0;
    long nrows = 0;
    int col_x = 0, col_y = 0;
    int anynul = 0;

    std::cout << "reading" << std::endl;

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
}

void write_result()
{
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
}

int main()
{
    read_data();
    work();
    detection();
    write_result();
    return 0;
}