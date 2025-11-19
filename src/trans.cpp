#include <fitsio.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>

#define CHECK_STATUS(status)                         \
  do {                                               \
    if (status) {                                    \
      fits_report_error(stderr, status);             \
      std::exit(EXIT_FAILURE);                       \
    }                                                \
  } while (0)

struct Source {
  double x;
  double y;
  double R;
};

bool operator<(const Source &a, const Source &b) {
  if (a.x != b.x) return a.x < b.x;
  if (a.y != b.y) return a.y < b.y;
  return a.R < b.R;
}

// mock_data.fits -> mock_data.txt
void read() {
  fitsfile *fptr = nullptr;
  int status = 0;
  int hdutype = 0;
  long nrows = 0;
  int col_x = 0, col_y = 0;
  int anynul = 0;

  std::cout << "reading" << std::endl;

  fits_open_file(&fptr, "../data/mock_data.fits", READONLY, &status);
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

  std::vector<double> x(nrows), y(nrows);

  fits_read_col(fptr, TDOUBLE, col_x, 1, 1, nrows, nullptr,
                x.data(), &anynul, &status);
  CHECK_STATUS(status);
  fits_read_col(fptr, TDOUBLE, col_y, 1, 1, nrows, nullptr,
                y.data(), &anynul, &status);
  CHECK_STATUS(status);

  fits_close_file(fptr, &status);
  CHECK_STATUS(status);

  FILE *fp = std::fopen("../data/mock_data.txt", "w");
  if (!fp) {
    std::perror("fopen mock_data.txt");
    std::exit(EXIT_FAILURE);
  }

  // first line: length
  std::fprintf(fp, "%ld\n", nrows);
  for (long i = 0; i < nrows; i++) {
    // cast to integer-like if needed
    std::fprintf(fp, "%.0f %.0f\n", x[i], y[i]);
  }

  std::fclose(fp);

  std::cout << "reading done" << std::endl;
}

// source_info.fits -> sorted source_info.txt
void show() {
  fitsfile *fptr = nullptr;
  int status = 0;
  int hdutype = 0;
  long nrows = 0;
  int col_x = 0, col_y = 0, col_R = 0;
  int anynul = 0;

  std::cout << "showing" << std::endl;

  fits_open_file(&fptr, "../data/source_info.fits", READONLY, &status);
  CHECK_STATUS(status);

  fits_movabs_hdu(fptr, 2, &hdutype, &status);
  CHECK_STATUS(status);

  fits_get_num_rows(fptr, &nrows, &status);
  CHECK_STATUS(status);

  fits_get_colnum(fptr, CASEINSEN, const_cast<char *>("x"), &col_x, &status);
  CHECK_STATUS(status);
  fits_get_colnum(fptr, CASEINSEN, const_cast<char *>("y"), &col_y, &status);
  CHECK_STATUS(status);
  fits_get_colnum(fptr, CASEINSEN, const_cast<char *>("countrate"), &col_R, &status);
  CHECK_STATUS(status);

  std::vector<double> vx(nrows), vy(nrows), vR(nrows);

  fits_read_col(fptr, TDOUBLE, col_x, 1, 1, nrows, nullptr,
                vx.data(), &anynul, &status);
  CHECK_STATUS(status);
  fits_read_col(fptr, TDOUBLE, col_y, 1, 1, nrows, nullptr,
                vy.data(), &anynul, &status);
  CHECK_STATUS(status);
  fits_read_col(fptr, TDOUBLE, col_R, 1, 1, nrows, nullptr,
                vR.data(), &anynul, &status);
  CHECK_STATUS(status);

  fits_close_file(fptr, &status);
  CHECK_STATUS(status);

  std::vector<Source> arr(nrows);
  for (long i = 0; i < nrows; i++) {
    arr[i].x = vx[i];
    arr[i].y = vy[i];
    arr[i].R = vR[i];
  }

  std::sort(arr.begin(), arr.end());

  FILE *fp = std::fopen("../data/source_info.txt", "w");
  if (!fp) {
    std::perror("fopen source_info.txt");
    std::exit(EXIT_FAILURE);
  }

  for (long i = 0; i < nrows; i++) {
    std::fprintf(fp, "%.0f %.0f %.6f\n", arr[i].x, arr[i].y, arr[i].R);
  }

  std::fclose(fp);

  std::cout << "showing done" << std::endl;
}

// detection_info.txt -> detection_info.fits
void write() {
  std::cout << "writing" << std::endl;

  FILE *fp = std::fopen("../data/detection_info.txt", "r");
  if (!fp) {
    std::perror("fopen detection_info.txt");
    std::exit(EXIT_FAILURE);
  }

  // first pass: count lines
  char buf[512];
  long nrows = 0;
  while (std::fgets(buf, sizeof(buf), fp)) {
    if (buf[0] == '\n' || buf[0] == '\0') continue;
    nrows++;
  }
  std::fclose(fp);

  if (nrows == 0) {
    std::cerr << "no data in detection_info.txt\n";
    return;
  }

  std::vector<double> xx(nrows), yy(nrows), RR(nrows);

  fp = std::fopen("../data/detection_info.txt", "r");
  if (!fp) {
    std::perror("fopen detection_info.txt");
    std::exit(EXIT_FAILURE);
  }

  long idx = 0;
  while (idx < nrows && std::fgets(buf, sizeof(buf), fp)) {
    double x, y, R;
    if (std::sscanf(buf, "%lf %lf %lf", &x, &y, &R) == 3) {
      xx[idx] = x;
      yy[idx] = y;
      RR[idx] = R;
      idx++;
    }
  }
  std::fclose(fp);

  if (idx != nrows) {
    std::cerr << "warning: line count mismatch, read "
              << idx << " vs " << nrows << "\n";
    nrows = idx;
  }

  fitsfile *fptr = nullptr;
  int status = 0;

  // "!" to overwrite existing file
  fits_create_file(&fptr, "!detection_info.fits", &status);
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

  fits_create_tbl(fptr, BINARY_TBL, nrows, 3,
                  ttype, tform, tunit,
                  const_cast<char *>("DETECTION_INFO"), &status);
  CHECK_STATUS(status);

  // prepare integer arrays for x,y
  std::vector<unsigned short> xI(nrows), yI(nrows);
  for (long i = 0; i < nrows; i++) {
    long xi = static_cast<long>(xx[i] + 0.5); // round to nearest
    long yi = static_cast<long>(yy[i] + 0.5);
    if (xi < 0) xi = 0;
    if (xi > 65535) xi = 65535;
    if (yi < 0) yi = 0;
    if (yi > 65535) yi = 65535;
    xI[i] = static_cast<unsigned short>(xi);
    yI[i] = static_cast<unsigned short>(yi);
  }

  fits_write_col(fptr, TUSHORT, 1, 1, 1, nrows,
                 xI.data(), &status);
  CHECK_STATUS(status);

  fits_write_col(fptr, TUSHORT, 2, 1, 1, nrows,
                 yI.data(), &status);
  CHECK_STATUS(status);

  fits_write_col(fptr, TDOUBLE, 3, 1, 1, nrows,
                 RR.data(), &status);
  CHECK_STATUS(status);

  fits_close_file(fptr, &status);
  CHECK_STATUS(status);

  std::cout << "writing done" << std::endl;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::fprintf(stderr, "Usage: %s [read|show|write]\n", argv[0]);
    return EXIT_FAILURE;
  }

  std::string mode = argv[1];
  if (mode == "read") {
    read();
  } else if (mode == "show") {
    show();
  } else if (mode == "write") {
    write();
  } else {
    std::fprintf(stderr, "unknown argv[1]: %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}