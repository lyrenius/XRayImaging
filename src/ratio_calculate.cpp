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

constexpr int STEP = 6;
constexpr float LIMIT = 6.0;
constexpr float threshold = 8.0;
constexpr int SMALL_COUNT_LIMIT = 9;

constexpr int ITERATION_COUNT = 10;


constexpr int LEN = 512;
constexpr int PSF_SIZE = 13;

constexpr float bkg_rate = 1e-5;
constexpr int TIME = 1000;

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

  if (sum_count[max_x][max_y]
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
  constexpr float max_distance = 362.039f;
  constexpr float inv_max_distance = 1.0f / max_distance;

  float dx = center_x - x - 0.5f;
  float dy = center_y - y - 0.5f;
  float distance = sqrtf(square_sum(dx, dy));

  float distance_scale = distance * inv_max_distance;
  float eccentricity = 0.9 * distance_scale;

  float sigma_minor = 0.5f + distance_scale * 2.5f;
  float denom = 1.0f - eccentricity * eccentricity;
  float sigma_major = sigma_minor / sqrtf(denom);

  float angle = atan2f(dy, dx);
  float cos_angle = cosf(angle);
  float sin_angle = sinf(angle);

  float normalization_factor = 1.0f / (2.0f * static_cast<float>(M_PI) * sigma_major * sigma_minor);

  // ---- fixed-size arrays instead of vector ----
  float psf_s[PSF_SIZE * PSF_SIZE];   // PSF value
  int   psf_c[PSF_SIZE * PSF_SIZE];   // counts
  int   psf_len = 0;

  for (int i = min_x; i <= max_x; ++i) {
    for (int j = min_y; j <= max_y; ++j) {
      int c = count[i][j];
      if (c) {
        float delta_x = static_cast<float>(i - x);
        float delta_y = static_cast<float>(j - y);
        float major_coord = (delta_x * cos_angle + delta_y * sin_angle) / sigma_major;
        float minor_coord = (delta_x * sin_angle - delta_y * cos_angle) / sigma_minor;
        float exponent = (square_sum(major_coord, minor_coord)) * 0.5f;
        float psf_value = normalization_factor * expf(-exponent);

        psf_s[psf_len] = psf_value;
        psf_c[psf_len] = c;
        ++psf_len;
      }
    }
  }

  float R = 0.05f;

  for (int iter = 0; iter < ITERATION_COUNT; ++iter) {
    float tmp_R = 0.0f;
    for (int k = 0; k < psf_len; ++k) {
      float s = psf_s[k];
      float num = R * s;
      float denom_rs = (num + bkg_rate) * TIME;
      tmp_R += psf_c[k] * num / denom_rs;
    }
    R = tmp_R;
  }

  Rval[x][y] = R;

  float res = 0;
  for (int k = 0; k < psf_len; ++k) {
    float s = psf_s[k];
    float val = (R * s + bkg_rate) / bkg_rate;
    res += psf_c[k] * logf(val);
  }

  ratio[x][y] = res - TIME * R;
}

void calc_sum()
{
    for(int i = 0; i < LEN; i++) {
        for(int j = 0; j < LEN; j++) {
            sum_count[i][j] = count[i][j];
            if(i > 0) sum_count[i][j] += sum_count[i - 1][j];
            if(j > 0) sum_count[i][j] += sum_count[i][j - 1];
            if(i > 0 && j > 0) sum_count[i][j] -= sum_count[i - 1][j - 1];
        }
    }
}

void preworker(int id)
{
    constexpr int GX = LEN / STEP;
    constexpr int GY = LEN / STEP;
    constexpr int total = GX * GY;

    for (int idx = id; idx < total; idx += NUM_THREADS) {
        int x = idx / GY;
        int y = idx % GY;
        calc(x * STEP, y * STEP);
    }
}

void prework()
{
    static std::thread threads[NUM_THREADS];

    for(int i = 0; i < NUM_THREADS; i++) {
        threads[i] = std::thread(preworker, i);
        set_cpu_affinity(threads[i], i);
    }

    for(auto& th : threads) {
        th.join();
    }
}

struct LocalResult {
    std::vector<int> xs;
    std::vector<int> ys;
    std::vector<double> Rs;
};

void work()
{
  const int GX = LEN / STEP;
  const int GY = LEN / STEP;
  const int total = GX * GY;

  static std::thread threads[NUM_THREADS];
  LocalResult locals[NUM_THREADS];

  for (int tid = 0; tid < NUM_THREADS; ++tid) {
    threads[tid] = std::thread([tid, &locals, total, GX, GY]() {
        LocalResult &lr = locals[tid];

        for (int idx = tid; idx < total; idx += NUM_THREADS) {
            int grid_x = idx / GY;
            int grid_y = idx % GY;

            int X = grid_x * STEP;
            int Y = grid_y * STEP;

            // skip if coarse cell is not promising
            if (ratio[X][Y] < LIMIT) {
                continue;
            }

            int x = X;
            int y = Y;

            // move to local maximum along four directions
            while (x < LEN - 1) {
                calc(x + 1, y);
                if (ratio[x + 1][y] < ratio[x][y]) break;
                ++x;
            }
            while (x > 0) {
                calc(x - 1, y);
                if (ratio[x - 1][y] < ratio[x][y]) break;
                --x;
            }
            while (y < LEN - 1) {
                calc(x, y + 1);
                if (ratio[x][y + 1] < ratio[x][y]) break;
                ++y;
            }
            while (y > 0) {
                calc(x, y - 1);
                if (ratio[x][y - 1] < ratio[x][y]) break;
                --y;
            }

            if (ratio[x][y] >= threshold) {
                lr.xs.push_back(x);
                lr.ys.push_back(y);
                lr.Rs.push_back(Rval[x][y]);
            }
        }
    });

    set_cpu_affinity(threads[tid], tid);
  }

  for (auto &th : threads) {
    th.join();
  }

  // merge local results into global vectors
  for (int tid = 0; tid < NUM_THREADS; ++tid) {
    auto &lr = locals[tid];
    source_x.insert(source_x.end(), lr.xs.begin(), lr.xs.end());
    source_y.insert(source_y.end(), lr.ys.begin(), lr.ys.end());
    source_R.insert(source_R.end(), lr.Rs.begin(), lr.Rs.end());
  }
}

void read_data()
{
    fitsfile *fptr = nullptr;
    int status = 0;
    int hdutype = 0;
    long nrows = 0;
    int anynul = 0;

    const char* submit_dir = std::getenv("SLURM_SUBMIT_DIR");

    std::string mock_file = std::string(submit_dir) + "/data/mock_data.fits";
    fits_open_file(&fptr, mock_file.c_str(), READONLY, &status);
    

    // Python hdu[1] -> CFITSIO HDU
    fits_movabs_hdu(fptr, 2, &hdutype, &status);

    fits_get_num_rows(fptr, &nrows, &status);    

    std::vector<int> x(nrows), y(nrows);

    fits_read_col(fptr, TINT, 1, 1, 1, nrows, nullptr,
                    x.data(), &anynul, &status);
    
    fits_read_col(fptr, TINT, 2, 1, 1, nrows, nullptr,
                    y.data(), &anynul, &status);
    

    fits_close_file(fptr, &status);

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
    

    // primary HDU
    fits_create_img(fptr, 8, 0, nullptr, &status);  // BITPIX=8
    

    fits_write_comment(fptr,
                        const_cast<char *>("This file storages the info of detected sources"),
                        &status);
    

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
    

    fits_write_col(fptr, TINT, 1, 1, 1, nrows,
                    source_x.data(), &status);
    

    fits_write_col(fptr, TINT, 2, 1, 1, nrows,
                    source_y.data(), &status);
    

    fits_write_col(fptr, TDOUBLE, 3, 1, 1, nrows,
                    source_R.data(), &status);
    

    fits_close_file(fptr, &status);
    

}

int main()
{
    read_data();
    calc_sum();
    prework();
    work();
    write_result();
    return 0;
} 
