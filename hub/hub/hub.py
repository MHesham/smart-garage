#!/usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (c) 2017 Jon Levell <levell@uk.ibm.com>
#
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Distribution License v1.0
# which accompanies this distribution.
#
# The Eclipse Distribution License is available at
#   http://www.eclipse.org/org/documents/edl-v10.php.
#
# All rights reserved.

# This shows a example of an MQTT publisher with the ability to use
# user name, password CA certificates based on command line arguments

import paho.mqtt.client as mqtt
import os
import ssl
import argparse
import time

parser = argparse.ArgumentParser()

parser.add_argument('-H', '--host', required=True, default=None)
parser.add_argument('-u', '--username', required=True, default=None)
parser.add_argument('-p', '--password', required=True, default=None)
parser.add_argument('-F', '--cacerts', required=True, default=None)
parser.add_argument('-D', '--debug', action='store_true')
args, unknown = parser.parse_known_args()

TOPIC_GARAGE_DOOR_ON = '/garage/door/on'
TOPIC_GARAGE_DOOR_OFF = '/garage/door/off'


def on_connect(mqttc, obj, flags, rc):
    print("connect rc: " + str(rc))


def on_message(mqttc, obj, msg):
    print(msg.topic + " " + str(msg.qos) + " " + str(msg.payload))


def on_publish(mqttc, obj, mid):
    print("mid: " + str(mid))


def on_subscribe(mqttc, obj, mid, granted_qos):
    print("Subscribed: " + str(mid) + " " + str(granted_qos))


def on_log(mqttc, obj, level, string):
    print(string)


def main():
    port = 8883
    clientid = 'hub0'
    mqttc = mqtt.Client(clientid, clean_session=True)
    mqttc.tls_set(ca_certs=args.cacerts, tls_version=ssl.PROTOCOL_TLSv1_1)
    mqttc.username_pw_set(args.username, args.password)

    mqttc.on_message = on_message
    mqttc.on_connect = on_connect
    mqttc.on_publish = on_publish
    mqttc.on_subscribe = on_subscribe

    if args.debug:
        mqttc.on_log = on_log

    print("Connecting to "+args.host+" port: "+str(port))
    mqttc.connect(args.host, port)

    mqttc.loop_start()

    infot = mqttc.publish(TOPIC_GARAGE_DOOR_ON, None, qos=0)
    infot.wait_for_publish()
    time.sleep(1)
    infot = mqttc.publish(TOPIC_GARAGE_DOOR_OFF, None, qos=0)
    infot.wait_for_publish()

    mqttc.disconnect()


if __name__ == '__main__':
    main()
