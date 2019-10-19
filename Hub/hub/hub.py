#!/usr/bin/env python

import paho.mqtt.client as mqtt
import ssl
import argparse
from device import LedStrip, MotionSensor
from event import EventManager, Event
import logging

parser = argparse.ArgumentParser()

parser.add_argument('-H', '--host', required=True, default=None)
parser.add_argument('-u', '--username', required=True, default=None)
parser.add_argument('-p', '--password', required=True, default=None)
parser.add_argument('-F', '--cacerts', required=True, default=None)
parser.add_argument('-D', '--debug', action='store_true')
args, unknown = parser.parse_known_args()

log = logging.getLogger(__name__)


class Hub:
    CLIENT_ID = 'hub0'

    def __init__(self):
        self.port = 8883
        self.mqttc = mqtt.Client(self.CLIENT_ID, clean_session=True)
        self.mqttc.tls_set(ca_certs=args.cacerts,
                           tls_version=ssl.PROTOCOL_TLSv1_1)
        self.mqttc.username_pw_set(args.username, args.password)

        self.evt_mgr = EventManager(self.mqttc)

        log.info("Connecting to "+args.host+" port: "+str(self.port))
        self.mqttc.connect(args.host, self.port)

        self.evt_mgr.subscribe(
            MotionSensor.TOPIC_MOTION_ACTIVE_CHANGED,
            self.__on_motion_active_changed)
        self.light = LedStrip(self.evt_mgr)
        self.motion = MotionSensor(self.evt_mgr)

    def __on_motion_active_changed(self, evt: Event):
        log.debug('__on_motion_active_changed' + str(evt.__dict__))
        if self.motion.is_active:
            self.light.power_on()
        else:
            self.light.power_off()

    def spin(self):
        while True:
            self.mqttc.loop()
            self.evt_mgr.execute()
