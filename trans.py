import sys
import numpy as np
from astropy.io import fits

if __name__ == '__main__':
    
    if sys.argv[1] == "read":
        print("reading")
        # memmap to reduce memory copy
        hdu = fits.open('mock_data.fits', memmap=True)
        data = hdu[1].data
        x_coord = data['x']
        y_coord = data['y']
        
        n = len(x_coord)
        arr = np.column_stack((x_coord, y_coord))
        
        # write all in one go
        with open('mock_data.txt', 'w') as f:
            f.write(f"{n}\n")
            np.savetxt(f, arr, fmt="%d %d")
        print("reading done")
    
    elif sys.argv[1] == "show":
        print("showing")
        hdu = fits.open("source_info.fits", memmap=True)
        data = hdu[1].data
        res_x = data['x'].astype(float)
        res_y = data['y'].astype(float)
        res_R = data['countrate'].astype(float)
        
        # sorted(zip(x, y, R))
        idx = np.lexsort((res_R, res_y, res_x))
        sx = res_x[idx]
        sy = res_y[idx]
        sR = res_R[idx]
        
        arr = np.column_stack((sx, sy, sR))
        with open("source_info.txt", "w") as f:
            np.savetxt(f, arr, fmt="%.0f %.0f %.6f")
        print("showing done")
        
    elif sys.argv[1] == "write":
        print("writing")
        # load all data at once
        data = np.loadtxt("detection_info.txt")
        xx = data[:, 0]
        yy = data[:, 1]
        RR = data[:, 2]
        
        # if x,y are integer in fact, cast to int to match format='I'
        xx_int = xx.astype('int64')
        yy_int = yy.astype('int64')
        
        col1 = fits.Column(name='x', format='I', array=xx_int)
        col2 = fits.Column(name='y', format='I', array=yy_int)
        col3 = fits.Column(name='countrate', format='D', array=RR)
        cols = fits.ColDefs([col1, col2, col3])
        tbhdu = fits.BinTableHDU.from_columns(cols)
        prihdr = fits.Header()
        prihdr['COMMENT'] = "This file storages the info of detected sources"
        prihdu = fits.PrimaryHDU(header=prihdr)
        hdulist = fits.HDUList([prihdu, tbhdu])
        hdulist.writeto('detection_info.fits', overwrite=True)
        print("writing done")
    
    else:
        print("unknown argv[1]")