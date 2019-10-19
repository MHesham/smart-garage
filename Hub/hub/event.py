#!/usr/bin/env python

from collections import deque
import paho.mqtt.client as mqtt
import json
import logging

log = logging.getLogger(__name__)


class Event(object):
    def __init__(self, name: str, value: dict = {}):
        assert isinstance(value, dict)
        self.name = name
        self.value = value


class EventManager:
    def __init__(self, mqttc: mqtt.Client):
        self.registry = {}
        self.q = deque()
        self.mqttc = mqttc
        self.mqttc.on_message = self.__on_message
        self.mqttc.on_connect = self.__on_connect
        self.mqttc.on_publish = self.__on_publish
        self.mqttc.on_subscribe = self.__on_subscribe
        self.mqttc.on_log = self.__on_log

    def subscribe(self, evt_name: str, callback):
        if evt_name not in self.registry:
            self.registry[evt_name] = []
            self.mqttc.subscribe(evt_name)
        self.registry[evt_name].append(callback)

    def publish(self, evt: Event):
        value_str = json.dumps(evt.value)
        log.debug('[IN]:' + evt.name + ' ' + value_str)
        infot = self.mqttc.publish(evt.name, value_str, qos=0)
        infot.wait_for_publish()

    def execute(self):
        for evt in self.q:
            log.debug('[OUT]:' + evt.name + ' ' + str(evt.value))
            if evt.name in self.registry:
                for callback in self.registry[evt.name]:
                    callback(evt)
        self.q.clear()

    def __on_connect(self, mqttc, obj, flags, rc):
        log.info("Connect: " + str(rc))

    def __on_message(self, client, userdata, message: mqtt.MQTTMessage):
        value_str = message.payload.decode('ascii')
        log.info("Message: " + message.topic + " " +
                 str(message.qos) + " " + value_str)
        value_doc = json.loads(value_str)
        log.debug('[IN]:' + message.topic + ' ' + str(value_doc))
        self.q.append(Event(message.topic, value_doc))

    def __on_publish(self, mqttc, obj, mid):
        log.info("Publish: " + str(mid))

    def __on_subscribe(self, mqttc, obj, mid, granted_qos):
        log.info("Subscribe: " + str(mid) + " " + str(granted_qos))

    def __on_log(self, mqttc, obj, level, string):
        log.debug("Log: " + string)
