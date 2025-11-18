#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <tuple>
#include <cmath>

constexpr int LEN = 512;
constexpr int PSF_SIZE = 15;

constexpr float bkg_rate = 1e-5;
constexpr int TIME = 1000;

std::vector<float> data_x;
std::vector<float> data_y;
int data_size;
float ratio[LEN][LEN];
float Rval[LEN][LEN];
std::vector<float> PSF_list;
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
        std::cerr << "Processing " << x << std::endl;
        for(int y = 0; y < LEN; y++) {
            PSF_list.clear();

            for(int i = 0; i < data_size; i++) {
                float delta_x = data_x[i] - x;
                float delta_y = data_y[i] - y;
                if(abs(delta_x) <= (PSF_SIZE - 1) / 2 && abs(delta_y) <= (PSF_SIZE - 1) / 2) {
                    PSF_list.push_back(PSF_frac_calc(x, y, delta_x, delta_y));
                }
            }

            float R = 0.05;

            for(int iter = 0; iter < ITERATION_COUNT; iter++) {
                float tmp_R = 0;
                for(float s : PSF_list) {
                    tmp_R += (R * s) / ((R * s + bkg_rate) * TIME);
                }
                R = tmp_R;
            }

            Rval[x][y] = R;
            
            float res = 0;
            for(float s : PSF_list) {
                res += log((R * s + bkg_rate) / bkg_rate);
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
    std::ifstream fin("mock_data.txt");
    std::ofstream fout("detection_info.txt");

    fin >> data_size;

    data_x.resize(data_size);
    data_y.resize(data_size);

    for(int i = 0; i < data_size; i++) {
        fin >> data_x[i] >> data_y[i];
    }

    work();
    detection();

    fout << std::setprecision(6) << std::fixed;
    for(auto [x, y, R] : source_list) {
        fout << x << " " << y << " " << R << std::endl;
    }

    return 0;
}