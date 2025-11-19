import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits
import math

def PSF_frac_calc(x, y, delta_x, delta_y, grid_size=512, output_size=15):

    '''
    This function generates PSF value for pixel located at (x + delta_x, y + delta_y), with nominal source located at (x,y)

    input: position of nominal source (x, y), relative position of a pixel (deltax, deltay)
    output: probability value
    '''

    center_x, center_y = 256, 256

    # calculate distance of pixel (x,y) relative to center
    dx = center_x - x - 0.5
    dy = center_y - y - 0.5
    distance = math.sqrt(dx**2 + dy**2)

    # calculate eccentricity
    max_distance = 362.039  #256*sqrt(2)
    eccentricity = 0.9 * (distance / max_distance)
    
    # calculate sigma on major and minor axis
    sigma_minor = 0.5 + (distance / max_distance) * 2.5
    sigma_major = sigma_minor / math.sqrt(1 - eccentricity**2)
    
    # calculate rotation angle of gaussian profile PSF
    angle = math.atan2(dy, dx) if (dx != 0 or dy != 0) else 0
    cos_angle = math.cos(angle)
    sin_angle = math.sin(angle)
    
    # normalization factor
    normalization_constant = 1 / (2 * math.pi * sigma_major * sigma_minor)
    
    # psf calculation
    exponent = (((delta_x * cos_angle + delta_y * sin_angle) / sigma_major) ** 2 + \
                ((-delta_x * sin_angle + delta_y * cos_angle) / sigma_minor) ** 2) / 2
    value = math.exp(-exponent)*normalization_constant

    return value

def ratio_map_calculation(data,bkg_rate=10**-5,t=1000):

    '''
    This function generates the distribution of the probability ratio and the source count rate

    input: original data
    output: probability ratio grid, source count rate grid
    '''

    data_size_x = 512
    data_size_y = 512
    psf_size = 15
    ratio_grid = np.zeros((data_size_x,data_size_y))
    R_grid = np.zeros((data_size_x,data_size_y))
    iteration = 5 # times for iteration in calculating R

    # calculate in every pixel (x,y)
    for x in range(0,data_size_x):
        print(x)
        for y in range(0,data_size_y):

            # generate list to storage events within PSF range
            temp_data_list=[] 
            for n in range(0,len(data)):
                delta_x = data[n][0] - x # distance between event and pixel (x,y)
                delta_y = data[n][1] - y
                if abs(delta_x) <= (psf_size - 1) / 2 and abs(delta_y) <= (psf_size - 1) / 2:
                    psf_frac = PSF_frac_calc(x,y,delta_x,delta_y)
                    s = psf_frac
                    b = bkg_rate
                    temp_data_list.append([data[n][0],data[n][1],s,b])

            # calculate R to maximize Delta ln(L)
            R = 0.05 # initial value for R 
            temp_R = 0
            for _ in range(0,iteration):
                for n in range(0,len(temp_data_list)):
                    s = temp_data_list[n][2]
                    b = temp_data_list[n][3]
                    temp_R = temp_R + (R * s) / ((R * s + b) * t)
                R = temp_R
                temp_R = 0

            # calculate final probability ratio
            for n in range(0,len(temp_data_list)):
                s = temp_data_list[n][2]
                b = temp_data_list[n][3]
                ratio_grid[x][y] += np.log((R * s + b) / b)
            ratio_grid[x][y] += -t * R
            R_grid[x][y] = R

    return ratio_grid, R_grid

def detection(ratio_grid,R_grid):

    '''
    This function detects sources by ratio value

    input: probability ratio grid, source count rate grid 
    output: list of source coordinate and count rate
    '''

    threshold = 8
    source_coord_list=[]
    for x in range(10,500):
        for y in range(10,500):
            if ratio_grid[x][y] >= threshold:
                if ratio_grid[x][y] == np.max(ratio_grid[x-10:x+10,y-10:y+10]):
                    source_coord_list.append([x,y,R_grid[x][y]])

    return source_coord_list



if __name__ == '__main__':
    # read data
    hdu = fits.open('mock_data.fits')
    x_coord = hdu[1].data['x']
    y_coord = hdu[1].data['y']
    data = np.column_stack((x_coord,y_coord))
    
    # calculate Delta ln(L) map
    ratio_grid, R_grid = ratio_map_calculation(data)

    # detection
    source_coord_list = detection(ratio_grid,R_grid)
    print(source_coord_list)

    # storage detected position
    source_coord_array = np.array(source_coord_list)
    col1 = fits.Column(name='x', format='I', array=source_coord_array[:, 0]) 
    col2 = fits.Column(name='y', format='I', array=source_coord_array[:, 1])
    col3 = fits.Column(name='countrate', format='D', array=source_coord_array[:, 2])
    cols = fits.ColDefs([col1, col2, col3])
    tbhdu = fits.BinTableHDU.from_columns(cols)
    prihdr = fits.Header()
    prihdr['COMMENT'] = "This file storages the info of detected sources"
    prihdu = fits.PrimaryHDU(header=prihdr)
    hdulist = fits.HDUList([prihdu, tbhdu])
    hdulist.writeto('detection_info.fits', overwrite=True)