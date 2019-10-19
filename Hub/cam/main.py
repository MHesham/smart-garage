#!/usr/bin/env python

import sys
import logging
import cv2 as cv
import numpy as np
from pupil_apriltags import Detector

root = logging.getLogger()
root.setLevel(logging.DEBUG)

handler = logging.StreamHandler(sys.stdout)
handler.setLevel(logging.DEBUG)
formatter = logging.Formatter(
    '%(asctime)s - %(name)s - %(levelname)s - %(message)s')
handler.setFormatter(formatter)
root.addHandler(handler)
log = logging.getLogger(__name__)


class AprilCam:
    TAG_SIZE_METER = 0.127

    def __init__(self):
        self.cap = cv.VideoCapture(0)
        # set the resolution
        self.cap.set(cv.CAP_PROP_FRAME_WIDTH, 640)
        self.cap.set(cv.CAP_PROP_FRAME_HEIGHT, 480)
        # turn the autofocus off
        self.cap.set(cv.CAP_PROP_AUTOFOCUS, 0)
        self.at_detector = Detector(families='tagStandard41h12')
        self.dist_coeffs = np.loadtxt('./dist_coeffs.txt', delimiter=',')
        self.camera_matrix = np.loadtxt('./camera_matrix.txt', delimiter=',')
        self.camera_params = [self.camera_matrix[0, 0],
                              self.camera_matrix[1, 1],
                              self.camera_matrix[0, 2],
                              self.camera_matrix[1, 2]]
        print(self.camera_params)

    def spin(self):
        while (True):
            ret, frame = self.cap.read()
            gray = cv.cvtColor(frame, cv.COLOR_BGR2GRAY)
            if cv.waitKey(1) & 0xFF == ord('q'):
                break
            tags = self.at_detector.detect(
                gray, estimate_tag_pose=True, camera_params=self.camera_params, tag_size=self.TAG_SIZE_METER)
            if len(tags) > 0:
                #print(tags)
                for tag in tags:
                    color = (255, 0, 0)
                    c = tag.corners.astype(int)
                    cv.polylines(gray, [c], True, color, 2)
            cv.imshow('frame', gray)
        self.cap.release()
        cv.destroyAllWindows()


def main():
    cam = AprilCam()
    cam.spin()


if __name__ == '__main__':
    main()
