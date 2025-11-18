import sys
import matplotlib.pyplot as plt
from astropy.io import fits

if __name__ == '__main__':
    
    if sys.argv[1] == "read":
        print("reading")
        # read data
        hdu = fits.open('mock_data.fits')
        x_coord = hdu[1].data['x']
        y_coord = hdu[1].data['y']
        with open('mock_data.txt', 'w') as f:
            print(len(x_coord), file=f)
            for x, y in zip(x_coord, y_coord):
                print(x, y, file=f)
        print("reading done")
    
    elif sys.argv[1] == "show":
        print("showing")
        hdu = fits.open("source_info.fits")
        res_x = hdu[1].data['x']
        res_y = hdu[1].data['y']
        res_R = hdu[1].data['countrate']
        with open("source_info.txt", "w") as f:
            for x, y, R in sorted(list(zip(res_x, res_y, res_R))):
                print(f"{x} {y} {R:.6f}", file=f)
        print("showing done")
        
    elif sys.argv[1] == "write":
        print("writing")
        # storage detected position
        xx = []
        yy = []
        RR = []
        with open("detection_info.txt", "r") as f:
            for line in f:
                x, y, R = tuple(map(float, line.strip().split()))
                xx.append(x)
                yy.append(y)
                RR.append(R)
                
        col1 = fits.Column(name='x', format='I', array=xx) 
        col2 = fits.Column(name='y', format='I', array=yy)
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