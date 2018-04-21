import time
import datetime
import os
import traceback
from luma.core.interface.serial import i2c
from luma.core.render import canvas
from luma.oled.device import ssd1306
import argparse


class OdroidOled(object):
    def __init__(self,
                 port=1,
                 address=0x3c,
                 image_dir="/home/odroid/Pictures/",
                 update_hz=1,
                 dev_path="/dev/ttyACM0/"):


        self.serial = i2c(port=port, address=address)
        self.device = ssd1306(self.serial)
        self.image_dir = image_dir
        self.update_hz = update_hz
        self.dev_path = dev_path

    def current_time(self):
        return datetime.datetime.now().time().strftime("%H:%M:%S")


    def image_count(self):
        camera_dirs = [os.path.join(self.image_dir, fname)
                       for fname in os.listdir(self.image_dir)
                       if os.path.isdir(os.path.join(self.image_dir, fname))]

        tiff_counts = []
        raw_counts = []

        for camera_dir in camera_dirs:
            tiff_counts.append(len([f for f in os.listdir(camera_dir) if ".tiff" in f]))
            raw_counts.append(len([f for f in os.listdir(camera_dir) if ".raw" in f]))

        return tiff_counts, raw_counts 

    def run(self):
        while True:

            try:
                current_time = self.current_time()
                tiff_counts, raw_counts = self.image_count()
    
                with canvas(self.device) as draw:
                    draw.rectangle(self.device.bounding_box, outline="white")
                    draw.text((3, 3), "NITELITE Monitor", fill="white")
                    draw.text((3, 15), "Last: +{} GMT".format(current_time), fill="white")
                    draw.text((3, 25), "TIFF: {}".format(tiff_counts), fill="white")
                    draw.text((3, 35), "RAW: {}".format(raw_counts), fill="white")

            except IOError as e:
                continue 


        time.sleep(self.update_hz)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--image-dir", default="/home/odroid/Pictures/",
                        help="directory to scan for Basler image output")

    kwargs = vars(parser.parse_args())
   
    OdroidOled(**kwargs).run() 


if __name__ == "__main__":
    main()
