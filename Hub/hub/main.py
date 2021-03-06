#!/usr/bin/env python

import sys
from hub import Hub
import logging

root = logging.getLogger()
root.setLevel(logging.DEBUG)

handler = logging.StreamHandler(sys.stdout)
handler.setLevel(logging.DEBUG)
formatter = logging.Formatter(
    '%(asctime)s - %(name)s - %(levelname)s - %(message)s')
handler.setFormatter(formatter)
root.addHandler(handler)


def main():
    root.info(sys.version)
    hub = Hub()
    hub.spin()


if __name__ == '__main__':
    main()
