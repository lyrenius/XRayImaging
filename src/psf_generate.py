import numpy as np
import matplotlib.pyplot as plt
from scipy import integrate
import math

def generate_elliptical_gaussian(x, y, grid_size=512, output_size=15):

    '''
    This function generates the PSF used in the mock data.
    Note that this version uses integrated value for each pixel (same for the generation of mock data), 
    but the function in the main code uses center point value.

    input: position of pixel (x,y)
    output: 15*15 grid of distribution probability for a photon from the source (nominal location is center of this grid)
    '''

    center_x, center_y = 256, 256
    
    # calculate distance of pixel (x,y) relative to center
    dx = center_x - x - 0.5
    dy = center_y - y - 0.5
    distance = math.sqrt(dx**2 + dy**2)
    
    # calculate eccentricity
    max_distance = 724.077  #512*sqrt(2)
    eccentricity = 0.9 * (distance / max_distance)
    
    # calculate sigma on major and minor axis
    sigma_minor = 0.5 + (distance / max_distance) * 2.5
    sigma_major = sigma_minor / math.sqrt(1 - eccentricity**2)

    # generate output grid
    output_grid = np.zeros((output_size, output_size))
    half_size = output_size // 2
    
    # calculate rotation angle of gaussian profile
    angle = math.atan2(dy, dx) if (dx != 0 or dy != 0) else 0
    cos_angle = math.cos(angle)
    sin_angle = math.sin(angle)
    
    # define gaussian function after rotation
    def rotated_gaussian(x, y):
        x_rot = x * cos_angle + y * sin_angle
        y_rot = -x * sin_angle + y * cos_angle
        exponent = (((x * cos_angle + y * sin_angle) / sigma_major) ** 2 + ((-x * sin_angle + y * cos_angle) / sigma_minor) ** 2) / 2
        return math.exp(-exponent)
    
    # narmalization
    normalization_constant = 1 / (2 * math.pi * sigma_major * sigma_minor)
    
    # integration
    for i in range(output_size):
        for j in range(output_size):
            x_min = i - half_size - 0.5
            x_max = i - half_size + 0.5
            y_min = j - half_size - 0.5
            y_max = j - half_size + 0.5
            integral, error = integrate.dblquad(
                rotated_gaussian, 
                x_min, x_max, 
                lambda x: y_min, lambda x: y_max
            )
            output_grid[i][j] = integral * normalization_constant
    return output_grid

if __name__ == '__main__':
    # visualize
    test_points = [(64,64),(128,128),(256,256),(64,256),(512,512)]
    for x,y in test_points:
        print(f"PSF at position ({x}, {y}):")
        grid = generate_elliptical_gaussian(x,y)
        fig = plt.figure(figsize=(10, 8))
        ax = fig.add_subplot(111)
        im=ax.imshow(grid, cmap='viridis', origin='lower',vmax=0.9 * np.max(grid))
        cbar = fig.colorbar(im, ax=ax, orientation='vertical')
        plt.show()
        plt.close()