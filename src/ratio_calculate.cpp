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

int count[LEN][LEN];
float ratio[LEN][LEN];
float Rval[LEN][LEN];
std::vector<std::pair<float,int>> PSF_list;
std::vector<std::tuple<int,int,float>> source_list;

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
                    source_list.emplace_back(x, y, Rval[x][y]);
                }
            }
        }
    }
}

int main()
{
    const char* submit_dir = std::getenv("SLURM_SUBMIT_DIR");
    std::ifstream fin(std::string(submit_dir) + "/data/mock_data.txt");
    std::ofstream fout(std::string(submit_dir) + "/data/detection_info.txt");

    int data_size;
    fin >> data_size;

    for(int i = 0; i < data_size; i++) {
        int x, y;
        fin >> x >> y;
        count[x][y]++;
    }

    work();
    detection();

    fout << std::setprecision(6) << std::fixed;
    for(auto [x, y, R] : source_list) {
        fout << x << " " << y << " " << R << std::endl;
    }

    return 0;
}